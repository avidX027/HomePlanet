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
// tile grid itself only knows the type). slots/counts is a tiny
// 4-slot inventory used by chests, drills, conveyors, inserters.
#define MACHINE_SLOTS 4
#define MAX_MACHINES  1024
typedef struct {
    bool     active;
    int      x, y;              // which tile this state belongs to
    TileType type;
    int      dir;               // conveyor/inserter facing (see DIR_DX)
    float    timer;             // generic cooldown (spawner/drill/turret...)
    ItemID   slots[MACHINE_SLOTS];
    int      counts[MACHINE_SLOTS];
    int      ammo;              // gun turrets: loaded bullets
    Vector2  beamTo;            // laser turrets: where the last zap went
    float    beamTtl;           // ...and how long to keep drawing it
} Machine;
static Machine machines[MAX_MACHINES];

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
}

static Machine *MachineAt(int x, int y) {
    if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) return NULL;
    short i = machineIndex[x][y];
    return (i >= 0) ? &machines[i] : NULL;
}

static Machine *AddMachineAt(int x, int y, TileType type, int dir) {
    for (int i = 0; i < MAX_MACHINES; i++) {
        if (!machines[i].active) {
            memset(&machines[i], 0, sizeof(Machine));
            machines[i].active = true;
            machines[i].x = x;  machines[i].y = y;
            machines[i].type = type;
            machines[i].dir = dir;
            machineIndex[x][y] = (short)i;
            return &machines[i];
        }
    }
    return NULL;   // pool full: the tile exists but stays inert
}

// Try to stuff items into a machine's 4 slots. Returns how many fit.
static int MachineAddItem(Machine *m, ItemID id, int amount) {
    if (m == NULL || id == ITEM_NONE || amount <= 0) return 0;
    for (int s = 0; s < MACHINE_SLOTS; s++) {          // merge first
        if (m->slots[s] == id && m->counts[s] > 0) { m->counts[s] += amount; return amount; }
    }
    for (int s = 0; s < MACHINE_SLOTS; s++) {          // then empty slot
        if (m->slots[s] == ITEM_NONE || m->counts[s] <= 0) {
            m->slots[s] = id; m->counts[s] = amount; return amount;
        }
    }
    return 0;
}

