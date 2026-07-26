#ifndef ENTITIES_H
#define ENTITIES_H
// ============================================================
//  ENTITIES.H — everything that MOVES or TICKS besides the player:
//
//    machines     placed tiles with per-instance state (chests,
//                 drills, conveyors, inserters, turrets, spawners)
//    mobs         the natives; they multiply and raid (Factorio-ish)
//    bots         autonomous mining drones you craft
//    projectiles  bullets and slingshot stones (yours + turrets')
//    bombs        timed explosives
//    effects      explosion rings, laser beams
//
//  Owns: all the pools above + game time / evolution / raid state.
//  Sits ABOVE world.h and player.h (it moves mobs through the world
//  and hurts the player) but BELOW ui.h and main.c.
//
//  GAME PATTERN — everything here is an object pool (fixed array +
//  `active` flags), same as projectiles always were: no malloc, no
//  leaks, trivially saveable with one fwrite per array.
// ============================================================

#include <stdio.h>
#include <string.h>    // memset — pool wipes
#include "raylib.h"
#include "raymath.h"
#include "config.h"
#include "gamedata.h"
#include "world.h"
#include "player.h"

// Direction tables: 0=East 1=South 2=West 3=North.
static const int DIR_DX[4] = { 1, 0, -1, 0 };
static const int DIR_DY[4] = { 0, 1, 0, -1 };

// ─── Machines ─────────────────────────────────────────────
// One Machine = the per-instance STATE of one special tile (the
// tile grid itself only knows the type). `slots` is its inventory:
// a chest shows all 49 as a 7x7 grid; belts and inserters only ever
// use slot 0 (the one item they're carrying).
#define MACHINE_SLOTS 49
// Belts are cheap and get spammed by the hundred, so the pool has
// to be generous — running out used to leave tiles with no machine
// record, which rendered as a dead east-facing belt.
#define MAX_MACHINES  8192
typedef struct {
    bool     active;
    int      x, y;              // which tile this state belongs to
    TileType type;
    int      dir;               // conveyor/inserter facing (see DIR_DX)
    float    timer;             // generic cooldown (spawner/drill/turret...)
    ItemID   slots[MACHINE_SLOTS];
    int      counts[MACHINE_SLOTS];
    int      ammo;              // gun turrets: loaded bullets
    int      coal;              // fuel-burners: coal in the hopper
    float    fuel;              // ...and seconds left on the current lump
    float    beltProgress;      // 0..1 slide of the item along a belt
    Vector2  beamTo;            // laser turrets: where the last zap went
    float    beamTtl;           // ...and how long to keep drawing it
} Machine;
static Machine machines[MAX_MACHINES];

// Which machine's UI panel is open? -1 = none. Lives here because
// both main.c (input) and ui.h (drawing) need it.
static int machineUiX = -1, machineUiY = -1;

// ─── Cross-panel drag ─────────────────────────────────────
// One drag state shared by every slot surface (backpack, hotbar,
// machine panels), so a stack can be dragged from any of them to
// any other. The item/count are cached here purely so the floating
// ghost is trivial to draw.
enum { DRAG_NONE = 0, DRAG_PLAYER, DRAG_MACHINE };
static int    uiDragKind  = DRAG_NONE;
static int    uiDragIndex = -1;
static ItemID uiDragItem  = ITEM_NONE;
static int    uiDragCount = 0;

static void UiDragClear(void) {
    uiDragKind = DRAG_NONE;
    uiDragIndex = -1;
    uiDragItem = ITEM_NONE;
    uiDragCount = 0;
}

// O(1) "which machine is on tile x,y" lookup: a grid of indices
// into machines[] (-1 = none). 128KB well spent — conveyor updates
// query their neighbors every tick.
static short machineIndex[WORLD_SIZE][WORLD_SIZE];

// ─── Mobs ─────────────────────────────────────────────────
#define MAX_MOBS 96
enum { MOB_IDLE = 0, MOB_RAID };
typedef struct {
    bool    active;
    Vector2 pos;
    Vector2 home;        // the spawner that made it
    Vector2 target;      // where it's walking
    float   hp, maxHp;
    int     state;
    float   retarget;    // seconds until it picks a new destination
    float   rage;        // >0 = enraged: faster, and it hunts YOU
} Mob;
static Mob mobs[MAX_MOBS];

// ─── Mining bots ──────────────────────────────────────────
#define MAX_BOTS 12
enum { BOT_FIND = 0, BOT_TO_TARGET, BOT_MINING, BOT_TO_DROPOFF };
typedef struct {
    bool    active;
    Vector2 pos;
    int     state;
    int     tx, ty;              // tile being mined
    int     inv[ITEM_COUNT];     // what it's carrying
    int     carrying;            // total item count (cached)
} Bot;
static Bot bots[MAX_BOTS];

// ─── Projectiles / bombs / effects ────────────────────────
#define MAX_PROJECTILES 96
typedef struct {
    bool     active;
    Vector2  pos, dir;
    float    speed, traveled, maxRange;
    ItemID   type;          // ITEM_BULLET or ITEM_SMALL_STONE
    bool     ignoreTiles;   // turret fire arcs OVER walls (no friendly fire)
} Projectile;
static Projectile projectiles[MAX_PROJECTILES];

#define MAX_BOMBS 8
typedef struct { bool active; Vector2 pos; float fuse; } Bomb;
static Bomb bombs[MAX_BOMBS];

// Effects come in flavors now: explosion rings, muzzle flashes,
// and little impact sparks — the seasoning that makes shooting
// feel like SHOOTING.
enum { EFFECT_RING = 0, EFFECT_FLASH, EFFECT_SPARK };
#define MAX_EFFECTS 48
typedef struct { bool active; int kind; Vector2 pos; float age, life, maxRadius; } Effect;
static Effect effects[MAX_EFFECTS];

#define MAX_BEAMS 16
typedef struct { bool active; Vector2 from, to; float ttl; } Beam;
static Beam beams[MAX_BEAMS];

// Screen shake: anything violent adds to this; main.c jitters the
// camera by it and it decays fast. Small numbers, big feel.
static float entShake = 0;

// ─── Clock & evolution ────────────────────────────────────
static float entGameTime = 0;    // total seconds played this world

// Factorio-style escalation: 0 at the start, 1 after
// TUNE.evolutionMinutes of play. Everything mob scales off this.
static float EvolutionFactor(void) {
    float evo = entGameTime / (TUNE.evolutionMinutes * 60.0f);
    if (evo < 0) evo = 0;
    if (evo > 1) evo = 1;
    return evo;
}

// ─── Pool plumbing ────────────────────────────────────────
static void EntitiesReset(void) {
    memset(machines, 0, sizeof(machines));
    memset(mobs, 0, sizeof(mobs));
    memset(bots, 0, sizeof(bots));
    memset(projectiles, 0, sizeof(projectiles));
    memset(bombs, 0, sizeof(bombs));
    memset(effects, 0, sizeof(effects));
    memset(beams, 0, sizeof(beams));
    for (int x = 0; x < WORLD_SIZE; x++)
        for (int y = 0; y < WORLD_SIZE; y++) machineIndex[x][y] = -1;
    entGameTime = 0;
    entShake    = 0;
    machineUiX  = machineUiY = -1;
}

static Machine *MachineAt(int x, int y) {
    if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) return NULL;
    short i = machineIndex[x][y];
    return (i >= 0) ? &machines[i] : NULL;
}

static Machine *AddMachineAt(int x, int y, TileType type, int dir) {
    if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) return NULL;

    // Reuse the record already on this tile instead of allocating a
    // second one. Overwriting machineIndex used to ORPHAN the old
    // record — active forever, unreachable, slowly eating the pool.
    short existing = machineIndex[x][y];
    int slot = -1;
    if (existing >= 0 && machines[existing].active) {
        slot = existing;
    } else {
        for (int i = 0; i < MAX_MACHINES; i++) {
            if (!machines[i].active) { slot = i; break; }
        }
    }
    if (slot < 0) return NULL;   // pool genuinely full — caller must refuse

    memset(&machines[slot], 0, sizeof(Machine));
    machines[slot].active = true;
    machines[slot].x = x;  machines[slot].y = y;
    machines[slot].type = type;
    machines[slot].dir = dir & 3;
    machineIndex[x][y] = (short)slot;
    return &machines[slot];
}

// How many slots does this machine actually expose? A chest is a
// full 7x7; a drill needs a few (rock yields four different ores);
// belts and inserters carry exactly one item.
#define DRILL_SLOTS 8
static int MachineSlotCount(const Machine *m) {
    if (m == NULL) return 0;
    if (m->type == TILE_CHEST) return MACHINE_SLOTS;
    if (m->type == TILE_DRILL) return DRILL_SLOTS;
    return 1;
}

