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
    float    workHours;         // SECONDS SPENT WORKING — see wear, below
    ItemID   slots[MACHINE_SLOTS];
    int      counts[MACHINE_SLOTS];
    int      ammo;              // gun turrets: loaded bullets
    int      coal;              // fuel-burners: coal in the hopper
    float    fuel;              // ...and seconds left on the current lump
    float    beltProgress;      // 0..1 slide of the item along a belt
    Vector2  beamTo;            // laser turrets: where the last zap went
    float    beamTtl;           // ...and how long to keep drawing it
    // ── Sorting (inserters and splitters) ──
    // ITEM_NONE = "no filter, handle anything". Otherwise an inserter
    // only picks THIS item up, and a splitter routes it left while
    // everything else goes right. Set from the item-grid picker.
    ItemID   filter;
    int      splitToggle;       // splitters: which side gets the next item
    // ── Underground belts ──
    // The two mouths of a tunnel each remember where the other one
    // is. That single pair of coordinates is what makes the run's
    // length irrelevant — cargo doesn't walk the gap, it's handed
    // straight across.
    int      linkX, linkY;      // partner mouth's tile (-1 = unlinked)
} Machine;
static Machine machines[MAX_MACHINES];

// Which machine's UI panel is open? -1 = none. Lives here because
// both main.c (input) and ui.h (drawing) need it.
static int machineUiX = -1, machineUiY = -1;

// Is the FILTER PICKER open on top of that panel? It's the grid of
// every item in the game — click one and it becomes the machine's
// filter. Same shared-state trick as machineUiX: main.c reads the
// clicks, ui.h draws the grid.
static bool machineFilterPickerOpen = false;

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
// Now four species (gamedata.h's MOBS_INFO holds their stats), and
// the spiders among them RANGE — they don't sit politely at their
// nest waiting to be farmed. A mob can also be carrying something it
// tore out of one of your chests, which it drops when it dies.
#define MAX_MOBS 160
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
    unsigned char kind;  // which MOBS_INFO row it is
    ItemID  loot;        // stolen goods, dropped when it dies
    int     lootN;
    float   chirp;       // countdown to its next noise
    float   legPhase;    // spider gait animation
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
enum { EFFECT_RING = 0, EFFECT_FLASH, EFFECT_SPARK, EFFECT_DEBRIS };
#define MAX_EFFECTS 64
typedef struct {
    bool active; int kind; Vector2 pos; float age, life, maxRadius;
    Color tint;          // debris takes the colour of whatever broke
    unsigned int seed;   // ...and its chunks fly in a fixed direction
} Effect;
static Effect effects[MAX_EFFECTS];

#define MAX_BEAMS 16
typedef struct { bool active; Vector2 from, to; float ttl; } Beam;
static Beam beams[MAX_BEAMS];

// ─── Items lying on the ground ────────────────────────────
// Factorio's floor. An inserter with nowhere to hand its cargo sets
// it down; an inserter with nothing behind it picks the floor back
// up; you drop things with X and hoover them up by walking over
// them. Piles of the same item MERGE, so an arm unloading onto one
// tile forever stays ONE entry instead of eating the whole pool.
#define MAX_GROUND_ITEMS    512
#define GROUND_MERGE_DIST   9.0f    // px — closer than this is the same pile
#define GROUND_PICKUP_DIST  16.0f   // px — walk this close and it's yours
#define GROUND_PICKUP_DELAY 0.8f    // s — grace, so you don't re-grab your own drop
typedef struct {
    bool    active;
    Vector2 pos;
    ItemID  id;
    int     count;
    float   age;
} GroundItem;
static GroundItem groundItems[MAX_GROUND_ITEMS];

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
    memset(groundItems, 0, sizeof(groundItems));
    for (int x = 0; x < WORLD_SIZE; x++)
        for (int y = 0; y < WORLD_SIZE; y++) machineIndex[x][y] = -1;
    entGameTime = 0;
    entShake    = 0;
    machineUiX  = machineUiY = -1;
    machineFilterPickerOpen = false;
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
    machines[slot].filter = ITEM_NONE;
    machines[slot].linkX = machines[slot].linkY = -1;   // 0,0 is a real tile
    machineIndex[x][y] = (short)slot;
    return &machines[slot];
}