// Pop ONE item out of the first non-empty slot (inserters grab one
// at a time, like their Factorio ancestors).
static ItemID MachineTakeItem(Machine *m) {
    if (m == NULL) return ITEM_NONE;
    for (int s = 0; s < MACHINE_SLOTS; s++) {
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

// After WorldInit (or a load with no entity data): every special
// tile that needs state gets a machine. Today that's spawners.
static void EntitiesRegisterWorldMachines(void) {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            if (world[x][y].type == TILE_SPAWNER && MachineAt(x, y) == NULL) {
                Machine *m = AddMachineAt(x, y, TILE_SPAWNER, 0);
                if (m != NULL) m->timer = (float)GetRandomValue(2, 12);
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
            if (WorldDamageTile(tx, ty, 150.0f)) {
                RemoveMachineAt(tx, ty, NULL);
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
        // turret behind your wall doesn't shoot your wall.
        if (proj->ignoreTiles) continue;

        int tx = (int)(proj->pos.x / TILE_SIZE);
        int ty = (int)(proj->pos.y / TILE_SIZE);
        if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) {
            proj->active = false;
            continue;
        }
        TileType hitType = world[tx][ty].type;
        if (!TILES[hitType].breakable) continue;
        // Ankle-high quarter pebbles don't stop bullets — shots sail
        // over them the same way your feet do.
        if (hitType == TILE_ROCK && (world[tx][ty].variant & 3) == 3) continue;

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
        if (m->state == MOB_IDLE) {
            if (Vector2Distance(m->pos, p->pos) < 230.0f) {
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
        // along walls instead of sticking to them.
        Vector2 d = Vector2Subtract(m->target, m->pos);
        if (Vector2Length(d) > 6.0f) {
            Vector2 dir = Vector2Normalize(d);
            Vector2 nx = { m->pos.x + dir.x * speed * dt, m->pos.y };
            Vector2 ny = { m->pos.x, m->pos.y + dir.y * speed * dt };
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
            // Gnaws the four neighbors; broken rocks/trees drop into
            // the drill's own slots (inserters or you take it out).
            m->timer -= dt;
            if (m->timer > 0) break;
            m->timer = TUNE.drillInterval;
            int order = GetRandomValue(0, 3);
            for (int k = 0; k < 4; k++) {
                int dir = (order + k) % 4;
                int nx = m->x + DIR_DX[dir], ny = m->y + DIR_DY[dir];
                if (nx < 0 || nx >= WORLD_SIZE || ny < 0 || ny >= WORLD_SIZE) continue;
                TileType t = world[nx][ny].type;
                if (t != TILE_ROCK && t != TILE_TREE) continue;
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

        case TILE_CONVEYOR: {
            // Push our item one tile along the belt direction.
            m->timer -= dt;
            if (m->timer > 0) break;
            m->timer = TUNE.conveyorInterval;
            if (m->slots[0] == ITEM_NONE || m->counts[0] <= 0) break;
            Machine *dest = MachineAt(m->x + DIR_DX[m->dir], m->y + DIR_DY[m->dir]);
            if (dest == NULL) break;
            ItemID id = m->slots[0];
            bool moved = false;
            if (dest->type == TILE_CONVEYOR) {
                if (dest->slots[0] == ITEM_NONE || dest->counts[0] <= 0) {
                    dest->slots[0] = id; dest->counts[0] = 1; moved = true;
                }
            } else if (dest->type == TILE_CHEST) {
                moved = MachineAddItem(dest, id, 1) > 0;
            } else if (dest->type == TILE_TURRET && id == ITEM_BULLET) {
                dest->ammo += 1; moved = true;    // belt-fed turrets!
            }
            if (moved && --m->counts[0] <= 0) { m->slots[0] = ITEM_NONE; m->counts[0] = 0; }
            break;
        }

        case TILE_INSERTER: {
            // The robotic hand: grab ONE item from the tile BEHIND,
            // drop it on the tile IN FRONT. Holds the item (slot 0)
            // until the destination has room.
            m->timer -= dt;
            if (m->timer > 0) break;
            m->timer = TUNE.inserterInterval;
            if (m->slots[0] == ITEM_NONE) {   // hand empty → try to grab
                Machine *src = MachineAt(m->x - DIR_DX[m->dir], m->y - DIR_DY[m->dir]);
                if (src != NULL && (src->type == TILE_CHEST || src->type == TILE_DRILL ||
                                    src->type == TILE_CONVEYOR)) {
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
                    else if (dest->type == TILE_CONVEYOR &&
                             (dest->slots[0] == ITEM_NONE || dest->counts[0] <= 0)) {
                        dest->slots[0] = id; dest->counts[0] = 1; placed = true;
                    }
                    else if (dest->type == TILE_TURRET && id == ITEM_BULLET) {
                        dest->ammo += 1; placed = true;
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
                    if (t != TILE_ROCK && t != TILE_TREE) continue;
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
            if (t != TILE_ROCK && t != TILE_TREE) { b->state = BOT_FIND; continue; }
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

// One call from main.c per gameplay frame.
static void EntitiesUpdate(float dt, Player *p) {
    entGameTime += dt;
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
            if (t == TILE_DOOR || t == TILE_CHEST || t == TILE_DRILL ||
                t == TILE_CONVEYOR || t == TILE_INSERTER || t == TILE_TURRET ||
                t == TILE_LASER_TURRET || t == TILE_RESEARCH) {
                ItemID icon = ItemThatPlaces(t);
                if (icon != ITEM_NONE) DrawItemSprite(icon, px + 2, py + 2, TILE_SIZE - 4);
            }

            Machine *m = MachineAt(x, y);
            if (m == NULL) continue;
            Vector2 center = { px + TILE_SIZE / 2.0f, py + TILE_SIZE / 2.0f };

            if (t == TILE_CONVEYOR || t == TILE_INSERTER) {
                // Direction arrow.
                Vector2 tip  = { center.x + DIR_DX[m->dir] * 10.0f, center.y + DIR_DY[m->dir] * 10.0f };
                Vector2 left = { center.x - DIR_DY[m->dir] * 5.0f,  center.y + DIR_DX[m->dir] * 5.0f };
                Vector2 rght = { center.x + DIR_DY[m->dir] * 5.0f,  center.y - DIR_DX[m->dir] * 5.0f };
                DrawTriangle(tip, left, rght, (Color){ 255, 255, 255, 160 });
                DrawTriangle(tip, rght, left, (Color){ 255, 255, 255, 160 });
            }
            if ((t == TILE_CONVEYOR || t == TILE_INSERTER) &&
                m->slots[0] != ITEM_NONE && m->counts[0] > 0) {
                DrawItemSprite(m->slots[0], center.x - 6, center.y - 6, 12);  // cargo
            }
            if (t == TILE_TURRET) {
                DrawText(TextFormat("%d", m->ammo), (int)px + 3, (int)py + 2, 10,
                         m->ammo > 0 ? GOLD : RED);
            }
        }
    }

    // Mobs: blobby circles that get bigger and angrier with evolution.
    float evo = EvolutionFactor();
    float mobR = 6.5f + 3.0f * evo;
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!mobs[i].active) continue;
        Vector2 mp = mobs[i].pos;
        Color body = (mobs[i].state == MOB_RAID) ? (Color){ 210, 60, 50, 255 }
                                                 : (Color){ 160, 60, 130, 255 };
        DrawCircleV(mp, mobR, body);
        DrawCircleLinesV(mp, mobR + 1, (Color){ 20, 10, 20, 255 });
        DrawCircle((int)(mp.x - 2), (int)(mp.y - 2), 1.5f, RAYWHITE);   // eyes
        DrawCircle((int)(mp.x + 2), (int)(mp.y - 2), 1.5f, RAYWHITE);
        if (mobs[i].hp < mobs[i].maxHp) {
            float w = 14.0f * (mobs[i].hp / mobs[i].maxHp);
            DrawRectangle((int)(mp.x - 7), (int)(mp.y - mobR - 5), (int)w, 2, RED);
        }
    }

    // Bots: their item sprite + a little thruster flicker.
    for (int i = 0; i < MAX_BOTS; i++) {
        if (!bots[i].active) continue;
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
    DrawText("[G] map", mx + size - 52, my + size + 8, 14, GRAY);
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

    DrawText("WORLD MAP — fog hides what you haven't scouted",
             mx, my - 24, 18, RAYWHITE);
    DrawText(TextFormat("THREAT %d%%   release [G] to return", (int)(EvolutionFactor() * 100)),
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