// Try to stuff items into a machine. Respects STACK_MAX per slot and
// spills into further slots, so nothing is silently swallowed or
// duplicated. Returns how many actually fit.
static int MachineAddItem(Machine *m, ItemID id, int amount) {
    if (m == NULL || id <= ITEM_NONE || id >= ITEM_COUNT || amount <= 0) return 0;
    int slots = MachineSlotCount(m);
    int placed = 0;

    for (int s = 0; s < slots && amount > 0; s++) {        // top up stacks
        if (m->slots[s] == id && m->counts[s] > 0 && m->counts[s] < STACK_MAX) {
            int room = STACK_MAX - m->counts[s];
            int take = amount < room ? amount : room;
            m->counts[s] += take;
            amount -= take; placed += take;
        }
    }
    for (int s = 0; s < slots && amount > 0; s++) {        // then empty slots
        if (m->slots[s] == ITEM_NONE || m->counts[s] <= 0) {
            int take = amount < STACK_MAX ? amount : STACK_MAX;
            m->slots[s] = id; m->counts[s] = take;
            amount -= take; placed += take;
        }
    }
    return placed;
}

// Push ONE item into whatever machine sits at (x,y). Used by the
// drill to unload onto the belt in front of it — the reason a drill
// no longer needs an inserter babysitting it.
static bool MachinePushInto(int x, int y, ItemID id) {
    Machine *dest = MachineAt(x, y);
    if (dest == NULL || id <= ITEM_NONE) return false;
    if (TileIsBelt(dest->type)) {
        if (dest->slots[0] == ITEM_NONE || dest->counts[0] <= 0) {
            dest->slots[0] = id; dest->counts[0] = 1; dest->beltProgress = 0;
            return true;
        }
        if (dest->slots[0] == id && dest->counts[0] < BELT_CAPACITY) {
            dest->counts[0]++;
            return true;
        }
        return false;
    }
    if (dest->type == TILE_CHEST)  return MachineAddItem(dest, id, 1) > 0;
    if (dest->type == TILE_TURRET && id == ITEM_BULLET) { dest->ammo++; return true; }
    return false;
}

// ─── Belt corners, auto-formed ────────────────────────────
// Factorio doesn't make you place corner pieces: a belt bends
// because its neighbour feeds it from the side. Same here — this
// returns the direction the incoming belt comes FROM when that
// feed is perpendicular, or -1 for a straight run.
static int BeltInputSide(const Machine *m) {
    if (m == NULL || !TileIsBelt(m->type)) return -1;
    int found = -1;
    for (int d = 0; d < 4; d++) {
        if (d == m->dir) continue;                  // that's our output side
        if (((d ^ 2) & 3) == m->dir) continue;      // straight-behind = not a bend
        Machine *n = MachineAt(m->x + DIR_DX[d], m->y + DIR_DY[d]);
        if (n == NULL || !TileIsBelt(n->type)) continue;
        // Does that neighbour actually point AT us?
        if (n->x + DIR_DX[n->dir] == m->x && n->y + DIR_DY[n->dir] == m->y) {
            if (found >= 0) return -1;              // fed from both sides → keep it straight
            found = d;
        }
    }
    return found;
}

// ─── Fuel ─────────────────────────────────────────────────
// Drills and inserters burn coal. `coal` is the hopper, `fuel` is
// the seconds left on the lump currently burning. Returns false when
// the machine is dry, which is what stops it working.
static bool MachineConsumeFuel(Machine *m, float dt) {
    if (!TileNeedsFuel(m->type)) return true;   // unpowered machines: always on
    if (m->fuel > 0) { m->fuel -= dt; return true; }
    if (m->coal > 0) {                          // light the next lump
        m->coal--;
        m->fuel = COAL_FUEL_SECONDS;
        return true;
    }
    return false;
}

// Hand one coal to a machine's hopper. Returns false when full —
// that's what makes the Z-drag stop topping up a loaded machine.
static bool MachineAddCoal(Machine *m) {
    if (m == NULL || !TileNeedsFuel(m->type)) return false;
    if (m->coal >= MACHINE_FUEL_MAX) return false;
    m->coal++;
    return true;
}

// Pop ONE item out of the first non-empty slot (inserters grab one
// at a time, like their Factorio ancestors).
static ItemID MachineTakeItem(Machine *m) {
    if (m == NULL) return ITEM_NONE;
    for (int s = 0; s < MachineSlotCount(m); s++) {
        if (m->slots[s] != ITEM_NONE && m->counts[s] > 0) {
            ItemID id = m->slots[s];
            if (--m->counts[s] <= 0) { m->slots[s] = ITEM_NONE; m->counts[s] = 0; }
            return id;
        }
    }
    return ITEM_NONE;
}

static void MachineGiveContentsTo(Machine *m, Player *p) {
    if (m == NULL || p == NULL) return;
    for (int s = 0; s < MACHINE_SLOTS; s++) {
        if (m->slots[s] != ITEM_NONE && m->counts[s] > 0)
            PlayerGiveItem(p, m->slots[s], m->counts[s]);
        m->slots[s] = ITEM_NONE; m->counts[s] = 0;
    }
    if (m->ammo > 0) { PlayerGiveItem(p, ITEM_BULLET, m->ammo); m->ammo = 0; }
    if (m->coal > 0) { PlayerGiveItem(p, ITEM_COAL, m->coal);   m->coal = 0; }
}

// Remove the machine on a tile (it broke). If `giveTo` is non-NULL
// the contents are salvaged into that player's inventory; mobs and
// explosions pass NULL — destruction is lossy.
static void RemoveMachineAt(int x, int y, Player *giveTo) {
    Machine *m = MachineAt(x, y);
    if (m == NULL) return;
    if (giveTo != NULL) MachineGiveContentsTo(m, giveTo);
    machineIndex[x][y] = -1;
    m->active = false;
}

// Give EVERY tile that needs per-instance state a machine record.
// Called after worldgen and — importantly — after any failed or
// partial entity load: the tile grid survives such a failure, so
// without this pass the map would be full of belts and chests with
// no record behind them (which draw as broken-X stubs and do
// nothing). Contents and facing are lost, but the base still
// stands and still works.
static void EntitiesRegisterWorldMachines(void) {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            TileType t = world[x][y].type;
            if (t == TILE_SPAWNER) {
                if (MachineAt(x, y) == NULL) {
                    Machine *m = AddMachineAt(x, y, TILE_SPAWNER, 0);
                    if (m != NULL) m->timer = (float)GetRandomValue(2, 12);
                }
            } else if (TileIsMachine(t) && MachineAt(x, y) == NULL) {
                AddMachineAt(x, y, t, 0);
            }
        }
    }
}

// ─── Effects ──────────────────────────────────────────────
static void AddEffect(int kind, Vector2 pos, float maxRadius, float life) {
    for (int i = 0; i < MAX_EFFECTS; i++) {
        if (!effects[i].active) {
            effects[i] = (Effect){ true, kind, pos, 0, life, maxRadius };
            return;
        }
    }
}

static void AddBeam(Vector2 from, Vector2 to) {
    for (int i = 0; i < MAX_BEAMS; i++) {
        if (!beams[i].active) {
            beams[i] = (Beam){ true, from, to, 0.12f };
            return;
        }
    }
}

// ─── Mobs ─────────────────────────────────────────────────
static int MobCount(void) {
    int n = 0;
    for (int i = 0; i < MAX_MOBS; i++) if (mobs[i].active) n++;
    return n;
}

static void SpawnMobAt(Vector2 pos) {
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!mobs[i].active) {
            float evo = EvolutionFactor();
            mobs[i].active = true;
            mobs[i].pos = pos;
            mobs[i].home = pos;
            mobs[i].target = pos;
            mobs[i].maxHp = TUNE.mobHp * (1.0f + 2.0f * evo);  // 1x → 3x health
            mobs[i].hp = mobs[i].maxHp;
            mobs[i].state = MOB_IDLE;
            mobs[i].retarget = 0;
            return;
        }
    }
}

static void MobDamage(Mob *m, float dmg) {
    m->hp -= dmg;
    if (m->hp <= 0) {
        m->active = false;
        AddEffect(EFFECT_RING, m->pos, 16, 0.25f);
    } else {
        AddEffect(EFFECT_SPARK, m->pos, 7, 0.12f);   // hit feedback
    }
}

// ─── Rage ─────────────────────────────────────────────────
// Hitting a nest is a declaration of war: every mob for a wide
// radius drops what it's doing and comes for you. This is what
// makes attacking a base feel like kicking a hornet's nest.
static void EnrageMobsAround(Vector2 at) {
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!mobs[i].active) continue;
        if (Vector2Distance(mobs[i].pos, at) > 620.0f) continue;
        mobs[i].state = MOB_RAID;
        mobs[i].retarget = 0;
        mobs[i].rage = 12.0f;      // seconds of faster, angrier pursuit
    }
}

// Hurt every mob within `radius` of a point (explosions).
static void DamageMobsInRadius(Vector2 pos, float radius, float dmg) {
    for (int i = 0; i < MAX_MOBS; i++) {
        if (mobs[i].active && Vector2Distance(mobs[i].pos, pos) <= radius)
            MobDamage(&mobs[i], dmg);
    }
}