// Add a record for a tile that just appeared in the world. Wraps
// AddMachineAt with the one piece of per-type setup that placement
// needs: a nest gets a staggered first tick, so a row of them
// doesn't breed in lockstep (and a freshly placed one doesn't spit
// a mob out on the very next frame).
static Machine *AddMachineRecordAt(int x, int y, TileType t, int dir) {
    Machine *m = AddMachineAt(x, y, t, dir);
    if (m != NULL && t == TILE_SPAWNER) m->timer = (float)GetRandomValue(2, 12);
    return m;
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

// ─── One rule for "will you take this?" ───────────────────
// Belts, arms, drills and splitters all used to carry their OWN copy
// of "a chest takes anything, a turret takes bullets, a belt takes
// one more if there's room" — four copies that had to be kept in
// step by hand. This is that rule, once. Every hand-off in the game
// goes through it, which is why the splitter and the underground
// belt work with everything the moment they exist.
static bool MachineAcceptItem(Machine *dest, ItemID id) {
    if (dest == NULL || id <= ITEM_NONE || id >= ITEM_COUNT) return false;
    if (TileIsBeltLike(dest->type)) {
        // A belt tile carries a small STACK of ONE item type. Anything
        // it can't take stays where it is, so lines back up like real
        // conveyors instead of quietly deleting cargo.
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
    if (dest->type == TILE_CHEST) return MachineAddItem(dest, id, 1) > 0;
    if (dest->type == TILE_TURRET && id == ITEM_BULLET) { dest->ammo++; return true; }
    if (TileNeedsFuel(dest->type) && id == ITEM_COAL) return MachineAddCoal(dest);
    return false;
}

// Push ONE item into whatever machine sits at (x,y). Used by the
// drill to unload onto the belt in front of it — the reason a drill
// no longer needs an inserter babysitting it.
static bool MachinePushInto(int x, int y, ItemID id) {
    return MachineAcceptItem(MachineAt(x, y), id);
}

// ─── Where does this piece of belt furniture send things? ─
// A belt hands cargo to the tile it FACES. A tunnel entrance hands it
// to its partner mouth, however far away that is — the whole trick of
// the underground belt is that this one lookup skips the distance.
static Machine *BeltOutputTarget(const Machine *m) {
    if (m == NULL) return NULL;
    if (m->type == TILE_TUNNEL_IN) return MachineAt(m->linkX, m->linkY);
    return MachineAt(m->x + DIR_DX[m->dir], m->y + DIR_DY[m->dir]);
}

// A splitter's two mouths: LEFT and RIGHT of its facing. Feed it from
// behind and one lane becomes two. (`side` 0 = left, 1 = right.)
static int SplitterOutDir(const Machine *m, int side) {
    return (side == 0) ? ((m->dir + 3) & 3) : ((m->dir + 1) & 3);
}

static Machine *SplitterOutput(const Machine *m, int side) {
    int d = SplitterOutDir(m, side);
    return MachineAt(m->x + DIR_DX[d], m->y + DIR_DY[d]);
}

// How long is this tunnel, end to end (in tiles)? That number is
// what the run COST to lay, so it's also what mining a mouth pays
// back. An unlinked mouth is worth its own single tile.
static int TunnelLength(const Machine *m) {
    if (m == NULL || !TileIsTunnel(m->type)) return 0;
    if (m->linkX < 0 || m->linkY < 0) return 1;
    int dx = m->linkX - m->x, dy = m->linkY - m->y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx + dy + 1;
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

// Pop ONE item of a SPECIFIC kind. This is what lets an arm hunt for
// coal in a chest full of ore instead of grabbing the first thing it
// touches and jamming with cargo the destination won't accept.
static ItemID MachineTakeSpecific(Machine *m, ItemID want) {
    if (m == NULL || want <= ITEM_NONE) return ITEM_NONE;
    for (int s = 0; s < MachineSlotCount(m); s++) {
        if (m->slots[s] != want || m->counts[s] <= 0) continue;
        if (--m->counts[s] <= 0) { m->slots[s] = ITEM_NONE; m->counts[s] = 0; }
        return want;
    }
    return ITEM_NONE;
}

// ─── The floor ────────────────────────────────────────────
// Set items down at a world position, merging into a nearby pile of
// the same kind first. Returns false only when the pool is full and
// nothing could merge — in which case the caller keeps holding them.
static bool DropItemAt(Vector2 pos, ItemID id, int count) {
    if (id <= ITEM_NONE || id >= ITEM_COUNT || count <= 0) return false;
    for (int i = 0; i < MAX_GROUND_ITEMS; i++) {
        if (!groundItems[i].active || groundItems[i].id != id) continue;
        if (Vector2Distance(groundItems[i].pos, pos) > GROUND_MERGE_DIST) continue;
        groundItems[i].count += count;
        groundItems[i].age = 0;
        return true;
    }
    for (int i = 0; i < MAX_GROUND_ITEMS; i++) {
        if (groundItems[i].active) continue;
        groundItems[i] = (GroundItem){ true, pos, id, count, 0 };
        return true;
    }
    return false;
}

// Take ONE item off whatever is lying on tile (tx,ty). `want` of
// ITEM_NONE means "anything". This is the arm's pickup.
static ItemID GroundTakeOneAt(int tx, int ty, ItemID want) {
    for (int i = 0; i < MAX_GROUND_ITEMS; i++) {
        GroundItem *g = &groundItems[i];
        if (!g->active || g->count <= 0) continue;
        if (want != ITEM_NONE && g->id != want) continue;
        if ((int)(g->pos.x / TILE_SIZE) != tx || (int)(g->pos.y / TILE_SIZE) != ty) continue;
        ItemID id = g->id;
        if (--g->count <= 0) g->active = false;
        return id;
    }
    return ITEM_NONE;
}

// Can loose items sit on this tile? Open ground only — nothing gets
// dropped inside a rock or a wall.
static bool GroundTileFree(int tx, int ty) {
    if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) return false;
    return TILES[world[tx][ty].type].walkable;
}

// Sweep everything within `radius` of `at` into the player's pockets.
// `minAge` keeps a stack you just threw down from leaping straight
// back into your hands.
static void GroundPickupNear(Player *p, Vector2 at, float radius, float minAge) {
    for (int i = 0; i < MAX_GROUND_ITEMS; i++) {
        GroundItem *g = &groundItems[i];
        if (!g->active || g->count <= 0 || g->age < minAge) continue;
        if (Vector2Distance(g->pos, at) > radius) continue;
        // Take only what FITS. A full backpack leaves the rest of the
        // pile lying there instead of quietly deleting it.
        int before = p->inventory[g->id];
        PlayerGiveItem(p, g->id, g->count);
        int took = p->inventory[g->id] - before;
        // Only when something actually LANDED — a full backpack
        // standing over a pile shouldn't click at you forever.
        if (took > 0) SfxPlay(SFX_PICKUP, 0.35f, 2.0f);
        if (took >= g->count) g->active = false;
        else if (took > 0)    g->count -= took;
    }
}

// ─── An arm that feeds itself ─────────────────────────────
// Look for one lump of coal: first the tile the arm draws FROM, then
// its four neighbours — in a chest, on a belt, in a drill's output,
// or lying on the floor. One lump is all it takes; COAL_FUEL_SECONDS
// later it comes back for another. This is what makes a belt of coal
// running past a row of arms keep the whole row alive.
static bool InserterSelfFuel(Machine *m) {
    if (m == NULL || m->coal >= MACHINE_FUEL_MAX) return false;
    for (int k = -1; k < 4; k++) {
        int nx = (k < 0) ? m->x - DIR_DX[m->dir] : m->x + DIR_DX[k];
        int ny = (k < 0) ? m->y - DIR_DY[m->dir] : m->y + DIR_DY[k];
        Machine *n = MachineAt(nx, ny);
        if (n != NULL && n != m &&
            (n->type == TILE_CHEST || n->type == TILE_DRILL || TileIsBeltLike(n->type))) {
            if (MachineTakeSpecific(n, ITEM_COAL) != ITEM_NONE) { m->coal++; return true; }
        }
        if (GroundTakeOneAt(nx, ny, ITEM_COAL) != ITEM_NONE) { m->coal++; return true; }
    }
    return false;
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

// Everything inside a machine, thrown onto the floor around it. This
// is what happens when something is DESTROYED rather than mined: a
// chest the mobs tore open spills its contents across the ground
// instead of quietly deleting a thousand plates.
static void MachineDropContents(Machine *m) {
    if (m == NULL) return;
    Vector2 at = { (m->x + 0.5f) * TILE_SIZE, (m->y + 0.5f) * TILE_SIZE };
    for (int s = 0; s < MACHINE_SLOTS; s++) {
        if (m->slots[s] != ITEM_NONE && m->counts[s] > 0) {
            Vector2 spill = { at.x + GetRandomValue(-9, 9), at.y + GetRandomValue(-9, 9) };
            DropItemAt(spill, m->slots[s], m->counts[s]);
        }
        m->slots[s] = ITEM_NONE; m->counts[s] = 0;
    }
    if (m->ammo > 0) { DropItemAt(at, ITEM_BULLET, m->ammo); m->ammo = 0; }
    if (m->coal > 0) { DropItemAt(at, ITEM_COAL,   m->coal); m->coal = 0; }
}

// Remove the machine on a tile (it broke). If `giveTo` is non-NULL
// the contents are salvaged straight into that player's inventory
// (you mined it); otherwise they hit the FLOOR, where you — or the
// thing that broke it — can pick them back up.
//
// A TUNNEL is one object with two ends: break either mouth and the
// whole run comes up, refunding every belt it cost. Half a tunnel is
// not a thing you can own, so it's not a state we allow to exist.
// The plain half: empty one machine out and free its record.
static void FreeMachineRecord(Machine *m, Player *giveTo) {
    if (m == NULL || !m->active) return;
    if (giveTo != NULL) MachineGiveContentsTo(m, giveTo);
    else                MachineDropContents(m);
    if (WorldInBounds(m->x, m->y)) machineIndex[m->x][m->y] = -1;
    m->active = false;
}

static void RemoveMachineAt(int x, int y, Player *giveTo) {
    Machine *m = MachineAt(x, y);
    if (m == NULL) return;

    if (TileIsTunnel(m->type)) {
        // Pay the run back ONCE, from whichever end broke, then take
        // the far mouth out too — no recursion, no double refund.
        int px = m->linkX, py = m->linkY;
        int refund = TunnelLength(m);
        Vector2 at = { (x + 0.5f) * TILE_SIZE, (y + 0.5f) * TILE_SIZE };
        if (giveTo != NULL) PlayerGiveItem(giveTo, ITEM_TUNNEL, refund);
        else                DropItemAt(at, ITEM_TUNNEL, refund);

        if (px >= 0 && py >= 0 && WorldInBounds(px, py) &&
            TileIsTunnel(world[px][py].type)) {
            FreeMachineRecord(MachineAt(px, py), giveTo);   // cargo only
            WorldSetTile(px, py, TILE_GRASS);
        }
    }

    FreeMachineRecord(m, giveTo);
}

// ─── The map painter's brush ──────────────────────────────
// Painting is NOT mining: a tile that gets painted over doesn't
// break, drop, or refund anything. It's an editor, so the old tile
// simply stops existing — which is why this can't reuse
// RemoveMachineAt (that one drops the contents on the floor, and a
// wide brush over a chest farm would bury the map in loose items).
static void DevClearMachineSilently(int x, int y) {
    Machine *m = MachineAt(x, y);
    if (m == NULL) return;
    // A tunnel is a PAIR. Painting over one mouth has to take the
    // far one with it, or the survivor keeps a link pointing at a
    // tile that is no longer a tunnel.
    if (TileIsTunnel(m->type)) {
        int px = m->linkX, py = m->linkY;
        if (px >= 0 && py >= 0 && WorldInBounds(px, py) &&
            TileIsTunnel(world[px][py].type)) {
            Machine *far = MachineAt(px, py);
            if (far != NULL) { machineIndex[px][py] = -1; far->active = false; }
            WorldSetTile(px, py, TILE_GRASS);
        }
    }
    if (WorldInBounds(m->x, m->y)) machineIndex[m->x][m->y] = -1;
    m->active = false;
}

// Lay one tile down, keeping the machine records honest in both
// directions: whatever was here loses its record, and whatever
// arrives gets one if its type needs it.
static void DevPaintTile(int x, int y, TileType t) {
    if (!WorldInBounds(x, y)) return;
    if (t < 0 || t >= TILE_COUNT) return;
    // Repainting a machine onto itself is a real operation (it
    // re-facings the record), but repainting plain ground onto
    // itself is a no-op worth skipping — a held brush hits the same
    // tile every frame.
    if (world[x][y].type == t && !TileNeedsRecord(t)) return;

    DevClearMachineSilently(x, y);
    WorldSetTile(x, y, t);
    if (TileNeedsRecord(t) && AddMachineRecordAt(x, y, t, devBrushDir) == NULL) {
        // Machine pool full — refuse rather than leave a machine tile
        // with no record behind it, which draws as a dead stub.
        WorldSetTile(x, y, TILE_GRASS);
    }
}

// The brush proper: a square of radius r, clipped to the map.
static void DevPaintBrush(int cx, int cy, int radius, TileType t) {
    if (radius < 0) radius = 0;
    if (radius > DEV_BRUSH_MAX) radius = DEV_BRUSH_MAX;
    for (int x = cx - radius; x <= cx + radius; x++) {
        for (int y = cy - radius; y <= cy + radius; y++) {
            DevPaintTile(x, y, t);
        }
    }
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
            if (TileNeedsRecord(t) && MachineAt(x, y) == NULL) {
                AddMachineRecordAt(x, y, t, 0);   // stagger is handled in there
            }
        }
    }
}

// ─── Effects ──────────────────────────────────────────────
static void AddEffectTinted(int kind, Vector2 pos, float maxRadius, float life, Color tint) {
    for (int i = 0; i < MAX_EFFECTS; i++) {
        if (!effects[i].active) {
            effects[i] = (Effect){ true, kind, pos, 0, life, maxRadius, tint,
                                   (unsigned int)GetRandomValue(1, 100000) };
            return;
        }
    }
}

static void AddEffect(int kind, Vector2 pos, float maxRadius, float life) {
    AddEffectTinted(kind, pos, maxRadius, life, (Color){ 255, 190, 80, 255 });
}

// ─── Something broke: make the mess ───────────────────────
// world.h records every destroyed tile (whoever destroyed it) and
// this drains that list once a frame. One place decides what a
// dying tree looks like, and the player, the drills, the bots, the
// bullets and the mobs' teeth all get it for free.
static void DrainWorldBreakEvents(void) {
    for (int i = 0; i < worldBreakCount; i++) {
        WorldBreakEvent *e = &worldBreaks[i];
        Vector2 at = { (e->x + 0.5f) * TILE_SIZE, (e->y + 0.5f) * TILE_SIZE };
        Color tint = TILES[e->type].color;
        float radius = 15.0f;
        float life = 0.42f;
        if (e->type == TILE_TREE) {
            // A felled tree throws leaves as well as splinters, and
            // the burst is bigger — you should SEE a tree come down.
            tint = (Color){ 52, 120, 58, 255 };
            radius = 24.0f;
            life = 0.70f;
            AddEffectTinted(EFFECT_DEBRIS, at, 17.0f, 0.55f, (Color){ 96, 62, 34, 255 });
        }
        AddEffectTinted(EFFECT_DEBRIS, at, radius, life, tint);
    }
    worldBreakCount = 0;
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

static void SpawnMobKindAt(Vector2 pos, MobKind kind) {
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!mobs[i].active) {
            float evo = EvolutionFactor();
            const MobInfo *info = &MOBS_INFO[kind];
            memset(&mobs[i], 0, sizeof(Mob));
            mobs[i].active = true;
            mobs[i].kind = (unsigned char)kind;
            mobs[i].pos = pos;
            mobs[i].home = pos;
            mobs[i].target = pos;
            // 1x → 3x health over the evolution curve, times the
            // species multiplier: a late broodmother is a wall.
            mobs[i].maxHp = TUNE.mobHp * info->hp * (1.0f + 2.0f * evo);
            mobs[i].hp = mobs[i].maxHp;
            mobs[i].state = MOB_IDLE;
            mobs[i].retarget = 0;
            mobs[i].chirp = (float)GetRandomValue(2, 14);
            mobs[i].legPhase = (float)GetRandomValue(0, 628) / 100.0f;
            return;
        }
    }
}