// ─── Explosions ───────────────────────────────────────────
// Bombs are the great equalizer: they crack spawner nests, enemy
// walls, YOUR walls, mobs, and you. Exploded tiles drop nothing —
// destruction is not mining.
static void ExplodeAt(Vector2 pos, float radius, Player *p) {
    int tx0 = (int)((pos.x - radius) / TILE_SIZE), tx1 = (int)((pos.x + radius) / TILE_SIZE);
    int ty0 = (int)((pos.y - radius) / TILE_SIZE), ty1 = (int)((pos.y + radius) / TILE_SIZE);
    for (int tx = tx0; tx <= tx1; tx++) {
        for (int ty = ty0; ty <= ty1; ty++) {
            if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) continue;
            Vector2 c = { (tx + 0.5f) * TILE_SIZE, (ty + 0.5f) * TILE_SIZE };
            if (Vector2Distance(c, pos) > radius) continue;
            bool wasNest = (world[tx][ty].type == TILE_SPAWNER);
            if (WorldDamageTile(tx, ty, 150.0f)) {
                RemoveMachineAt(tx, ty, NULL);
                if (wasNest) EnrageMobsAround(c);   // bombing a nest = war
            }
        }
    }
    DamageMobsInRadius(pos, radius * 1.2f, 90.0f);
    if (p != NULL && Vector2Distance(p->pos, pos) <= radius * 1.2f) {
        PlayerDamage(p, 55.0f);
    }
    AddEffect(EFFECT_RING, pos, radius, 0.45f);
    entShake += 7.0f;   // explosions should be FELT
}

// ─── Projectiles ──────────────────────────────────────────
static void SpawnProjectile(Vector2 origin, Vector2 target, ItemID type, bool fromTurret) {
    Vector2 direction = Vector2Normalize(Vector2Subtract(target, origin));
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].active      = true;
            projectiles[i].pos         = origin;
            projectiles[i].dir         = direction;
            projectiles[i].traveled    = 0;
            projectiles[i].type        = type;
            projectiles[i].ignoreTiles = fromTurret;
            if (type == ITEM_BULLET) {
                projectiles[i].speed    = TUNE.pistolBulletSpeed;
                projectiles[i].maxRange = fromTurret ? TUNE.turretRange : TUNE.pistolReach;
            } else {
                projectiles[i].speed    = TUNE.slingshotSpeed;
                projectiles[i].maxRange = TUNE.slingshotReach;
            }
            return;
        }
    }
}

static void UpdateProjectiles(float dt, Player *p) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *proj = &projectiles[i];
        if (!proj->active) continue;

        Vector2 previousPos = proj->pos;
        proj->pos = Vector2Add(proj->pos, Vector2Scale(proj->dir, proj->speed * dt));
        proj->traveled += Vector2Distance(previousPos, proj->pos);
        if (proj->traveled >= proj->maxRange) { proj->active = false; continue; }

        // Mobs first: a bullet that finds flesh never reaches the wall.
        bool hitMob = false;
        for (int mi = 0; mi < MAX_MOBS; mi++) {
            if (!mobs[mi].active) continue;
            if (Vector2Distance(mobs[mi].pos, proj->pos) < 14.0f) {
                float dmg = (proj->type == ITEM_BULLET) ? TUNE.pistolBulletDamage
                                                        : TUNE.slingshotDamage;
                MobDamage(&mobs[mi], dmg);
                proj->active = false;
                hitMob = true;
                break;
            }
        }
        if (hitMob) continue;

        // Turret rounds arc over terrain — no tile interaction, so a
        // turret behind your wall doesn't shoot your wall. The ONE
        // exception is a nest: turrets are allowed to grind those
        // down, and cracking one enrages everything nearby.
        if (proj->ignoreTiles) {
            int px = (int)(proj->pos.x / TILE_SIZE);
            int py = (int)(proj->pos.y / TILE_SIZE);
            if (px >= 0 && px < WORLD_SIZE && py >= 0 && py < WORLD_SIZE &&
                world[px][py].type == TILE_SPAWNER) {
                if (WorldDamageTile(px, py, TUNE.pistolBulletDamage)) {
                    RemoveMachineAt(px, py, NULL);
                    EnrageMobsAround((Vector2){ (px + 0.5f) * TILE_SIZE,
                                                (py + 0.5f) * TILE_SIZE });
                }
                AddEffect(EFFECT_SPARK, proj->pos, 6, 0.1f);
                proj->active = false;
            }
            continue;
        }

        int tx = (int)(proj->pos.x / TILE_SIZE);
        int ty = (int)(proj->pos.y / TILE_SIZE);
        if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) {
            proj->active = false;
            continue;
        }
        TileType hitType = world[tx][ty].type;
        if (!TILES[hitType].breakable) continue;
        // Ankle-high quarter pebbles (stone OR ore) don't stop
        // bullets — shots sail over them the same way your feet do.
        if (TileIsRockLike(hitType) && (world[tx][ty].variant & 3) == 3) continue;

        // Damage falloff with distance (same rules as before).
        float distanceRatio = proj->traveled / proj->maxRange;
        if (distanceRatio > 1.0f) distanceRatio = 1.0f;
        float damage;
        if (proj->type == ITEM_BULLET) {
            damage = TUNE.pistolBulletDamage * (1.0f - distanceRatio * 0.5f);
            if (damage < 1.0f) damage = 1.0f;
        } else {
            damage = TUNE.slingshotDamage * (1.0f - distanceRatio);
            if (damage < 0.5f) damage = 0.5f;
        }
        if (WorldDamageTile(tx, ty, damage)) {
            // Shot tiles ARE mined — the shooter collects the drops.
            int drops[ITEM_COUNT];
            RollTileBreakDrops(hitType, drops);
            for (int d = 1; d < ITEM_COUNT; d++)
                if (drops[d] > 0) PlayerGiveItem(p, (ItemID)d, drops[d]);
            RemoveMachineAt(tx, ty, p);
        }
        AddEffect(EFFECT_SPARK, proj->pos, 6, 0.1f);   // chip of dust
        proj->active = false;
    }
}

// ─── Bombs ────────────────────────────────────────────────
static void PlaceBomb(Vector2 pos) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].active) {
            bombs[i] = (Bomb){ true, pos, BOMB_FUSE };
            return;
        }
    }
}

static void UpdateBombs(float dt, Player *p) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].active) continue;
        bombs[i].fuse -= dt;
        if (bombs[i].fuse <= 0) {
            bombs[i].active = false;
            ExplodeAt(bombs[i].pos, BOMB_RADIUS, p);
        }
    }
}

// ─── Mob AI ───────────────────────────────────────────────
// Two states. IDLE: loiter near home, but aggro if the player gets
// close. RAID: march on the player; anything in the way gets chewed.
static void UpdateMobs(float dt, Player *p) {
    float evo = EvolutionFactor();
    float speed = TUNE.mobSpeed * (1.0f + 0.5f * evo);

    for (int i = 0; i < MAX_MOBS; i++) {
        Mob *m = &mobs[i];
        if (!m->active) continue;

        m->retarget -= dt;
        if (m->rage > 0) m->rage -= dt;

        if (m->state == MOB_IDLE) {
            // Nest guards get more alert as evolution rises: early
            // game you can skirt a patch; late game they notice you
            // from three times as far. Standing ON their home ground
            // (near the nest they spawned from) sets them off no
            // matter how young the world is.
            float aggroRange = 70.0f + 190.0f * evo;
            if (Vector2Distance(p->pos, m->home) < 190.0f) {
                m->state = MOB_RAID;                 // you're in their yard
                m->retarget = 0;
                m->rage = 8.0f;
            } else if (Vector2Distance(m->pos, p->pos) < aggroRange) {
                m->state = MOB_RAID;              // you woke the nest guard
            } else if (m->retarget <= 0) {
                m->target = (Vector2){ m->home.x + GetRandomValue(-90, 90),
                                       m->home.y + GetRandomValue(-90, 90) };
                m->retarget = (float)GetRandomValue(3, 6);
            }
        }
        if (m->state == MOB_RAID && m->retarget <= 0) {
            m->target = p->pos;                   // re-acquire the player
            m->retarget = 1.5f;
        }

        // Move axis-by-axis (same trick as PlayerMove) so mobs slide
        // along walls instead of sticking to them. Enraged mobs move
        // noticeably faster — you can see the swarm accelerate.
        float moveSpeed = speed * (m->rage > 0 ? 1.45f : 1.0f);
        Vector2 d = Vector2Subtract(m->target, m->pos);
        if (Vector2Length(d) > 6.0f) {
            Vector2 dir = Vector2Normalize(d);
            Vector2 nx = { m->pos.x + dir.x * moveSpeed * dt, m->pos.y };
            Vector2 ny = { m->pos.x, m->pos.y + dir.y * moveSpeed * dt };
            bool movedX = false, movedY = false;
            if (WorldPositionWalkableEx(nx, false)) { m->pos.x = nx.x; movedX = true; }
            if (WorldPositionWalkableEx(ny, false)) { m->pos.y = ny.y; movedY = true; }

            // Fully blocked while raiding? CHEW. Find the tile in the
            // way and gnaw it down — walls are food, doors are food,
            // your turrets are food.
            if (!movedX && !movedY && m->state == MOB_RAID) {
                int tx = (int)((m->pos.x + dir.x * TILE_SIZE * 0.8f) / TILE_SIZE);
                int ty = (int)((m->pos.y + dir.y * TILE_SIZE * 0.8f) / TILE_SIZE);
                if (tx >= 0 && tx < WORLD_SIZE && ty >= 0 && ty < WORLD_SIZE &&
                    TILES[world[tx][ty].type].breakable) {
                    float chew = TUNE.mobChewDPS * (1.0f + 1.0f * evo);
                    if (WorldDamageTile(tx, ty, chew * dt)) {
                        RemoveMachineAt(tx, ty, NULL);   // eaten, not salvaged
                    }
                }
            }
        }

        // Contact damage.
        if (Vector2Distance(m->pos, p->pos) < PLAYER_RADIUS + 10.0f) {
            PlayerDamage(p, TUNE.mobContactDPS * (1.0f + evo) * dt);
        }
    }

    // Separation: overlapping mobs shove each other apart so a wave
    // arrives as a PACK, not as one super-mob stack. (O(n²), but
    // n ≤ 96 — 4,560 checks is nothing.)
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!mobs[i].active) continue;
        for (int j = i + 1; j < MAX_MOBS; j++) {
            if (!mobs[j].active) continue;
            Vector2 d = Vector2Subtract(mobs[j].pos, mobs[i].pos);
            float dist = Vector2Length(d);
            float minDist = 15.0f;
            if (dist < minDist && dist > 0.001f) {
                Vector2 push = Vector2Scale(d, (minDist - dist) / dist * 0.5f);
                mobs[i].pos = Vector2Subtract(mobs[i].pos, push);
                mobs[j].pos = Vector2Add(mobs[j].pos, push);
            } else if (dist <= 0.001f) {
                mobs[j].pos.x += (float)GetRandomValue(-4, 4);   // perfectly stacked → nudge
                mobs[j].pos.y += (float)GetRandomValue(-4, 4);
            }
        }
    }
}

// ─── Spawners + emergent waves ────────────────────────────
// No scheduled "raid events". Like biters: nests keep breeding, the
// brood loiters at home, and once a nest's crowd grows past its
// (randomized) patience, the whole bunch sets off at once. Waves
// emerge from POPULATION PRESSURE, not from a timer you can clock.
static void UpdateSpawnersAndRaids(float dt, Player *p) {
    (void)p;   // waves target the player INDIRECTLY (mob state changes)
    float evo = EvolutionFactor();
    int   population = MobCount();
    int   populationCap = 24 + (int)((MAX_MOBS - 24) * evo);   // grows over time
    float interval = TUNE.spawnIntervalStart +
                     (TUNE.spawnIntervalEnd - TUNE.spawnIntervalStart) * evo;

    for (int i = 0; i < MAX_MACHINES; i++) {
        Machine *m = &machines[i];
        if (!m->active || m->type != TILE_SPAWNER) continue;
        m->timer -= dt;
        if (m->timer <= 0) {
            m->timer = interval * (0.75f + GetRandomValue(0, 50) / 100.0f);
            Vector2 nest = { (m->x + 0.5f) * TILE_SIZE, (m->y + 0.5f) * TILE_SIZE };

            if (population < populationCap) {
                Vector2 at = { nest.x + GetRandomValue(-40, 40),
                               nest.y + GetRandomValue(-40, 40) };
                if (WorldPositionWalkableEx(at, false)) {
                    SpawnMobAt(at);
                    population++;
                }
            }

            // Crowd check: how many of OUR brood are loitering here?
            // Past the threshold (bigger + a random fudge so you
            // can't predict it), everyone marches at once.
            int loitering = 0;
            for (int mi = 0; mi < MAX_MOBS; mi++) {
                if (mobs[mi].active && mobs[mi].state == MOB_IDLE &&
                    Vector2Distance(mobs[mi].pos, nest) < TILE_SIZE * 7.0f) loitering++;
            }
            int threshold = (int)(TUNE.waveSize + evo * 5.0f) + GetRandomValue(0, 3);
            if (loitering >= threshold) {
                for (int mi = 0; mi < MAX_MOBS; mi++) {
                    if (mobs[mi].active && mobs[mi].state == MOB_IDLE &&
                        Vector2Distance(mobs[mi].pos, nest) < TILE_SIZE * 7.0f) {
                        mobs[mi].state = MOB_RAID;
                        mobs[mi].retarget = 0;
                    }
                }
            }
        }
    }
}

// ─── Machines: drills, belts, hands, turrets ──────────────
static void UpdateMachines(float dt, Player *p) {
    (void)p;
    for (int i = 0; i < MAX_MACHINES; i++) {
        Machine *m = &machines[i];
        if (!m->active) continue;
        Vector2 center = { (m->x + 0.5f) * TILE_SIZE, (m->y + 0.5f) * TILE_SIZE };

        switch (m->type) {

        case TILE_DRILL: {
            if (!MachineConsumeFuel(m, dt)) break;   // out of coal → idle

            // Unload: shove one item onto whatever the drill FACES.
            // This is what lets a drill feed a belt directly, with
            // no inserter in between.
            for (int s = 0; s < MachineSlotCount(m); s++) {
                if (m->slots[s] == ITEM_NONE || m->counts[s] <= 0) continue;
                if (MachinePushInto(m->x + DIR_DX[m->dir], m->y + DIR_DY[m->dir], m->slots[s])) {
                    if (--m->counts[s] <= 0) { m->slots[s] = ITEM_NONE; m->counts[s] = 0; }
                }
                break;
            }

            m->timer -= dt;
            if (m->timer > 0) break;
            m->timer = TUNE.drillInterval;
            // Sitting on an ore FIELD? Pump that ore forever —
            // that's the whole point of placing drills on patches.
            unsigned char ore = world[m->x][m->y].ore;
            if (ore != 0) {
                MachineAddItem(m, WorldOreItem(ore), 1);
                break;
            }
            // Otherwise gnaw the four neighbors (rocks, trees, and
            // solid ore nodes); drops land in the drill's own slots.
            int order = GetRandomValue(0, 3);
            for (int k = 0; k < 4; k++) {
                int dir = (order + k) % 4;
                int nx = m->x + DIR_DX[dir], ny = m->y + DIR_DY[dir];
                if (nx < 0 || nx >= WORLD_SIZE || ny < 0 || ny >= WORLD_SIZE) continue;
                TileType t = world[nx][ny].type;
                if (!TileIsRockLike(t) && t != TILE_TREE) continue;
                if (WorldDamageTile(nx, ny, DRILL_BITE)) {
                    int drops[ITEM_COUNT];
                    RollTileBreakDrops(t, drops);
                    for (int d = 1; d < ITEM_COUNT; d++)
                        if (drops[d] > 0) MachineAddItem(m, (ItemID)d, drops[d]);
                }
                break;   // one bite per tick
            }
            break;
        }

        case TILE_CONVEYOR:
        case TILE_CONVEYOR_CORNER: {
            // Belts run on their own — no fuel, always moving. The
            // item slides visually via beltProgress (0..1) and then
            // hops to the next tile when the timer expires.
            if (m->slots[0] == ITEM_NONE || m->counts[0] <= 0) {
                m->beltProgress = 0;
                m->timer = TUNE.conveyorInterval;
                break;
            }
            m->timer -= dt;
            m->beltProgress = 1.0f - (m->timer / TUNE.conveyorInterval);
            if (m->beltProgress < 0) m->beltProgress = 0;
            if (m->beltProgress > 1) m->beltProgress = 1;
            if (m->timer > 0) break;
            m->timer = TUNE.conveyorInterval;

            Machine *dest = MachineAt(m->x + DIR_DX[m->dir], m->y + DIR_DY[m->dir]);
            if (dest == NULL) break;       // dead end: hold the item
            ItemID id = m->slots[0];
            bool moved = false;
            if (TileIsBelt(dest->type)) {
                // A belt tile carries a small STACK of one item type.
                // Anything it can't accept stays put on this tile
                // instead of evaporating — items never get lost, the
                // line just backs up like a real conveyor.
                if (dest->slots[0] == ITEM_NONE || dest->counts[0] <= 0) {
                    dest->slots[0] = id; dest->counts[0] = 1;
                    dest->beltProgress = 0;
                    moved = true;
                } else if (dest->slots[0] == id && dest->counts[0] < BELT_CAPACITY) {
                    dest->counts[0]++;
                    moved = true;
                }
            } else if (dest->type == TILE_CHEST) {
                moved = MachineAddItem(dest, id, 1) > 0;
            } else if (dest->type == TILE_TURRET && id == ITEM_BULLET) {
                dest->ammo += 1; moved = true;    // belt-fed turrets!
            } else if (TileNeedsFuel(dest->type) && id == ITEM_COAL) {
                moved = MachineAddCoal(dest);     // belt-fed fuel!
            }
            if (moved && --m->counts[0] <= 0) {
                m->slots[0] = ITEM_NONE; m->counts[0] = 0;
                m->beltProgress = 0;
            }
            break;
        }

        case TILE_INSERTER: {
            // The robotic hand: grab ONE item from the tile BEHIND,
            // drop it on the tile IN FRONT. Holds the item (slot 0)
            // until the destination has room. Runs on coal.
            if (!MachineConsumeFuel(m, dt)) break;   // out of coal → arm stops
            m->timer -= dt;
            if (m->timer > 0) break;
            m->timer = TUNE.inserterInterval;
            if (m->slots[0] == ITEM_NONE) {   // hand empty → try to grab
                Machine *src = MachineAt(m->x - DIR_DX[m->dir], m->y - DIR_DY[m->dir]);
                if (src != NULL && (src->type == TILE_CHEST || src->type == TILE_DRILL ||
                                    TileIsBelt(src->type))) {
                    ItemID got = MachineTakeItem(src);
                    if (got != ITEM_NONE) { m->slots[0] = got; m->counts[0] = 1; }
                }
            }
            if (m->slots[0] != ITEM_NONE) {   // hand full → try to place
                Machine *dest = MachineAt(m->x + DIR_DX[m->dir], m->y + DIR_DY[m->dir]);
                ItemID id = m->slots[0];
                bool placed = false;
                if (dest != NULL) {
                    if (dest->type == TILE_CHEST) placed = MachineAddItem(dest, id, 1) > 0;
                    else if (TileIsBelt(dest->type)) {
                        if (dest->slots[0] == ITEM_NONE || dest->counts[0] <= 0) {
                            dest->slots[0] = id; dest->counts[0] = 1;
                            dest->beltProgress = 0;
                            placed = true;
                        } else if (dest->slots[0] == id && dest->counts[0] < BELT_CAPACITY) {
                            dest->counts[0]++;
                            placed = true;
                        }
                    }
                    else if (dest->type == TILE_TURRET && id == ITEM_BULLET) {
                        dest->ammo += 1; placed = true;
                    }
                    else if (TileNeedsFuel(dest->type) && id == ITEM_COAL) {
                        placed = MachineAddCoal(dest);   // arms can refuel arms
                    }
                }
                if (placed) { m->slots[0] = ITEM_NONE; m->counts[0] = 0; }
            }
            break;
        }

        case TILE_TURRET: {
            m->timer -= dt;
            if (m->timer > 0 || m->ammo <= 0) break;
            // Nearest mob in range gets a bullet.
            int   bestI = -1;
            float bestD = TUNE.turretRange;
            for (int mi = 0; mi < MAX_MOBS; mi++) {
                if (!mobs[mi].active) continue;
                float d = Vector2Distance(mobs[mi].pos, center);
                if (d < bestD) { bestD = d; bestI = mi; }
            }
            if (bestI >= 0) {
                SpawnProjectile(center, mobs[bestI].pos, ITEM_BULLET, true);
                m->ammo--;
                m->timer = TUNE.turretCooldown;
                break;
            }
            // No mobs? Grind down any NEST in range — turrets push
            // the frontier forward on their own.
            int range = (int)(TUNE.turretRange / TILE_SIZE);
            int sx = -1, sy = -1;
            float nearest = TUNE.turretRange;
            for (int x = m->x - range; x <= m->x + range; x++) {
                for (int y = m->y - range; y <= m->y + range; y++) {
                    if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) continue;
                    if (world[x][y].type != TILE_SPAWNER) continue;
                    Vector2 c = { (x + 0.5f) * TILE_SIZE, (y + 0.5f) * TILE_SIZE };
                    float d = Vector2Distance(c, center);
                    if (d < nearest) { nearest = d; sx = x; sy = y; }
                }
            }
            if (sx >= 0) {
                Vector2 target = { (sx + 0.5f) * TILE_SIZE, (sy + 0.5f) * TILE_SIZE };
                SpawnProjectile(center, target, ITEM_BULLET, true);
                m->ammo--;
                m->timer = TUNE.turretCooldown;
            }
            break;
        }

        case TILE_LASER_TURRET: {
            m->timer -= dt;
            if (m->beamTtl > 0) m->beamTtl -= dt;
            if (m->timer > 0) break;
            int   bestI = -1;
            float bestD = TUNE.laserRange;
            for (int mi = 0; mi < MAX_MOBS; mi++) {
                if (!mobs[mi].active) continue;
                float d = Vector2Distance(mobs[mi].pos, center);
                if (d < bestD) { bestD = d; bestI = mi; }
            }
            if (bestI >= 0) {
                // Lasers hit INSTANTLY — no projectile, just a beam.
                m->beamTo  = mobs[bestI].pos;
                m->beamTtl = 0.12f;
                AddBeam(center, mobs[bestI].pos);
                MobDamage(&mobs[bestI], TUNE.laserDamage);
                m->timer = TUNE.laserCooldown;
            }
            break;
        }

        default: break;   // chests, spawners (spawners tick elsewhere)
        }
    }
}