// The old one-argument version: pick a species by the current
// evolution and spawn that.
static void SpawnMobAt(Vector2 pos) {
    SpawnMobKindAt(pos, RollMobKind(EvolutionFactor()));
}

static void MobDamage(Mob *m, float dmg) {
    m->hp -= dmg;
    if (m->hp <= 0) {
        m->active = false;
        const MobInfo *info = &MOBS_INFO[m->kind];
        AddEffectTinted(EFFECT_DEBRIS, m->pos, 15.0f, 0.4f, info->body);
        AddEffect(EFFECT_RING, m->pos, 16, 0.25f);
        SfxPlayAt(SFX_MOB_DIE, m->pos, worldListener, 0.5f);
        // They drop what they're made of — and whatever they stole.
        int n = GetRandomValue(info->dropMin, info->dropMax);
        if (info->drops != ITEM_NONE && n > 0) DropItemAt(m->pos, info->drops, n);
        if (GetRandomValue(1, 100) <= (m->kind == MOB_BROODMOTHER ? 100 : 8))
            DropItemAt(m->pos, ITEM_GEM, 1);
        if (m->loot != ITEM_NONE && m->lootN > 0) {
            DropItemAt(m->pos, m->loot, m->lootN);      // your stuff, back
            m->loot = ITEM_NONE; m->lootN = 0;
        }
    } else {
        AddEffect(EFFECT_SPARK, m->pos, 7, 0.12f);   // hit feedback
        SfxPlayAt(SFX_MOB_HIT, m->pos, worldListener, 0.35f);
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
    SfxPlayAt(SFX_EXPLODE, pos, worldListener, 1.0f);   // ...and HEARD
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
            SfxPlayAt(SFX_BOMB_ARM, pos, worldListener, 0.7f);
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
// One mob spotting you is now everyone's business: it shrieks, and
// every native within earshot drops what it was doing and converges.
// This is what turns "a mob" into "a swarm" — and what makes walking
// into the waste without a gun a death sentence.
static void MobCallForHelp(Vector2 at, float radius) {
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!mobs[i].active || mobs[i].state == MOB_RAID) continue;
        if (Vector2Distance(mobs[i].pos, at) > radius) continue;
        mobs[i].state = MOB_RAID;
        mobs[i].retarget = 0;
    }
}