// ─── Mining bots ──────────────────────────────────────────
static void SpawnBot(Vector2 pos) {
    for (int i = 0; i < MAX_BOTS; i++) {
        if (!bots[i].active) {
            memset(&bots[i], 0, sizeof(Bot));
            bots[i].active = true;
            bots[i].pos = pos;
            bots[i].state = BOT_FIND;
            return;
        }
    }
}

// Is another bot already working that tile?
static bool BotTileClaimed(int tx, int ty, int self) {
    for (int i = 0; i < MAX_BOTS; i++) {
        if (i == self || !bots[i].active) continue;
        if (bots[i].state != BOT_FIND && bots[i].tx == tx && bots[i].ty == ty) return true;
    }
    return false;
}

static Machine *BotNearestChest(Vector2 pos) {
    Machine *best = NULL;
    float bestD = 1e9f;
    for (int i = 0; i < MAX_MACHINES; i++) {
        if (!machines[i].active || machines[i].type != TILE_CHEST) continue;
        Vector2 c = { (machines[i].x + 0.5f) * TILE_SIZE, (machines[i].y + 0.5f) * TILE_SIZE };
        float d = Vector2Distance(pos, c);
        if (d < bestD) { bestD = d; best = &machines[i]; }
    }
    return best;
}

static void UpdateBots(float dt, Player *p) {
    for (int i = 0; i < MAX_BOTS; i++) {
        Bot *b = &bots[i];
        if (!b->active) continue;

        if (b->state == BOT_FIND) {
            // Spiral-ish search: nearest unclaimed rock/tree in a
            // 40-tile box around the bot. Bots FLY, so no pathfinding
            // — straight lines only. (This is the "agentic" loop:
            // sense, decide, act, repeat.)
            int bx = (int)(b->pos.x / TILE_SIZE), by = (int)(b->pos.y / TILE_SIZE);
            int bestX = -1, bestY = -1;
            long bestD = 0x7FFFFFFF;
            for (int x = bx - 40; x <= bx + 40; x++) {
                for (int y = by - 40; y <= by + 40; y++) {
                    if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) continue;
                    TileType t = world[x][y].type;
                    if (!TileIsRockLike(t) && t != TILE_TREE) continue;   // ore nodes too
                    if (BotTileClaimed(x, y, i)) continue;
                    long dx = x - bx, dy = y - by;
                    long d = dx * dx + dy * dy;
                    if (d < bestD) { bestD = d; bestX = x; bestY = y; }
                }
            }
            if (bestX >= 0) { b->tx = bestX; b->ty = bestY; b->state = BOT_TO_TARGET; }
            // nothing to mine → hover in place and look decorative
        }

        Vector2 goal = b->pos;
        if (b->state == BOT_TO_TARGET || b->state == BOT_MINING) {
            goal = (Vector2){ (b->tx + 0.5f) * TILE_SIZE, (b->ty + 0.5f) * TILE_SIZE };
        } else if (b->state == BOT_TO_DROPOFF) {
            Machine *chest = BotNearestChest(b->pos);
            if (chest != NULL) {
                goal = (Vector2){ (chest->x + 0.5f) * TILE_SIZE, (chest->y + 0.5f) * TILE_SIZE };
                if (Vector2Distance(b->pos, goal) < TILE_SIZE * 1.2f) {
                    for (int d = 1; d < ITEM_COUNT; d++) {
                        if (b->inv[d] > 0 && MachineAddItem(chest, (ItemID)d, b->inv[d]) > 0)
                            b->inv[d] = 0;
                    }
                }
            } else {
                goal = p->pos;   // no chest anywhere → bring it to the boss
                if (Vector2Distance(b->pos, goal) < 50.0f) {
                    for (int d = 1; d < ITEM_COUNT; d++) {
                        if (b->inv[d] > 0) { PlayerGiveItem(p, (ItemID)d, b->inv[d]); b->inv[d] = 0; }
                    }
                }
            }
            b->carrying = 0;
            for (int d = 1; d < ITEM_COUNT; d++) b->carrying += b->inv[d];
            if (b->carrying == 0) b->state = BOT_FIND;
        }

        // Fly toward the goal.
        Vector2 d = Vector2Subtract(goal, b->pos);
        float dist = Vector2Length(d);
        if (dist > 4.0f) {
            b->pos = Vector2Add(b->pos, Vector2Scale(Vector2Normalize(d), TUNE.botSpeed * dt));
        }

        if (b->state == BOT_TO_TARGET && dist < TILE_SIZE * 0.9f) b->state = BOT_MINING;

        if (b->state == BOT_MINING) {
            TileType t = world[b->tx][b->ty].type;
            if (!TileIsRockLike(t) && t != TILE_TREE) { b->state = BOT_FIND; continue; }
            if (WorldDamageTile(b->tx, b->ty, TUNE.botDPS * dt)) {
                int drops[ITEM_COUNT];
                RollTileBreakDrops(t, drops);
                for (int dd = 1; dd < ITEM_COUNT; dd++) b->inv[dd] += drops[dd];
                b->carrying = 0;
                for (int dd = 1; dd < ITEM_COUNT; dd++) b->carrying += b->inv[dd];
                // Full load → haul it home; otherwise keep mining.
                b->state = (b->carrying >= 30) ? BOT_TO_DROPOFF : BOT_FIND;
            }
        }
    }
}

// ─── Effects tick ─────────────────────────────────────────
static void UpdateEffects(float dt) {
    for (int i = 0; i < MAX_EFFECTS; i++) {
        if (effects[i].active && (effects[i].age += dt) >= effects[i].life)
            effects[i].active = false;
    }
    for (int i = 0; i < MAX_BEAMS; i++) {
        if (beams[i].active && (beams[i].ttl -= dt) <= 0)
            beams[i].active = false;
    }
}

// ─── Belts move you, too ──────────────────────────────────
// Standing on a running belt drags you along its direction — ride
// them like moving walkways. Returns the displacement to apply.
static Vector2 BeltCarry(Vector2 pos, float dt) {
    int tx = (int)(pos.x / TILE_SIZE), ty = (int)(pos.y / TILE_SIZE);
    if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) return (Vector2){ 0, 0 };
    if (!TileIsBelt(world[tx][ty].type)) return (Vector2){ 0, 0 };
    Machine *m = MachineAt(tx, ty);
    if (m == NULL) return (Vector2){ 0, 0 };
    return (Vector2){ DIR_DX[m->dir] * BELT_CARRY_SPEED * dt,
                      DIR_DY[m->dir] * BELT_CARRY_SPEED * dt };
}

// ─── F: scoop items off belts ─────────────────────────────
// Hold F to pull cargo off any belt within reach — the manual
// override for when you want something back off the line.
static void BeltPickupNear(Player *p, float radiusPx) {
    int cx = (int)(p->pos.x / TILE_SIZE), cy = (int)(p->pos.y / TILE_SIZE);
    int r = (int)(radiusPx / TILE_SIZE) + 1;
    for (int x = cx - r; x <= cx + r; x++) {
        for (int y = cy - r; y <= cy + r; y++) {
            if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) continue;
            if (!TileIsBelt(world[x][y].type)) continue;
            Vector2 c = { (x + 0.5f) * TILE_SIZE, (y + 0.5f) * TILE_SIZE };
            if (Vector2Distance(p->pos, c) > radiusPx) continue;
            Machine *m = MachineAt(x, y);
            if (m == NULL || m->slots[0] == ITEM_NONE || m->counts[0] <= 0) continue;
            PlayerGiveItem(p, m->slots[0], m->counts[0]);
            m->slots[0] = ITEM_NONE;
            m->counts[0] = 0;
            m->beltProgress = 0;
        }
    }
}

// ─── Self-healing: keep records and tiles in agreement ────
// A machine record whose tile has changed underneath it (mined,
// blown up, chewed) is stale — free it, or the pool slowly fills
// with ghosts until new belts get no record at all and render as
// dead east-facing stubs. Cheap: one pass over the pool, not the
// 409k-tile map.
static void EntitiesReconcile(void) {
    for (int i = 0; i < MAX_MACHINES; i++) {
        Machine *m = &machines[i];
        if (!m->active) continue;
        if (m->x < 0 || m->x >= WORLD_SIZE || m->y < 0 || m->y >= WORLD_SIZE) {
            m->active = false;
            continue;
        }
        if (world[m->x][m->y].type != m->type) {
            if (machineIndex[m->x][m->y] == (short)i) machineIndex[m->x][m->y] = -1;
            m->active = false;
        }
    }
}

// One call from main.c per gameplay frame.
static void EntitiesUpdate(float dt, Player *p) {
    entGameTime += dt;
    EntitiesReconcile();
    UpdateSpawnersAndRaids(dt, p);
    UpdateMachines(dt, p);
    UpdateMobs(dt, p);
    UpdateBots(dt, p);
    UpdateProjectiles(dt, p);
    UpdateBombs(dt, p);
    UpdateEffects(dt);
    // Shake decays fast — a thump, not an earthquake.
    entShake -= entShake * 9.0f * dt;
    if (entShake > 12.0f) entShake = 12.0f;
    if (entShake < 0.05f) entShake = 0;
}