static void UpdateMobs(float dt, Player *p) {
    float evo = EvolutionFactor();
    float baseSpeed = TUNE.mobSpeed * (1.0f + 0.5f * evo);

    for (int i = 0; i < MAX_MOBS; i++) {
        Mob *m = &mobs[i];
        if (!m->active) continue;
        const MobInfo *info = &MOBS_INFO[m->kind];

        m->retarget -= dt;
        if (m->rage > 0) m->rage -= dt;
        m->legPhase += dt * (info->speed * 7.0f);

        // Idle chatter. You hear them before you see them, which is
        // the only warning you get out in the dark.
        m->chirp -= dt;
        if (m->chirp <= 0) {
            m->chirp = (float)GetRandomValue(4, 16);
            if (Vector2Distance(m->pos, p->pos) < 620.0f)
                SfxPlayAt(SFX_MOB_CHIRP, m->pos, worldListener, 0.30f);
        }

        if (m->state == MOB_IDLE) {
            // Nest guards get more alert as evolution rises: early
            // game you can skirt a patch; late game they notice you
            // from three times as far. The spiders are hunters — they
            // see you from much farther out than the crawlers do.
            float aggroRange = (70.0f + 190.0f * evo) * (info->legs > 0 ? 2.2f : 1.0f);
            if (Vector2Distance(p->pos, m->home) < 190.0f) {
                m->state = MOB_RAID;                 // you're in their yard
                m->retarget = 0;
                m->rage = 8.0f;
                MobCallForHelp(m->pos, 420.0f);
            } else if (Vector2Distance(m->pos, p->pos) < aggroRange) {
                m->state = MOB_RAID;              // spotted you
                MobCallForHelp(m->pos, 380.0f);   // ...and told everyone
                SfxPlayAt(SFX_MOB_CHIRP, m->pos, worldListener, 0.5f);
            } else if (m->retarget <= 0) {
                // Wander — and the spiders wander FAR. Their roam
                // radius is measured in map-widths, not tiles, which
                // is why you meet them a long way from any nest.
                float roam = info->roam;
                m->target = (Vector2){ m->home.x + GetRandomValue(-(int)roam, (int)roam),
                                       m->home.y + GetRandomValue(-(int)roam, (int)roam) };
                float span = worldSize * (float)TILE_SIZE;
                m->target.x = Clamp(m->target.x, 8.0f, span - 8.0f);
                m->target.y = Clamp(m->target.y, 8.0f, span - 8.0f);
                m->retarget = (float)GetRandomValue(3, 9);
            }
        }
        if (m->state == MOB_RAID && m->retarget <= 0) {
            m->target = p->pos;                   // re-acquire the player
            m->retarget = 1.5f;
            // Keep the pack together: a raider re-acquiring you drags
            // its neighbours along, so they arrive as a wall.
            MobCallForHelp(m->pos, 260.0f);
        }

        // Move axis-by-axis (same trick as PlayerMove) so mobs slide
        // along walls instead of sticking to them. Enraged mobs move
        // noticeably faster — you can see the swarm accelerate.
        float moveSpeed = baseSpeed * info->speed * (m->rage > 0 ? 1.45f : 1.0f);
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
            // your turrets are food. And when a container comes apart
            // they STEAL: one stack goes home in the mob's jaws, the
            // rest ends up scattered across the floor.
            if (!movedX && !movedY && m->state == MOB_RAID) {
                int tx = (int)((m->pos.x + dir.x * TILE_SIZE * 0.8f) / TILE_SIZE);
                int ty = (int)((m->pos.y + dir.y * TILE_SIZE * 0.8f) / TILE_SIZE);
                if (WorldInBounds(tx, ty) && TILES[world[tx][ty].type].breakable) {
                    float chew = TUNE.mobChewDPS * info->damage * (1.0f + 1.0f * evo);
                    if (WorldDamageTile(tx, ty, chew * dt)) {
                        Machine *victim = MachineAt(tx, ty);
                        if (victim != NULL && m->loot == ITEM_NONE) {
                            for (int s = 0; s < MachineSlotCount(victim); s++) {
                                if (victim->slots[s] == ITEM_NONE || victim->counts[s] <= 0) continue;
                                m->loot  = victim->slots[s];
                                m->lootN = victim->counts[s];
                                victim->slots[s] = ITEM_NONE;
                                victim->counts[s] = 0;
                                break;
                            }
                        }
                        RemoveMachineAt(tx, ty, NULL);   // the rest spills out
                    }
                }
            }
        }

        // Contact damage, per species. A broodmother hits nearly
        // three times as hard as a crawler.
        if (Vector2Distance(m->pos, p->pos) < PLAYER_RADIUS + info->size + 3.0f) {
            PlayerDamage(p, TUNE.mobContactDPS * info->damage * (1.0f + evo) * dt);
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

// ─── Wear: nothing runs forever ───────────────────────────
// A machine counts the seconds it spends WORKING — idle time is
// free, which is why a drill with no coal ages not at all. Past its
// rated service life (TileServiceLife, in gamedata.h) it starts to
// come apart on an ASYMPTOTIC curve: for a long while there's barely
// any damage, and then the damage per second climbs as the square-ish
// power of how far past due it is, so the last few percent of its
// life happen very fast.
//
// The intended experience is exactly the one you get with a real
// compressor: you never think about it, you don't know it can fail,
// and then one day the line is stopped and something is smoking.
// Returns false if the machine died this tick (the caller must stop
// touching it — its record is gone).
static bool MachineWearTick(Machine *m, float dt, bool working) {
    float life = TileServiceLife(m->type) * TUNE.machineWearScale;
    if (life <= 0) return true;                 // this kind never wears out
    if (working) m->workHours += dt;
    if (m->workHours <= life) return true;      // still within its rated life

    float over = (m->workHours - life) / life;  // 0 at rated life, 1 at double
    float dmg = MACHINE_DECAY_RATE * powf(over, MACHINE_DECAY_POWER);
    Vector2 at = { (m->x + 0.5f) * TILE_SIZE, (m->y + 0.5f) * TILE_SIZE };
    if (WorldDamageTile(m->x, m->y, dmg * dt)) {
        SfxPlayAt(SFX_MACHINE_FAIL, at, worldListener, 0.85f);
        AddEffectTinted(EFFECT_DEBRIS, at, 20.0f, 0.6f, (Color){ 90, 90, 96, 255 });
        RemoveMachineAt(m->x, m->y, NULL);      // whatever was inside spills out
        return false;
    }
    return true;
}

// How worn is it, 0..1? (1 = at its rated life; beyond that it's
// living on borrowed time.) The machine panel draws this.
static float MachineWearFrac(const Machine *m) {
    float life = TileServiceLife(m->type) * TUNE.machineWearScale;
    if (life <= 0) return 0;
    float f = m->workHours / life;
    return (f > 1.0f) ? 1.0f : f;
}

// ─── Machines: drills, belts, hands, turrets ──────────────
static void UpdateMachines(float dt, Player *p) {
    (void)p;
    for (int i = 0; i < MAX_MACHINES; i++) {
        Machine *m = &machines[i];
        if (!m->active) continue;
        Vector2 center = { (m->x + 0.5f) * TILE_SIZE, (m->y + 0.5f) * TILE_SIZE };

        // Age it first. "Working" means burning fuel, carrying cargo,
        // or standing ready to shoot — an idle machine doesn't age.
        bool working = false;
        if (TileNeedsFuel(m->type))       working = (m->fuel > 0 || m->coal > 0);
        else if (TileIsBeltLike(m->type)) working = (m->slots[0] != ITEM_NONE);
        else if (m->type == TILE_TURRET) working = (m->ammo > 0);
        else if (m->type == TILE_LASER_TURRET) working = true;
        if (!MachineWearTick(m, dt, working)) continue;   // it just died

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

            // Hand one item to the tile we face. Chests, turrets,
            // fuel hoppers, splitters, tunnel mouths and other belts
            // all answer through the one shared rule.
            if (MachineAcceptItem(BeltOutputTarget(m), m->slots[0])) {
                if (--m->counts[0] <= 0) {
                    m->slots[0] = ITEM_NONE; m->counts[0] = 0;
                }
                m->beltProgress = 0;
            }
            break;
        }

        case TILE_SPLITTER: {
            // ── One lane in, two lanes out ──────────────────────
            // Items arrive from behind (or from any belt pointing at
            // it) and leave through the tiles to its LEFT and RIGHT,
            // alternating — that alternation is the whole machine.
            //
            // With a FILTER set it stops alternating and starts
            // SORTING: the filtered item only ever goes left, and
            // everything else only ever goes right. A filtered item
            // waits for its own lane rather than escaping down the
            // wrong one, which is the entire point of saying so.
            if (m->slots[0] == ITEM_NONE || m->counts[0] <= 0) {
                m->beltProgress = 0;
                m->timer = SPLITTER_INTERVAL;
                break;
            }
            m->timer -= dt;
            m->beltProgress = 1.0f - (m->timer / SPLITTER_INTERVAL);
            if (m->beltProgress < 0) m->beltProgress = 0;
            if (m->beltProgress > 1) m->beltProgress = 1;
            if (m->timer > 0) break;
            m->timer = SPLITTER_INTERVAL;

            ItemID id = m->slots[0];
            bool sent = false;
            if (m->filter != ITEM_NONE) {
                int side = (id == m->filter) ? 0 : 1;      // left : right
                sent = MachineAcceptItem(SplitterOutput(m, side), id);
            } else {
                // Round robin, and if the side whose turn it is won't
                // take it, offer the other — one blocked branch must
                // never stall the whole line.
                for (int attempt = 0; attempt < 2 && !sent; attempt++) {
                    int side = (m->splitToggle + attempt) & 1;
                    sent = MachineAcceptItem(SplitterOutput(m, side), id);
                    if (sent) m->splitToggle = (side + 1) & 1;
                }
            }
            if (sent) {
                if (--m->counts[0] <= 0) { m->slots[0] = ITEM_NONE; m->counts[0] = 0; }
                m->beltProgress = 0;
            }
            break;
        }

        case TILE_TUNNEL_IN: {
            // The mouth items go down. It behaves exactly like a belt
            // whose next tile happens to be somewhere else entirely:
            // BeltOutputTarget returns the partner mouth, so LENGTH
            // costs nothing at all here — no queue to walk, no timers
            // per tile. That's what makes "any length" honest.
            if (m->slots[0] == ITEM_NONE || m->counts[0] <= 0) {
                m->beltProgress = 0;
                m->timer = TUNNEL_INTERVAL;
                break;
            }
            m->timer -= dt;
            m->beltProgress = 1.0f - (m->timer / TUNNEL_INTERVAL);
            if (m->beltProgress < 0) m->beltProgress = 0;
            if (m->beltProgress > 1) m->beltProgress = 1;
            if (m->timer > 0) break;
            m->timer = TUNNEL_INTERVAL;

            if (MachineAcceptItem(BeltOutputTarget(m), m->slots[0])) {
                if (--m->counts[0] <= 0) { m->slots[0] = ITEM_NONE; m->counts[0] = 0; }
                m->beltProgress = 0;
            }
            break;
        }

        case TILE_TUNNEL_OUT: {
            // ...and the mouth they come back up from: a plain belt
            // tile that unloads onto whatever it faces.
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
            ItemID id = m->slots[0];
            bool sent = MachineAcceptItem(dest, id);
            // Nothing there to take it? Set it on the ground in front,
            // the way an arm does — a tunnel that ends in open dirt
            // still delivers instead of silently plugging itself.
            if (!sent && dest == NULL) {
                int fx = m->x + DIR_DX[m->dir], fy = m->y + DIR_DY[m->dir];
                if (GroundTileFree(fx, fy)) {
                    sent = DropItemAt((Vector2){ (fx + 0.5f) * TILE_SIZE,
                                                 (fy + 0.5f) * TILE_SIZE }, id, 1);
                }
            }
            if (sent) {
                if (--m->counts[0] <= 0) { m->slots[0] = ITEM_NONE; m->counts[0] = 0; }
                m->beltProgress = 0;
            }
            break;
        }

        case TILE_INSERTER: {
            // The robotic hand: grab ONE item from the tile BEHIND,
            // set it down on the tile IN FRONT. Holds the item (slot
            // 0) until the destination has room. Runs on coal — and
            // keeps ITSELF fed, so a line doesn't quietly die the
            // moment one arm burns its last lump.
            int bx = m->x - DIR_DX[m->dir], by = m->y - DIR_DY[m->dir];   // source
            int fx = m->x + DIR_DX[m->dir], fy = m->y + DIR_DY[m->dir];   // dest
            Machine *src  = MachineAt(bx, by);
            Machine *dest = MachineAt(fx, fy);

            // AUTO-FUELING. A dry arm spends its swing looking for
            // coal instead: in the tile behind it, in its four
            // neighbours, or in a pile on the floor. Rate-limited by
            // the same timer, so a starving arm doesn't scan every
            // frame — it just tries once per swing, forever, and
            // wakes itself up the moment coal comes past.
            if (m->coal <= 0 && m->fuel <= 0) {
                m->timer -= dt;
                if (m->timer > 0) break;
                m->timer = TUNE.inserterInterval;
                if (!InserterSelfFuel(m)) break;      // still dry → keep sleeping
            }
            if (!MachineConsumeFuel(m, dt)) break;    // out of coal → arm stops

            m->timer -= dt;
            if (m->timer > 0) break;
            m->timer = TUNE.inserterInterval;

            if (m->slots[0] == ITEM_NONE) {   // hand empty → try to grab
                // WHAT is this arm allowed to pick up?
                //   a filter set by hand   → only that item, ever
                //   a fuel burner in front → coal, because that's the
                //                            only thing it can accept
                //   otherwise              → the first thing it touches
                // Grabbing indiscriminately is what used to leave arms
                // stood there holding an ore plate forever, in front of
                // a machine that only wanted coal.
                bool feeding = (dest != NULL && TileNeedsFuel(dest->type));
                ItemID want = (m->filter != ITEM_NONE) ? m->filter
                            : (feeding ? ITEM_COAL : ITEM_NONE);
                bool blocked = feeding && want == ITEM_COAL &&
                               dest->coal >= MACHINE_FUEL_MAX;
                if (!blocked) {
                    ItemID got = ITEM_NONE;
                    if (src != NULL && (src->type == TILE_CHEST || src->type == TILE_DRILL ||
                                        TileIsBeltLike(src->type))) {
                        got = (want != ITEM_NONE) ? MachineTakeSpecific(src, want)
                                                  : MachineTakeItem(src);
                    }
                    // Nothing behind it? Pick the floor up instead.
                    if (got == ITEM_NONE) got = GroundTakeOneAt(bx, by, want);
                    if (got != ITEM_NONE) { m->slots[0] = got; m->counts[0] = 1; }
                }
            }
            if (m->slots[0] != ITEM_NONE) {   // hand full → try to place
                ItemID id = m->slots[0];
                bool placed = false;
                if (dest != NULL) {
                    placed = MachineAcceptItem(dest, id);
                } else if (GroundTileFree(fx, fy)) {
                    // Nothing in front to hand it to → set it on the
                    // floor, exactly like an arm unloading onto the
                    // ground in Factorio. A machine that REFUSES the
                    // item is different: then the arm keeps holding,
                    // so a mis-aimed arm can't quietly empty a chest
                    // onto the dirt.
                    placed = DropItemAt((Vector2){ (fx + 0.5f) * TILE_SIZE,
                                                   (fy + 0.5f) * TILE_SIZE }, id, 1);
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
                SfxPlayAt(SFX_SHOT_SMALL, center, worldListener, 0.45f);
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
                SfxPlayAt(SFX_SHOT_SMALL, center, worldListener, 0.45f);
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
                SfxPlayAt(SFX_LASER, center, worldListener, 0.5f);
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

// ─── F: scoop items off belts AND off the floor ───────────
// Hold F to pull cargo off any belt within reach — the manual
// override for when you want something back off the line. It sweeps
// loose piles off the ground in the same gesture, so F is simply
// "gather everything near me".
static void BeltPickupNear(Player *p, float radiusPx) {
    GroundPickupNear(p, p->pos, radiusPx, 0.25f);

    int cx = (int)(p->pos.x / TILE_SIZE), cy = (int)(p->pos.y / TILE_SIZE);
    int r = (int)(radiusPx / TILE_SIZE) + 1;
    for (int x = cx - r; x <= cx + r; x++) {
        for (int y = cy - r; y <= cy + r; y++) {
            if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) continue;
            if (!TileIsBeltLike(world[x][y].type)) continue;
            Vector2 c = { (x + 0.5f) * TILE_SIZE, (y + 0.5f) * TILE_SIZE };
            if (Vector2Distance(p->pos, c) > radiusPx) continue;
            Machine *m = MachineAt(x, y);
            if (m == NULL || m->slots[0] == ITEM_NONE || m->counts[0] <= 0) continue;
            PlayerGiveItem(p, m->slots[0], m->counts[0]);
            SfxPlay(SFX_PICKUP, 0.35f, 2.0f);
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
            continue;
        }
        // A tunnel mouth whose partner is gone (blown up, chewed,
        // loaded from a stale save) is ORPHANED. Forget the dead link
        // rather than pointing at whatever moved in — the drawing
        // shows the mouth as broken, and mining it refunds one tile.
        if (TileIsTunnel(m->type) && m->linkX >= 0 && m->linkY >= 0) {
            if (!WorldInBounds(m->linkX, m->linkY) ||
                !TileIsTunnel(world[m->linkX][m->linkY].type)) {
                m->linkX = m->linkY = -1;
            }
        }
    }
}

// ─── Loose items ──────────────────────────────────────────
// They just sit there and age; the age is what stops a stack you
// dropped a frame ago from jumping back into your pockets. Walk
// close enough and it's yours (hold F to reach farther).
static void UpdateGroundItems(float dt, Player *p) {
    for (int i = 0; i < MAX_GROUND_ITEMS; i++) {
        if (groundItems[i].active) groundItems[i].age += dt;
    }
    GroundPickupNear(p, p->pos, GROUND_PICKUP_DIST, GROUND_PICKUP_DELAY);
}

// One call from main.c per gameplay frame.
static void EntitiesUpdate(float dt, Player *p) {
    entGameTime += dt;
    EntitiesReconcile();
    DrainWorldBreakEvents();   // debris for anything destroyed last frame
    UpdateSpawnersAndRaids(dt, p);
    UpdateMachines(dt, p);
    UpdateMobs(dt, p);
    UpdateBots(dt, p);
    UpdateProjectiles(dt, p);
    UpdateBombs(dt, p);
    UpdateGroundItems(dt, p);
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

    // ── Buried belt runs ──────────────────────────────────
    // The tunnel itself is invisible by definition, so we draw the
    // faintest possible ghost of it: a dotted line between the two
    // mouths, under everything else. Without it a base full of
    // underground belts is a base you can't read. Walked from the
    // ENTRANCE only, so a run is drawn once, not twice.
    for (int i = 0; i < MAX_MACHINES; i++) {
        Machine *tm = &machines[i];
        if (!tm->active || tm->type != TILE_TUNNEL_IN) continue;
        if (tm->linkX < 0 || tm->linkY < 0) continue;
        Vector2 a = { (tm->x + 0.5f) * TILE_SIZE, (tm->y + 0.5f) * TILE_SIZE };
        Vector2 b = { (tm->linkX + 0.5f) * TILE_SIZE, (tm->linkY + 0.5f) * TILE_SIZE };
        // Cheap reject: both ends far off the same side of the view.
        if ((a.x < viewTopLeft.x && b.x < viewTopLeft.x) ||
            (a.x > viewBottomRight.x && b.x > viewBottomRight.x) ||
            (a.y < viewTopLeft.y && b.y < viewTopLeft.y) ||
            (a.y > viewBottomRight.y && b.y > viewBottomRight.y)) continue;
        int steps = TunnelLength(tm);
        float crawl = fmodf((float)GetTime() * 1.6f, 1.0f);
        for (int s = 0; s < steps; s++) {
            float f = (s + 0.5f) / steps;
            Vector2 dot = { a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f };
            // One pip travels the line so you can see which way it runs.
            bool head = (fabsf(f - crawl) < 0.5f / steps);
            DrawCircleV(dot, head ? 2.6f : 1.6f,
                        head ? (Color){ 190, 170, 255, 190 } : (Color){ 120, 108, 180, 90 });
        }
    }

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
            } else if (t == TILE_SPLITTER) {
                // ── Splitter ───────────────────────────────────
                // A housing with the input chevron coming in from
                // behind and two output arrows leaving sideways —
                // you can read which way it splits from across the
                // base, without opening anything.
                float ts = (float)TILE_SIZE;
                DrawRectangle((int)px, (int)py, TILE_SIZE, TILE_SIZE, (Color){ 66, 58, 44, 255 });
                DrawRectangleLines((int)px, (int)py, TILE_SIZE, TILE_SIZE, (Color){ 34, 32, 28, 255 });
                float dx = (float)DIR_DX[dir], dy = (float)DIR_DY[dir];
                // Feed chevron, sliding in from the back edge.
                float phase = fmodf((float)GetTime() / SPLITTER_INTERVAL, 1.0f);
                float in = -0.5f + phase * 0.5f;
                DrawLineEx((Vector2){ center.x + dx * ts * in - (-dy) * 7.0f,
                                      center.y + dy * ts * in - dx * 7.0f },
                           (Vector2){ center.x + dx * ts * in + (-dy) * 7.0f,
                                      center.y + dy * ts * in + dx * 7.0f },
                           3.0f, (Color){ 176, 156, 60, 210 });
                // The two output arrows.
                for (int side = 0; side < 2; side++) {
                    int od = (side == 0) ? ((dir + 3) & 3) : ((dir + 1) & 3);
                    float ox = (float)DIR_DX[od], oy = (float)DIR_DY[od];
                    Vector2 tip  = { center.x + ox * 10.0f, center.y + oy * 10.0f };
                    Vector2 lft  = { center.x - oy * 4.5f - ox * 2.0f,
                                     center.y + ox * 4.5f - oy * 2.0f };
                    Vector2 rgt  = { center.x + oy * 4.5f - ox * 2.0f,
                                     center.y - ox * 4.5f - oy * 2.0f };
                    Color arrow = (Color){ 255, 255, 255, 190 };
                    // With a filter on, the two lanes mean different
                    // things, so they stop looking the same: the
                    // filtered lane goes orange, the reject lane gray.
                    if (m != NULL && m->filter != ITEM_NONE) {
                        arrow = (side == 0) ? (Color){ 255, 166, 2, 230 }
                                            : (Color){ 150, 150, 158, 200 };
                    }
                    DrawTriangle(tip, lft, rgt, arrow);
                    DrawTriangle(tip, rgt, lft, arrow);
                }
            } else if (TileIsTunnel(t)) {
                // ── Underground belt mouth ─────────────────────
                // A ramp: bars that shrink as they go down (entrance)
                // or grow as they come up (exit), so which end you're
                // looking at is obvious from the slope alone.
                bool goingDown = (t == TILE_TUNNEL_IN);
                DrawRectangle((int)px, (int)py, TILE_SIZE, TILE_SIZE, (Color){ 48, 44, 70, 255 });
                DrawRectangleLines((int)px, (int)py, TILE_SIZE, TILE_SIZE, (Color){ 26, 24, 40, 255 });
                float dx = (float)DIR_DX[dir], dy = (float)DIR_DY[dir];
                for (int k = 0; k < 4; k++) {
                    float u = -0.36f + k * 0.24f;          // back → front
                    float shrink = goingDown ? (1.0f - k * 0.22f) : (0.34f + k * 0.22f);
                    float ax = center.x + dx * TILE_SIZE * u;
                    float ay = center.y + dy * TILE_SIZE * u;
                    unsigned char a = (unsigned char)(90 + 40 * k);
                    DrawLineEx((Vector2){ ax - (-dy) * 7.0f * shrink, ay - dx * 7.0f * shrink },
                               (Vector2){ ax + (-dy) * 7.0f * shrink, ay + dx * 7.0f * shrink },
                               2.5f, (Color){ 168, 152, 228, a });
                }
                Vector2 tip  = { center.x + dx * 9.0f, center.y + dy * 9.0f };
                Vector2 lft  = { center.x - dy * 4.0f - dx * 1.0f, center.y + dx * 4.0f - dy * 1.0f };
                Vector2 rgt  = { center.x + dy * 4.0f - dx * 1.0f, center.y - dx * 4.0f - dy * 1.0f };
                Color nose = goingDown ? (Color){ 200, 190, 255, 210 } : (Color){ 255, 255, 255, 210 };
                DrawTriangle(tip, lft, rgt, nose);
                DrawTriangle(tip, rgt, lft, nose);
                // An orphaned mouth (its partner was destroyed) is
                // dead weight — say so instead of letting it look fine.
                if (m != NULL && (m->linkX < 0 || m->linkY < 0)) {
                    DrawRectangleLines((int)px, (int)py, TILE_SIZE, TILE_SIZE, RED);
                }
            } else if (t == TILE_DOOR || t == TILE_CHEST || t == TILE_DRILL ||
                       t == TILE_INSERTER || t == TILE_TURRET ||
                       t == TILE_LASER_TURRET || t == TILE_RESEARCH) {
                ItemID icon = ItemThatPlaces(t);
                if (icon != ITEM_NONE) DrawItemSprite(icon, px + 2, py + 2, TILE_SIZE - 4);
            }

            if (m == NULL) continue;

            if (t == TILE_INSERTER) {
                // ── Animated swing arm ─────────────────────────
                // A square motor housing, a jointed boom, and a
                // two-finger GRIPPER — machinery, not a stick with a
                // knob on the end. The boom swings from the pickup
                // side (behind) to the drop side (front) as its timer
                // runs, so you can watch it carry the item across.
                float swing;                       // -1 = behind, +1 = front
                float phase = 1.0f - (m->timer / TUNE.inserterInterval);
                if (phase < 0) phase = 0;
                if (phase > 1) phase = 1;
                bool carrying = (m->slots[0] != ITEM_NONE && m->counts[0] > 0);
                // Carrying → swinging forward; empty → returning back.
                swing = carrying ? (-1.0f + 2.0f * phase) : (1.0f - 2.0f * phase);

                float dxf = (float)DIR_DX[dir], dyf = (float)DIR_DY[dir];
                float sxf = -dyf, syf = dxf;      // "sideways" for this facing
                Vector2 baseP = center;
                float reach = TILE_SIZE * 0.62f;
                Vector2 gripP = { center.x + dxf * reach * swing,
                                  center.y + dyf * reach * swing };
                // The elbow rides off to one side so the boom reads as
                // a two-bar linkage folding and unfolding.
                Vector2 elbow = { (baseP.x + gripP.x) * 0.5f + sxf * 6.0f,
                                  (baseP.y + gripP.y) * 0.5f + syf * 6.0f };

                // Motor housing: a squat plate bolted to the tile.
                DrawRectanglePro((Rectangle){ baseP.x, baseP.y, 13, 9 },
                                 (Vector2){ 6.5f, 4.5f },
                                 atan2f(dyf, dxf) * RAD2DEG,
                                 (Color){ 96, 96, 104, 255 });
                DrawCircleV(baseP, 3.0f, (Color){ 58, 58, 64, 255 });

                // Boom: two straight bars with a pin joint between.
                DrawLineEx(baseP, elbow, 3.0f, (Color){ 190, 175, 90, 255 });
                DrawLineEx(elbow, gripP, 3.0f, (Color){ 190, 175, 90, 255 });
                DrawCircleV(elbow, 2.5f, (Color){ 150, 150, 160, 255 });

                // Gripper: two short fingers set across the boom,
                // open when empty and closed around the cargo.
                float spread = carrying ? 2.6f : 4.6f;
                Vector2 wrist = { gripP.x - dxf * 2.5f, gripP.y - dyf * 2.5f };
                for (int side = -1; side <= 1; side += 2) {
                    Vector2 root = { wrist.x + sxf * spread * side,
                                     wrist.y + syf * spread * side };
                    Vector2 tip  = { root.x + dxf * 4.5f, root.y + dyf * 4.5f };
                    DrawLineEx(root, tip, 2.0f, (Color){ 214, 196, 104, 255 });
                }
                DrawLineEx((Vector2){ wrist.x + sxf * spread, wrist.y + syf * spread },
                           (Vector2){ wrist.x - sxf * spread, wrist.y - syf * spread },
                           2.0f, (Color){ 150, 150, 160, 255 });
                if (carrying) {
                    DrawItemSprite(m->slots[0], gripP.x - 6, gripP.y - 6, 12);
                }
            }

            // Cargo. On a belt it SLIDES along the path as
            // beltProgress advances, so you can watch items travel.
            // (The inserter draws its own cargo in the claw, above.)
            if (m->slots[0] != ITEM_NONE && m->counts[0] > 0 && TileIsBeltLike(t)) {
                Vector2 at = center;
                float u = m->beltProgress - 0.5f;         // -0.5 .. +0.5
                if (t == TILE_CONVEYOR_CORNER && m->beltProgress < 0.5f) {
                    float sx = -(float)DIR_DY[dir], sy = (float)DIR_DX[dir];
                    at.x -= sx * TILE_SIZE * (0.5f - m->beltProgress);
                    at.y -= sy * TILE_SIZE * (0.5f - m->beltProgress);
                } else if (t == TILE_SPLITTER) {
                    // Cargo drifts from the back of the splitter to
                    // the mouth it's about to leave by.
                    int side = (m->filter != ITEM_NONE)
                             ? ((m->slots[0] == m->filter) ? 0 : 1) : (m->splitToggle & 1);
                    int od = SplitterOutDir(m, side);
                    float back = 0.5f - m->beltProgress;
                    at.x += -DIR_DX[dir] * TILE_SIZE * back * 0.6f +
                             DIR_DX[od] * TILE_SIZE * m->beltProgress * 0.5f;
                    at.y += -DIR_DY[dir] * TILE_SIZE * back * 0.6f +
                             DIR_DY[od] * TILE_SIZE * m->beltProgress * 0.5f;
                } else {
                    at.x += DIR_DX[dir] * TILE_SIZE * u;
                    at.y += DIR_DY[dir] * TILE_SIZE * u;
                }
                DrawItemSprite(m->slots[0], at.x - 7, at.y - 7, 14);
                if (m->counts[0] > 1) {
                    DrawText(TextFormat("%d", m->counts[0]), (int)(at.x + 5),
                             (int)(at.y + 1), 10, RAYWHITE);
                }
            }

            // A filtered machine wears the item it sorts for, small,
            // in its top-left corner — the setting is visible on the
            // factory floor, not buried in a panel.
            if (TileHasFilter(t) && m->filter != ITEM_NONE) {
                DrawRectangle((int)px + 1, (int)py + 1, 11, 11, (Color){ 18, 16, 22, 210 });
                DrawItemSprite(m->filter, px + 1.5f, py + 1.5f, 10);
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

    // Loose items on the floor: the sprite, a soft shadow, and a
    // count when the pile has grown. Drawn under everything that
    // moves, because that's where they are — on the dirt.
    for (int i = 0; i < MAX_GROUND_ITEMS; i++) {
        GroundItem *g = &groundItems[i];
        if (!g->active || g->count <= 0) continue;
        if (g->pos.x < viewTopLeft.x - TILE_SIZE || g->pos.x > viewBottomRight.x + TILE_SIZE ||
            g->pos.y < viewTopLeft.y - TILE_SIZE || g->pos.y > viewBottomRight.y + TILE_SIZE)
            continue;
        DrawEllipse((int)(g->pos.x + worldShadowVec.x * 0.35f),
                    (int)(g->pos.y + worldShadowVec.y * 0.35f + 4),
                    6.0f, 3.0f, WORLD_SHADOW_COLOR);
        DrawItemSprite(g->id, g->pos.x - 7, g->pos.y - 7, 14);
        if (g->count > 1) {
            DrawText(TextFormat("%d", g->count), (int)(g->pos.x + 5),
                     (int)(g->pos.y + 1), 10, RAYWHITE);
        }
    }

    // The natives. Crawlers are blobs; the three spider species get
    // jointed legs that scuttle in time with their movement, so you
    // can tell at a glance what is coming at you and how fast.
    float evo = EvolutionFactor();
    for (int i = 0; i < MAX_MOBS; i++) {
        if (!mobs[i].active) continue;
        const MobInfo *info = &MOBS_INFO[mobs[i].kind];
        Vector2 mp = mobs[i].pos;
        float mobR = info->size * (1.0f + 0.35f * evo);
        Color body = info->body;
        if (mobs[i].state == MOB_RAID) body = ItemArtShade(body, 1.25f);
        if (mobs[i].rage > 0) {   // enraged: a hot halo you can read at a glance
            body = (Color){ 255, 90, 40, 255 };
            DrawCircleLinesV(mp, mobR + 3.0f, (Color){ 255, 140, 40, 190 });
        }
        DrawEllipse((int)(mp.x + worldShadowVec.x * 0.5f),
                    (int)(mp.y + worldShadowVec.y * 0.5f),
                    mobR * 1.05f, mobR * 0.8f, WORLD_SHADOW_COLOR);

        // Legs first, so the body sits on top of them.
        if (info->legs > 0) {
            Color legC = ItemArtShade(body, 0.62f);
            for (int leg = 0; leg < info->legs; leg++) {
                float side = (leg < info->legs / 2) ? -1.0f : 1.0f;
                float slot = (float)(leg % (info->legs / 2));
                float ang = (-0.75f + 0.5f * slot) * PI * 0.62f;   // fan them out
                // Each leg lifts and falls out of phase with the next.
                float step = sinf(mobs[i].legPhase + leg * 1.4f) * 0.30f;
                float len  = mobR * (1.75f + 0.28f * sinf(mobs[i].legPhase * 1.3f + leg));
                Vector2 knee = { mp.x + cosf(ang + step) * len * 0.55f * side,
                                 mp.y + sinf(ang + step) * len * 0.55f };
                Vector2 foot = { mp.x + cosf(ang + step * 1.6f) * len * side,
                                 mp.y + sinf(ang + step * 1.6f) * len + mobR * 0.35f };
                DrawLineEx(mp, knee, 2.0f, legC);
                DrawLineEx(knee, foot, 1.6f, legC);
            }
        }

        DrawCircleV(mp, mobR, body);
        DrawCircleLinesV(mp, mobR + 1, (Color){ 20, 10, 20, 255 });
        if (info->legs > 0) {
            // A spider's front half: a smaller cephalothorax with a
            // cluster of little eyes.
            Vector2 head = { mp.x, mp.y - mobR * 0.72f };
            DrawCircleV(head, mobR * 0.55f, ItemArtShade(body, 0.8f));
            for (int e = -1; e <= 1; e += 2) {
                DrawCircle((int)(head.x + e * mobR * 0.22f), (int)(head.y - mobR * 0.1f),
                           mobR * 0.13f, RAYWHITE);
                DrawCircle((int)(head.x + e * mobR * 0.38f), (int)(head.y + mobR * 0.12f),
                           mobR * 0.09f, (Color){ 255, 220, 220, 220 });
            }
        } else {
            DrawCircle((int)(mp.x - 2), (int)(mp.y - 2), 1.5f, RAYWHITE);   // eyes
            DrawCircle((int)(mp.x + 2), (int)(mp.y - 2), 1.5f, RAYWHITE);
        }
        // Carrying something it tore out of your base? Show it.
        if (mobs[i].loot != ITEM_NONE && mobs[i].lootN > 0) {
            DrawItemSprite(mobs[i].loot, mp.x - 5, mp.y - mobR - 13, 10);
        }
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
        } else if (effects[i].kind == EFFECT_DEBRIS) {
            // Chunks thrown outward and falling: eight little squares
            // on fixed radial paths, sinking as they go. This is what
            // a tree coming down or a machine giving up looks like.
            unsigned int seed = effects[i].seed;
            for (int chunk = 0; chunk < 8; chunk++) {
                seed = seed * 1664525u + 1013904223u;
                float ang = (seed % 628) / 100.0f;
                float speed = 0.5f + ((seed >> 9) % 100) / 100.0f;
                float dist = effects[i].maxRadius * speed * k;
                float size = 2.0f + ((seed >> 17) % 3);
                float fall = 14.0f * k * k;             // gravity, roughly
                Color c = effects[i].tint;
                c.a = a;
                DrawRectangle((int)(effects[i].pos.x + cosf(ang) * dist),
                              (int)(effects[i].pos.y + sinf(ang) * dist + fall),
                              (int)size, (int)size, c);
            }
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
                   // Only the ACTIVE map: a shrunk world fills the
                   // minimap instead of hiding in one corner of it.
                   (Rectangle){ 0, 0, (float)worldSize, (float)worldSize },
                   (Rectangle){ (float)mx, (float)my, (float)size, (float)size },
                   (Vector2){ 0, 0 }, 0, WHITE);
    DrawRectangleLines(mx - 3, my - 3, size + 6, size + 6, SKYBLUE);

    // Pips respect the fog: a mob you've never scouted stays hidden.
    float scale = (float)size / (worldSize * TILE_SIZE);
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
                   // Only the ACTIVE map: a shrunk world fills the
                   // minimap instead of hiding in one corner of it.
                   (Rectangle){ 0, 0, (float)worldSize, (float)worldSize },
                   (Rectangle){ (float)mx, (float)my, (float)size, (float)size },
                   (Vector2){ 0, 0 }, 0, WHITE);
    DrawRectangleLines(mx - 2, my - 2, size + 4, size + 4, SKYBLUE);

    float scale = (float)size / (worldSize * TILE_SIZE);
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
    // Appended AFTER the old blocks on purpose: a save written before
    // loose items existed simply ends here, and still loads.
    if (fwrite(groundItems, sizeof(groundItems), 1, f) != 1) return false;
    return true;
}

static bool EntitiesRead(FILE *f) {
    if (fread(&entGameTime, sizeof(entGameTime), 1, f) != 1) return false;
    if (fread(machines, sizeof(machines), 1, f) != 1) return false;
    if (fread(mobs,     sizeof(mobs),     1, f) != 1) return false;
    if (fread(bots,     sizeof(bots),     1, f) != 1) return false;
    // Loose items are OPTIONAL: an older save just stops after the
    // bots, and a world with nothing on its floor is a perfectly good
    // world. Failing the whole load over it would throw away a base.
    if (fread(groundItems, sizeof(groundItems), 1, f) != 1) {
        memset(groundItems, 0, sizeof(groundItems));
    }

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
        // Untrusted file data: a bogus filter would index ITEMS[] out
        // of range, and a bogus tunnel link would send cargo into the
        // void. Both fail SAFE — no filter, no link.
        if (m->filter < ITEM_NONE || m->filter >= ITEM_COUNT) m->filter = ITEM_NONE;
        if (!WorldInBounds(m->linkX, m->linkY)) m->linkX = m->linkY = -1;
        m->splitToggle &= 1;
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
    for (int i = 0; i < MAX_GROUND_ITEMS; i++) {
        GroundItem *g = &groundItems[i];
        if (!g->active) continue;
        if (g->id <= ITEM_NONE || g->id >= ITEM_COUNT || g->count <= 0) {
            g->active = false;                      // garbage entry → drop it
            continue;
        }
        g->pos.x = Clamp(g->pos.x, 0, worldMax);
        g->pos.y = Clamp(g->pos.y, 0, worldMax);
        if (g->age < 0) g->age = 0;
    }
    if (entGameTime < 0) entGameTime = 0;
    entShake = 0;
    return true;
}

#endif // ENTITIES_H