// ─── Drawing (world space — call inside BeginMode2D) ──────
static void EntitiesDrawWorld(Vector2 viewTopLeft, Vector2 viewBottomRight) {
    // Machine tile decorations, only over the visible slice.
    int x0 = (int)(viewTopLeft.x / TILE_SIZE) - 1, y0 = (int)(viewTopLeft.y / TILE_SIZE) - 1;
    int x1 = (int)(viewBottomRight.x / TILE_SIZE) + 1, y1 = (int)(viewBottomRight.y / TILE_SIZE) + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > WORLD_SIZE - 1) x1 = WORLD_SIZE - 1;
    if (y1 > WORLD_SIZE - 1) y1 = WORLD_SIZE - 1;

    for (int x = x0; x <= x1; x++) {
        for (int y = y0; y <= y1; y++) {
            TileType t = world[x][y].type;
            float px = (float)(x * TILE_SIZE), py = (float)(y * TILE_SIZE);

            if (t == TILE_SPAWNER) {
                // Pulsing core so nests read as ALIVE and dangerous.
                float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 3.0f + x * 0.7f);
                DrawCircle((int)(px + TILE_SIZE / 2), (int)(py + TILE_SIZE / 2),
                           6 + 4 * pulse, (Color){ 220, 60, 220, 200 });
                continue;
            }
            Machine *m = MachineAt(x, y);
            Vector2 center = { px + TILE_SIZE / 2.0f, py + TILE_SIZE / 2.0f };

            // A machine tile with no record is BROKEN, not east-facing.
            // Say so loudly instead of drawing a plausible-looking belt
            // that silently does nothing.
            if (m == NULL && TileIsMachine(t)) {
                DrawRectangle((int)px, (int)py, TILE_SIZE, TILE_SIZE,
                              (Color){ 90, 20, 90, 220 });
                DrawLineEx((Vector2){ px + 3, py + 3 },
                           (Vector2){ px + TILE_SIZE - 3, py + TILE_SIZE - 3 }, 2, RAYWHITE);
                DrawLineEx((Vector2){ px + TILE_SIZE - 3, py + 3 },
                           (Vector2){ px + 3, py + TILE_SIZE - 3 }, 2, RAYWHITE);
                continue;
            }
            int dir = (m != NULL) ? m->dir : 0;

            if (TileIsBelt(t)) {
                // ── Animated belt ──────────────────────────────
                // The tread is a row of chevrons scrolling along the
                // belt's direction; the offset comes from wall-clock
                // time, so belts visibly RUN even when empty.
                float ts = (float)TILE_SIZE;
                DrawRectangle((int)px, (int)py, TILE_SIZE, TILE_SIZE, (Color){ 58, 54, 46, 255 });
                float dx = (float)DIR_DX[dir], dy = (float)DIR_DY[dir];
                float phase = fmodf((float)GetTime() / TUNE.conveyorInterval, 1.0f);
                // A bend is DETECTED, not placed: if a neighbouring
                // belt feeds us from the side, the tread curves.
                int inSide = BeltInputSide(m);
                for (int k = 0; k < 3; k++) {
                    float along = fmodf(phase + k / 3.0f, 1.0f);       // 0..1 down the tile
                    // On a bend the first half of the path runs in
                    // from that side, then turns toward the output.
                    float ax, ay;
                    if (inSide >= 0 && along < 0.5f) {
                        float sx = (float)DIR_DX[inSide], sy = (float)DIR_DY[inSide];
                        ax = center.x + sx * ts * (0.5f - along);
                        ay = center.y + sy * ts * (0.5f - along);
                    } else {
                        float u = along - 0.5f;          // -0.5 .. +0.5 along dir
                        ax = center.x + dx * ts * u;
                        ay = center.y + dy * ts * u;
                    }
                    // Chevron: a short bar across the belt.
                    Color tread = (Color){ 176, 156, 60, 210 };
                    DrawLineEx((Vector2){ ax - (-dy) * 7.0f, ay - dx * 7.0f },
                               (Vector2){ ax + (-dy) * 7.0f, ay + dx * 7.0f }, 3.0f, tread);
                }
                DrawRectangleLines((int)px, (int)py, TILE_SIZE, TILE_SIZE, (Color){ 34, 32, 28, 255 });
                // Nose arrow so the output side is unmistakable.
                Vector2 tip  = { center.x + dx * 10.0f, center.y + dy * 10.0f };
                Vector2 left = { center.x - dy * 4.5f - dx * 2.0f, center.y + dx * 4.5f - dy * 2.0f };
                Vector2 rght = { center.x + dy * 4.5f - dx * 2.0f, center.y - dx * 4.5f - dy * 2.0f };
                DrawTriangle(tip, left, rght, (Color){ 255, 255, 255, 190 });
                DrawTriangle(tip, rght, left, (Color){ 255, 255, 255, 190 });
            } else if (t == TILE_DOOR || t == TILE_CHEST || t == TILE_DRILL ||
                       t == TILE_INSERTER || t == TILE_TURRET ||
                       t == TILE_LASER_TURRET || t == TILE_RESEARCH) {
                ItemID icon = ItemThatPlaces(t);
                if (icon != ITEM_NONE) DrawItemSprite(icon, px + 2, py + 2, TILE_SIZE - 4);
            }

            if (m == NULL) continue;

            if (t == TILE_INSERTER) {
                // ── Animated linkage arm ───────────────────────
                // A base motor, an elbow motor, and a claw. The arm
                // swings from the pickup side (behind) to the drop
                // side (front) as its timer runs, so you can watch
                // it actually carry the item across.
                float swing;                       // -1 = behind, +1 = front
                float phase = 1.0f - (m->timer / TUNE.inserterInterval);
                if (phase < 0) phase = 0;
                if (phase > 1) phase = 1;
                bool carrying = (m->slots[0] != ITEM_NONE && m->counts[0] > 0);
                // Carrying → swinging forward; empty → returning back.
                swing = carrying ? (-1.0f + 2.0f * phase) : (1.0f - 2.0f * phase);

                float dxf = (float)DIR_DX[dir], dyf = (float)DIR_DY[dir];
                Vector2 baseP = center;
                float reach = TILE_SIZE * 0.62f;
                Vector2 clawP = { center.x + dxf * reach * swing,
                                  center.y + dyf * reach * swing };
                // Elbow is lifted perpendicular so the arm reads as a
                // linkage, not a stick.
                Vector2 elbow = { (baseP.x + clawP.x) * 0.5f - dyf * 6.0f,
                                  (baseP.y + clawP.y) * 0.5f + dxf * 6.0f };

                DrawLineEx(baseP, elbow, 3.0f, (Color){ 190, 175, 90, 255 });
                DrawLineEx(elbow, clawP, 3.0f, (Color){ 190, 175, 90, 255 });
                // Motors: circles at base and elbow.
                DrawCircleV(baseP, 5.0f, (Color){ 120, 120, 130, 255 });
                DrawCircleV(baseP, 2.5f, (Color){ 60, 60, 66, 255 });
                DrawCircleV(elbow, 3.5f, (Color){ 150, 150, 160, 255 });
                // Claw
                DrawCircleV(clawP, 4.0f, (Color){ 210, 190, 100, 255 });
                DrawCircleLinesV(clawP, 4.0f, (Color){ 50, 45, 25, 255 });
                if (carrying) {
                    DrawItemSprite(m->slots[0], clawP.x - 6, clawP.y - 6, 12);
                }
            }

            // Cargo. On a belt it SLIDES along the path as
            // beltProgress advances, so you can watch items travel.
            // (The inserter draws its own cargo in the claw, above.)
            if (m->slots[0] != ITEM_NONE && m->counts[0] > 0 && TileIsBelt(t)) {
                Vector2 at = center;
                if (TileIsBelt(t)) {
                    float u = m->beltProgress - 0.5f;         // -0.5 .. +0.5
                    if (t == TILE_CONVEYOR_CORNER && m->beltProgress < 0.5f) {
                        float sx = -(float)DIR_DY[dir], sy = (float)DIR_DX[dir];
                        at.x -= sx * TILE_SIZE * (0.5f - m->beltProgress);
                        at.y -= sy * TILE_SIZE * (0.5f - m->beltProgress);
                    } else {
                        at.x += DIR_DX[dir] * TILE_SIZE * u;
                        at.y += DIR_DY[dir] * TILE_SIZE * u;
                    }
                }
                DrawItemSprite(m->slots[0], at.x - 7, at.y - 7, 14);
                if (m->counts[0] > 1) {
                    DrawText(TextFormat("%d", m->counts[0]), (int)(at.x + 5),
                             (int)(at.y + 1), 10, RAYWHITE);
                }
            }

            if (t == TILE_TURRET) {
                DrawText(TextFormat("%d", m->ammo), (int)px + 3, (int)py + 2, 10,
                         m->ammo > 0 ? GOLD : RED);
            }

            // Fuel state: a flame pip while burning, a dark "no fuel"
            // veil when dry, so a stalled factory is obvious at a glance.
            if (TileNeedsFuel(t)) {
                if (m->fuel > 0 || m->coal > 0) {
                    float flick = 2.0f + 1.2f * sinf((float)GetTime() * 9.0f + x);
                    DrawCircle((int)(px + 4), (int)(py + TILE_SIZE - 5), flick,
                               (Color){ 255, 150, 40, 230 });
                    DrawText(TextFormat("%d", m->coal), (int)(px + 8),
                             (int)(py + TILE_SIZE - 11), 10, (Color){ 255, 210, 120, 220 });
                } else {
                    DrawRectangle((int)px, (int)py, TILE_SIZE, TILE_SIZE,
                                  (Color){ 10, 10, 16, 130 });
                    // A coal glyph marks exactly what it's begging for.
                    DrawItemSprite(ITEM_COAL, px + TILE_SIZE / 2.0f - 6,
                                   py + TILE_SIZE / 2.0f - 6, 12);
                    if (((int)(GetTime() * 2)) % 2 == 0) {
                        DrawRectangleLines((int)px, (int)py, TILE_SIZE, TILE_SIZE, RED);
                    }
                }
            }
        }
    }

    // Mobs: blobby circles that get bigger and angrier with evolution.
    float evo = EvolutionFactor();
    float mobR = 6.5f + 3.0f * evo;
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!mobs[i].active) continue;
        Vector2 mp = mobs[i].pos;
        Color body = (mobs[i].rage > 0) ? (Color){ 255, 90, 40, 255 }
                   : (mobs[i].state == MOB_RAID) ? (Color){ 210, 60, 50, 255 }
                                                 : (Color){ 160, 60, 130, 255 };
        if (mobs[i].rage > 0) {   // enraged: a hot halo you can read at a glance
            DrawCircleLinesV(mobs[i].pos, mobR + 3.0f, (Color){ 255, 140, 40, 190 });
        }
        DrawEllipse((int)(mp.x + worldShadowVec.x * 0.5f),
                    (int)(mp.y + worldShadowVec.y * 0.5f),
                    mobR * 1.05f, mobR * 0.8f, WORLD_SHADOW_COLOR);
        DrawCircleV(mp, mobR, body);
        DrawCircleLinesV(mp, mobR + 1, (Color){ 20, 10, 20, 255 });
        DrawCircle((int)(mp.x - 2), (int)(mp.y - 2), 1.5f, RAYWHITE);   // eyes
        DrawCircle((int)(mp.x + 2), (int)(mp.y - 2), 1.5f, RAYWHITE);
        if (mobs[i].hp < mobs[i].maxHp) {
            float w = 14.0f * (mobs[i].hp / mobs[i].maxHp);
            DrawRectangle((int)(mp.x - 7), (int)(mp.y - mobR - 5), (int)w, 2, RED);
        }
    }

    // Bots: their item sprite + a little thruster flicker. They fly,
    // so their shadow sits farther away and fainter.
    for (int i = 0; i < MAX_BOTS; i++) {
        if (!bots[i].active) continue;
        DrawEllipse((int)(bots[i].pos.x + worldShadowVec.x * 1.4f),
                    (int)(bots[i].pos.y + worldShadowVec.y * 1.4f),
                    6, 4, (Color){ 12, 18, 12, 28 });
        DrawItemSprite(ITEM_MINING_BOT, bots[i].pos.x - 7, bots[i].pos.y - 7, 14);
        if (((int)(GetTime() * 10) + i) % 2 == 0)
            DrawCircle((int)bots[i].pos.x, (int)(bots[i].pos.y + 8), 2, (Color){ 255, 170, 60, 200 });
    }

    // Projectiles as TRACERS: bullets are hot streaks of light,
    // slingshot stones are dark pellets with a bright rim (high
    // contrast against any ground).
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;
        Vector2 pp = projectiles[i].pos;
        Vector2 tail = Vector2Subtract(pp, Vector2Scale(projectiles[i].dir, 14.0f));
        if (projectiles[i].type == ITEM_BULLET) {
            DrawLineEx(tail, pp, 5.0f, (Color){ 255, 200, 80, 70 });    // glow
            DrawLineEx(tail, pp, 2.0f, (Color){ 255, 240, 170, 255 });  // core
            DrawCircleV(pp, 2.0f, RAYWHITE);
        } else {
            DrawLineEx(tail, pp, 3.0f, (Color){ 230, 230, 240, 60 });   // faint trail
            DrawCircleV(pp, 3.5f, (Color){ 36, 36, 44, 255 });          // dark stone
            DrawCircleLinesV(pp, 4.5f, RAYWHITE);                       // bright rim
        }
    }

    // Bombs blink faster as the fuse runs out.
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].active) continue;
        DrawItemSprite(ITEM_BOMB, bombs[i].pos.x - 10, bombs[i].pos.y - 10, 20);
        if (((int)(GetTime() * (bombs[i].fuse < 1.0f ? 12 : 4))) % 2 == 0)
            DrawCircleLinesV(bombs[i].pos, 14, RED);
    }

    // Laser beams + effects (explosions, muzzle flashes, sparks).
    for (int i = 0; i < MAX_BEAMS; i++) {
        if (beams[i].active) {
            DrawLineEx(beams[i].from, beams[i].to, 3.0f, (Color){ 255, 70, 70, 230 });
            DrawLineEx(beams[i].from, beams[i].to, 1.0f, RAYWHITE);
        }
    }
    for (int i = 0; i < MAX_EFFECTS; i++) {
        if (!effects[i].active) continue;
        float k = effects[i].age / effects[i].life;   // 0..1 over lifetime
        unsigned char a = (unsigned char)(200 * (1.0f - k));
        if (effects[i].kind == EFFECT_RING) {
            float r = effects[i].maxRadius * k;
            DrawCircleLinesV(effects[i].pos, r, (Color){ 255, 190, 80, a });
            DrawCircleV(effects[i].pos, r * 0.6f, (Color){ 255, 120, 40, (unsigned char)(a / 3) });
        } else if (effects[i].kind == EFFECT_FLASH) {
            // Muzzle flash: bright core that dies in a few frames.
            float r = effects[i].maxRadius * (1.0f - k * 0.5f);
            DrawCircleV(effects[i].pos, r, (Color){ 255, 240, 180, a });
            DrawCircleV(effects[i].pos, r * 0.5f, (Color){ 255, 255, 255, a });
        } else {   // EFFECT_SPARK — a puff of hit-confirm
            float r = effects[i].maxRadius * (0.4f + 0.6f * k);
            DrawCircleLinesV(effects[i].pos, r, (Color){ 255, 220, 140, a });
        }
    }
}

// ─── Minimap + alerts (screen space) ──────────────────────
static void EntitiesDrawMinimap(const Player *p) {
    WorldMinimapRefresh();
    int size = 176;
    int mx = GetScreenWidth() - size - 12, my = 12;

    DrawRectangle(mx - 3, my - 3, size + 6, size + 6, (Color){ 10, 10, 18, 220 });
    DrawTexturePro(worldMinimapTex,
                   (Rectangle){ 0, 0, (float)WORLD_SIZE, (float)WORLD_SIZE },
                   (Rectangle){ (float)mx, (float)my, (float)size, (float)size },
                   (Vector2){ 0, 0 }, 0, WHITE);
    DrawRectangleLines(mx - 3, my - 3, size + 6, size + 6, SKYBLUE);

    // Pips respect the fog: a mob you've never scouted stays hidden.
    float scale = (float)size / (WORLD_SIZE * TILE_SIZE);
    for (int i = 0; i < MAX_MOBS; i++) {          // mobs: red pips
        if (!mobs[i].active) continue;
        int tx = (int)(mobs[i].pos.x / TILE_SIZE), ty = (int)(mobs[i].pos.y / TILE_SIZE);
        if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) continue;
        if (!worldExplored[tx][ty]) continue;
        DrawRectangle(mx + (int)(mobs[i].pos.x * scale) - 1,
                      my + (int)(mobs[i].pos.y * scale) - 1, 2, 2, RED);
    }
    for (int i = 0; i < MAX_BOTS; i++) {          // bots: cyan pips (always yours)
        if (!bots[i].active) continue;
        DrawRectangle(mx + (int)(bots[i].pos.x * scale) - 1,
                      my + (int)(bots[i].pos.y * scale) - 1, 2, 2, SKYBLUE);
    }
    DrawRectangle(mx + (int)(p->pos.x * scale) - 2,   // you: white dot
                  my + (int)(p->pos.y * scale) - 2, 4, 4, RAYWHITE);

    // Threat readout — the polite version of Factorio's evolution %.
    int threat = (int)(EvolutionFactor() * 100.0f);
    DrawText(TextFormat("THREAT %d%%  MOBS %d", threat, MobCount()),
             mx, my + size + 8, 14, threat > 66 ? RED : (threat > 33 ? GOLD : GREEN));
}

// ─── Full map (hold G) ────────────────────────────────────
// The whole world at once, fogged where you haven't been — the
// zoomed-out "where am I, where are the nests" view.
static void EntitiesDrawFullMap(const Player *p) {
    WorldMinimapRefresh();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int size = (sw < sh ? sw : sh) - 70;
    int mx = (sw - size) / 2, my = (sh - size) / 2;

    DrawRectangle(0, 0, sw, sh, (Color){ 4, 6, 10, 235 });
    DrawTexturePro(worldMinimapTex,
                   (Rectangle){ 0, 0, (float)WORLD_SIZE, (float)WORLD_SIZE },
                   (Rectangle){ (float)mx, (float)my, (float)size, (float)size },
                   (Vector2){ 0, 0 }, 0, WHITE);
    DrawRectangleLines(mx - 2, my - 2, size + 4, size + 4, SKYBLUE);

    float scale = (float)size / (WORLD_SIZE * TILE_SIZE);
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!mobs[i].active) continue;
        int tx = (int)(mobs[i].pos.x / TILE_SIZE), ty = (int)(mobs[i].pos.y / TILE_SIZE);
        if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) continue;
        if (!worldExplored[tx][ty]) continue;
        DrawRectangle(mx + (int)(mobs[i].pos.x * scale) - 1,
                      my + (int)(mobs[i].pos.y * scale) - 1, 3, 3, RED);
    }
    for (int i = 0; i < MAX_BOTS; i++) {
        if (!bots[i].active) continue;
        DrawRectangle(mx + (int)(bots[i].pos.x * scale) - 1,
                      my + (int)(bots[i].pos.y * scale) - 1, 3, 3, SKYBLUE);
    }
    // You, pulsing so you can find yourself at a glance.
    float pulse = 3.0f + 2.0f * sinf((float)GetTime() * 5.0f);
    DrawCircleLines(mx + (int)(p->pos.x * scale), my + (int)(p->pos.y * scale),
                    pulse + 3, RAYWHITE);
    DrawRectangle(mx + (int)(p->pos.x * scale) - 2,
                  my + (int)(p->pos.y * scale) - 2, 4, 4, RAYWHITE);

    DrawText("WORLD MAP", mx, my - 24, 18, RAYWHITE);
    DrawText(TextFormat("THREAT %d%%", (int)(EvolutionFactor() * 100)),
             mx, my + size + 8, 16, GRAY);
}

// ─── Save / load ──────────────────────────────────────────
// One fwrite per pool — the payoff of fixed-size arrays.
static bool EntitiesWrite(FILE *f) {
    if (fwrite(&entGameTime, sizeof(entGameTime), 1, f) != 1) return false;
    if (fwrite(machines, sizeof(machines), 1, f) != 1) return false;
    if (fwrite(mobs,     sizeof(mobs),     1, f) != 1) return false;
    if (fwrite(bots,     sizeof(bots),     1, f) != 1) return false;
    return true;
}

static bool EntitiesRead(FILE *f) {
    if (fread(&entGameTime, sizeof(entGameTime), 1, f) != 1) return false;
    if (fread(machines, sizeof(machines), 1, f) != 1) return false;
    if (fread(mobs,     sizeof(mobs),     1, f) != 1) return false;
    if (fread(bots,     sizeof(bots),     1, f) != 1) return false;

    // Untrusted data: clear the transient pools, rebuild the machine
    // index from what we loaded, clamp everything into the world.
    memset(projectiles, 0, sizeof(projectiles));
    memset(bombs, 0, sizeof(bombs));
    memset(effects, 0, sizeof(effects));
    memset(beams, 0, sizeof(beams));
    for (int x = 0; x < WORLD_SIZE; x++)
        for (int y = 0; y < WORLD_SIZE; y++) machineIndex[x][y] = -1;
    for (int i = 0; i < MAX_MACHINES; i++) {
        Machine *m = &machines[i];
        if (!m->active) continue;
        if (m->x < 0 || m->x >= WORLD_SIZE || m->y < 0 || m->y >= WORLD_SIZE) {
            m->active = false;
            continue;
        }
        m->dir &= 3;
        machineIndex[m->x][m->y] = (short)i;
    }
    float worldMax = (float)(WORLD_SIZE * TILE_SIZE);
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!mobs[i].active) continue;
        mobs[i].pos.x = Clamp(mobs[i].pos.x, 0, worldMax);
        mobs[i].pos.y = Clamp(mobs[i].pos.y, 0, worldMax);
        if (mobs[i].hp <= 0 || mobs[i].hp > 10000) mobs[i].active = false;
    }
    for (int i = 0; i < MAX_BOTS; i++) {
        if (!bots[i].active) continue;
        bots[i].pos.x = Clamp(bots[i].pos.x, 0, worldMax);
        bots[i].pos.y = Clamp(bots[i].pos.y, 0, worldMax);
        for (int d = 0; d < ITEM_COUNT; d++)
            if (bots[i].inv[d] < 0) bots[i].inv[d] = 0;
    }
    if (entGameTime < 0) entGameTime = 0;
    entShake = 0;
    return true;
}

#endif // ENTITIES_H
