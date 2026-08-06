#ifndef WORLD_H
#define WORLD_H
// ============================================================
//  WORLD.H — the tile grid and everything that happens to it.
//
//  Owns: the `world` array. Knows how to: generate it, draw it,
//  damage/replace tiles, save/load it. Knows NOTHING about the
//  player, input, or UI — that keeps it swappable and testable.
//
//  C CONCEPT — static (at file scope):
//  `static` here means "private to this file-inclusion" — these
//  names won't clash with names in other files. We use it on
//  everything in headers in this project (see README note on the
//  single-translation-unit style this codebase uses).
// ============================================================

#include <stdio.h>     // FILE, fopen, fread, fwrite — C's file I/O
#include <string.h>    // memset — clearing the fog-of-war grid
#include <math.h>      // cosf/sinf — placing infestation patches
#include "raylib.h"
#include "raymath.h"   // Vector2Distance — canopy fade near the player
#include "config.h"
#include "sound.h"     // tiles make noises when they break
#include "gamedata.h"

// ─── Biomes ───────────────────────────────────────────────
// A biome is a REGION FLAVOR: it tints the ground and biases what
// generates there. Stored per tile so drawing stays a table lookup.
typedef enum {
    BIOME_MEADOW = 0,   // balanced starter land
    BIOME_FOREST,       // dense trees — wood country
    BIOME_ROCKLANDS,    // dense rocks — ore country
    BIOME_WASTE,        // the corrupted ring at the map edge; mob home
    BIOME_COUNT
} Biome;

static const Color BIOME_GROUND[BIOME_COUNT] = {
    [BIOME_MEADOW]    = {  62, 158,  76, 255 },
    [BIOME_FOREST]    = {  30, 106,  50, 255 },
    [BIOME_ROCKLANDS] = { 112, 116,  94, 255 },
    [BIOME_WASTE]     = {  98,  50,  70, 255 },
};

// Each cell only stores what VARIES per tile: its type, current
// health, biome, and shape variant. Color/name/maxHealth are looked
// up in TILES[] — derive, don't copy.
//
// `variant` packs a rock's SUB-TILE SHAPE into one byte:
//   low 2 bits  = size class: 0=full, 1=three-quarter, 2=half, 3=quarter
//   next 2 bits = which corner the mass sits in (0 TL, 1 TR, 2 BL, 3 BR)
// Quarter rocks are ankle-high — you WALK OVER them; three-quarter
// rocks leave one walkable corner you can squeeze through.
typedef struct {
    TileType      type;
    float         health;
    unsigned char biome;
    unsigned char variant;
    unsigned char ore;      // 0 = plain ground, 1 = sulfur field,
                            // 2 = metal field, 3 = coal field.
                            // Survives mining the node ON it —
                            // clear it, then place a drill.
} Tile;

// Rocks and ore nodes share the size-variant system and collision.
static bool TileIsRockLike(TileType t) {
    return t == TILE_ROCK || t == TILE_SULFUR_NODE ||
           t == TILE_METAL_NODE || t == TILE_COAL_NODE;
}

// C CONCEPT — 2D array: WORLD_SIZE * WORLD_SIZE Tiles in one block
// of memory. world[x][y] picks one. ~1.8MB at 384x384 — still fine.
static Tile world[WORLD_SIZE][WORLD_SIZE];

// Fog of war: which tiles has the player ever SEEN? Drives the
// minimap and the hold-G map; the live view is never fogged.~
static bool worldExplored[WORLD_SIZE][WORLD_SIZE];

// The minimap needs re-rendering whenever a tile changes; drawing
// 147,456 rectangles every frame would not fly, so we keep a texture
// and only rebuild it when this flag says the world changed.
static bool worldMinimapDirty = true;

// ─── How big is the map, really? ──────────────────────────
// WORLD_SIZE is how much memory we RESERVE; `worldSize` is how much
// of it is currently a world. Everything outside is TILE_VOID, which
// nothing can enter or break. The dev console slides this and
// regenerates, which is how you get a pocket-sized test map or the
// full continent without recompiling.
static int worldSize = WORLD_SIZE;

static float WorldCenterPx(void) { return worldSize * TILE_SIZE / 2.0f; }
static bool  WorldInBounds(int x, int y) {
    return x >= 0 && x < worldSize && y >= 0 && y < worldSize;
}

// Where the player is standing, in world pixels. Sounds get quieter
// with distance from it. main.c feeds this in every frame, the same
// way it feeds the day-cycle clock — world.h can't see the player.
static Vector2 worldListener = { 0, 0 };
static void WorldSetListener(Vector2 at) { worldListener = at; }

// ─── Break events ─────────────────────────────────────────
// A tile can be destroyed by six different things (you, a drill, a
// bot, a bullet, a mob's teeth, a bomb). Rather than teach all six
// to spawn debris, WorldDamageTile records WHAT broke and WHERE, and
// entities.h drains the list once a frame and makes the mess. One
// place to add a new effect; every cause gets it for free.
#define WORLD_BREAK_EVENTS 48
typedef struct { int x, y; TileType type; } WorldBreakEvent;
static WorldBreakEvent worldBreaks[WORLD_BREAK_EVENTS];
static int worldBreakCount = 0;

static void WorldPushBreak(int x, int y, TileType t) {
    if (worldBreakCount >= WORLD_BREAK_EVENTS) return;   // a busy frame; fine
    worldBreaks[worldBreakCount++] = (WorldBreakEvent){ x, y, t };
}

// Deterministic per-tile "randomness" for visual variety (speckles,
// canopy sizes). Same x,y → same number, every frame, no storage.
static unsigned int WorldHash(int x, int y) {
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// ─── Day cycle & shadows ──────────────────────────────────
// The "sun" makes one lap per DAY_LENGTH_SECONDS of play time, so
// every shadow in the world slowly swings around its object as the
// day progresses. main.c feeds the clock in each frame (world.h
// can't see the entities' game timer — layering).
static Vector2 worldShadowVec = { 6, 5 };   // current shadow offset, px

static void WorldSetClock(float seconds) {
    float frac = fmodf(seconds, DAY_LENGTH_SECONDS) / DAY_LENGTH_SECONDS;  // 0..1
    float ang  = frac * 2.0f * PI;
    // Shadows stretch at "sunrise/sunset" and shrink toward "noon".
    float len  = TILE_SIZE * (0.20f + 0.14f * fabsf(cosf(ang)));
    worldShadowVec = (Vector2){ cosf(ang) * len, sinf(ang) * len };
}

#define WORLD_SHADOW_COLOR (Color){ 12, 18, 12, 44 }   // light, not goth

// Blend two colors; t = 0 → a, t = 1 → b.
static Color WorldMix(Color a, Color b, float t) {
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t),
                    (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t), a.a };
}

// What color is the GROUND of tile (x,y)? Biome base, shifted
// toward the ore color where an ore field lies. Shared by the
// world renderer and the minimap so they can never disagree.
static Color WorldGroundColor(int x, int y) {
    Color g = BIOME_GROUND[world[x][y].biome];
    if (world[x][y].ore == 1) g = WorldMix(g, (Color){ 176, 156, 44, 255 }, 0.55f);
    if (world[x][y].ore == 2) g = WorldMix(g, (Color){ 118, 124, 164, 255 }, 0.55f);
    if (world[x][y].ore == 3) g = WorldMix(g, (Color){  58,  56,  62, 255 }, 0.60f);
    return g;
}

// Which solid node grows on an ore field of this type?
static TileType WorldOreNodeTile(unsigned char ore) {
    if (ore == 1) return TILE_SULFUR_NODE;
    if (ore == 2) return TILE_METAL_NODE;
    return TILE_COAL_NODE;
}

// Which item does a drill on this field pump?
static ItemID WorldOreItem(unsigned char ore) {
    if (ore == 1) return ITEM_SULFUR_ORE;
    if (ore == 2) return ITEM_METAL_ORE;
    return ITEM_COAL;
}

// Reveal the fog around a world-space position.
static void WorldRevealAround(Vector2 posPx, int radiusTiles) {
    int cx = (int)(posPx.x / TILE_SIZE), cy = (int)(posPx.y / TILE_SIZE);
    long r2 = (long)radiusTiles * radiusTiles;
    for (int x = cx - radiusTiles; x <= cx + radiusTiles; x++) {
        for (int y = cy - radiusTiles; y <= cy + radiusTiles; y++) {
            if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) continue;
            long dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy > r2) continue;
            if (!worldExplored[x][y]) {
                worldExplored[x][y] = true;
                worldMinimapDirty = true;
            }
        }
    }
}

// Rolling cursor for the self-repair sweep (WorldHealTick, below).
// Declared up here because WorldInit rewinds it, and C reads
// top-to-bottom.
static int worldHealCursor = 0;

// ─── Generate a fresh world ───────────────────────────────
// TECHNIQUE — Voronoi biomes: scatter random "seed points", each
// carrying a biome; every tile takes the biome of its NEAREST seed.
// Enemy territory is different: a handful of BIG circular
// infestation patches placed far from spawn, like biter bases.
#define BIOME_SEEDS 40   // more seeds for the much bigger map
static void WorldInit(void) {
    // Everything outside the ACTIVE map is void first, so a smaller
    // world is genuinely bounded rather than fading into old terrain.
    if (worldSize < 32) worldSize = 32;
    if (worldSize > WORLD_SIZE) worldSize = WORLD_SIZE;
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            world[x][y].type    = TILE_VOID;
            world[x][y].health  = TILES[TILE_VOID].maxHealth;
            world[x][y].biome   = BIOME_MEADOW;
            world[x][y].variant = 0;
            world[x][y].ore     = 0;
        }
    }

    int seedX[BIOME_SEEDS], seedY[BIOME_SEEDS];
    Biome seedB[BIOME_SEEDS];
    for (int i = 0; i < BIOME_SEEDS; i++) {
        seedX[i] = GetRandomValue(0, worldSize - 1);
        seedY[i] = GetRandomValue(0, worldSize - 1);
        int roll = GetRandomValue(1, 100);   // meadow half, rest split
        seedB[i] = (roll <= 50) ? BIOME_MEADOW : (roll <= 75) ? BIOME_FOREST : BIOME_ROCKLANDS;
    }

    // Infestation patch centers: far from the center spawn, not
    // hugging the border. MATH — random angle + random distance in
    // [0.33, 0.46] of the world span puts them "out there" without
    // being predictable.
    int center = worldSize / 2;
    int patchX[INFESTATION_PATCHES], patchY[INFESTATION_PATCHES];
    for (int i = 0; i < INFESTATION_PATCHES; i++) {
        float angle = GetRandomValue(0, 6283) / 1000.0f;   // 0..2π
        float dist  = worldSize * (0.33f + GetRandomValue(0, 130) / 1000.0f);
        int px = center + (int)(cosf(angle) * dist);
        int py = center + (int)(sinf(angle) * dist);
        if (px < INFESTATION_RADIUS + 4) px = INFESTATION_RADIUS + 4;
        if (py < INFESTATION_RADIUS + 4) py = INFESTATION_RADIUS + 4;
        if (px > worldSize - INFESTATION_RADIUS - 4) px = worldSize - INFESTATION_RADIUS - 4;
        if (py > worldSize - INFESTATION_RADIUS - 4) py = worldSize - INFESTATION_RADIUS - 4;
        patchX[i] = px; patchY[i] = py;
    }

    for (int x = 0; x < worldSize; x++) {
        for (int y = 0; y < worldSize; y++) {
            // Nearest seed → biome (squared distance: no sqrt needed
            // when you only COMPARE distances).
            int best = 0;
            long bestD = 0x7FFFFFFF;
            for (int i = 0; i < BIOME_SEEDS; i++) {
                long dx = x - seedX[i], dy = y - seedY[i];
                long d = dx * dx + dy * dy;
                if (d < bestD) { bestD = d; best = i; }
            }
            Biome b = seedB[best];

            // Inside an infestation patch? That's enemy ground. The
            // hash jitters the radius so edges look chewed, organic.
            for (int i = 0; i < INFESTATION_PATCHES; i++) {
                long dx = x - patchX[i], dy = y - patchY[i];
                long jitter = (long)(WorldHash(x, y) % 4);
                long r = INFESTATION_RADIUS - jitter;
                if (dx * dx + dy * dy <= r * r) { b = BIOME_WASTE; break; }
            }

            // Generation chances by biome — deliberately SPARSE so
            // there's open ground to walk (and build) between things.
            int treeChance = TREE_CHANCE, rockChance = ROCK_CHANCE;
            if (b == BIOME_FOREST)    { treeChance = 9; rockChance = 1; }
            if (b == BIOME_ROCKLANDS) { treeChance = 1; rockChance = 7; }
            if (b == BIOME_WASTE)     { treeChance = 0; rockChance = 3; }

            int roll = GetRandomValue(1, 100);
            TileType t = TILE_GRASS;
            if      (roll <= treeChance)              t = TILE_TREE;
            else if (roll <= treeChance + rockChance) t = TILE_ROCK;

            // Keep the spawn area obstacle-free so you never start
            // walled into a forest.
            long cdx = x - center, cdy = y - center;
            if (cdx * cdx + cdy * cdy < (long)SPAWN_CLEAR_RADIUS * SPAWN_CLEAR_RADIUS) {
                t = TILE_GRASS;
            }

            world[x][y].type   = t;
            world[x][y].health = TILES[t].maxHealth;
            world[x][y].biome  = (unsigned char)b;
            world[x][y].variant = 0;
            world[x][y].ore    = 0;

            // Rocks come in sizes: full, 3/4 (one open corner),
            // half, and quarter pebbles you can step over. Smaller
            // rocks have less health to chew through.
            if (t == TILE_ROCK) {
                int sroll = GetRandomValue(1, 100);
                int size = (sroll <= 25) ? 0 : (sroll <= 45) ? 1 : (sroll <= 70) ? 2 : 3;
                int corner = GetRandomValue(0, 3);
                world[x][y].variant = (unsigned char)(size | (corner << 2));
                static const float sizeHp[4] = { 1.0f, 0.8f, 0.6f, 0.35f };
                world[x][y].health = TILES[t].maxHealth * sizeHp[size];
            }
        }
    }

    // ── Ore fields ────────────────────────────────────────
    // Factorio-style: blobby colored patches of ore GROUND (place a
    // drill on one and it pumps ore forever), with solid ore NODES
    // clustered on and around each patch. Nodes reuse the rock
    // size-variant system, so they vary from pebbles to full chunks.
    int totalPatches = ORE_SULFUR_PATCHES + ORE_METAL_PATCHES + ORE_COAL_PATCHES;
    for (int i = 0; i < totalPatches; i++) {
        unsigned char oreType = (i < ORE_SULFUR_PATCHES) ? 1
                              : (i < ORE_SULFUR_PATCHES + ORE_METAL_PATCHES) ? 2 : 3;
        TileType nodeTile = WorldOreNodeTile(oreType);
        int pcx = 0, pcy = 0;
        // A few tries to land away from the spawn clearing.
        for (int attempt = 0; attempt < 8; attempt++) {
            pcx = GetRandomValue(12, worldSize - 13);
            pcy = GetRandomValue(12, worldSize - 13);
            long dx = pcx - center, dy = pcy - center;
            if (dx * dx + dy * dy > (long)(SPAWN_CLEAR_RADIUS + 14) * (SPAWN_CLEAR_RADIUS + 14)) break;
        }
        int radius = GetRandomValue(ORE_PATCH_RADIUS_MIN, ORE_PATCH_RADIUS_MAX);

        for (int x = pcx - radius; x <= pcx + radius; x++) {
            for (int y = pcy - radius; y <= pcy + radius; y++) {
                if (x < 1 || x >= worldSize - 1 || y < 1 || y >= worldSize - 1) continue;
                long dx = x - pcx, dy = y - pcy;
                long r = radius - (long)(WorldHash(x, y) % 3);   // chewed edge
                if (dx * dx + dy * dy > r * r) continue;
                world[x][y].ore = oreType;
                // Grow a solid node here?
                if (world[x][y].type == TILE_GRASS &&
                    GetRandomValue(1, 100) <= ORE_NODE_CHANCE) {
                    int sroll = GetRandomValue(1, 100);
                    int size = (sroll <= 30) ? 0 : (sroll <= 55) ? 1 : (sroll <= 80) ? 2 : 3;
                    int corner = GetRandomValue(0, 3);
                    static const float sizeHp[4] = { 1.0f, 0.8f, 0.6f, 0.35f };
                    world[x][y].type    = nodeTile;
                    world[x][y].variant = (unsigned char)(size | (corner << 2));
                    world[x][y].health  = TILES[nodeTile].maxHealth * sizeHp[size];
                }
            }
        }
    }

    // ── Quicksand ─────────────────────────────────────────
    // Soft blobby pools, mostly out in the open ground where you'd
    // be running carelessly. They look like pale sand and they are
    // NOT solid — you walk in, and then you find out.
    for (int i = 0; i < QUICKSAND_PATCHES; i++) {
        int qcx = GetRandomValue(8, worldSize - 9);
        int qcy = GetRandomValue(8, worldSize - 9);
        long dcx = qcx - center, dcy = qcy - center;
        // Never right on top of spawn; the first ten seconds of the
        // game shouldn't be a trap.
        if (dcx * dcx + dcy * dcy < (long)(SPAWN_CLEAR_RADIUS + 10) * (SPAWN_CLEAR_RADIUS + 10)) continue;
        int radius = GetRandomValue(2, 5);
        for (int x = qcx - radius; x <= qcx + radius; x++) {
            for (int y = qcy - radius; y <= qcy + radius; y++) {
                if (x < 1 || x >= worldSize - 1 || y < 1 || y >= worldSize - 1) continue;
                long dx = x - qcx, dy = y - qcy;
                long r = radius - (long)(WorldHash(x, y) % 2);
                if (dx * dx + dy * dy > r * r) continue;
                if (world[x][y].type != TILE_GRASS) continue;   // only open ground
                world[x][y].type   = TILE_QUICKSAND;
                world[x][y].health = TILES[TILE_QUICKSAND].maxHealth;
            }
        }
    }

    // Spawner nests cluster INSIDE each infestation patch.
    for (int i = 0; i < INFESTATION_PATCHES; i++) {
        int nests = GetRandomValue(4, 7);
        for (int n = 0; n < nests; n++) {
            int sx = patchX[i] + GetRandomValue(-INFESTATION_RADIUS + 4, INFESTATION_RADIUS - 4);
            int sy = patchY[i] + GetRandomValue(-INFESTATION_RADIUS + 4, INFESTATION_RADIUS - 4);
            if (sx < 1 || sx >= worldSize - 1 || sy < 1 || sy >= worldSize - 1) continue;
            world[sx][sy].type   = TILE_SPAWNER;
            world[sx][sy].health = TILES[TILE_SPAWNER].maxHealth;
            world[sx][sy].variant = 0;
        }
    }

    // Fresh world → fresh fog. You know only where you're standing.
    memset(worldExplored, 0, sizeof(worldExplored));
    WorldRevealAround((Vector2){ center * (float)TILE_SIZE, center * (float)TILE_SIZE },
                      FOW_REVEAL_TILES + 6);
    worldBreakCount = 0;
    worldHealCursor = 0;
    worldMinimapDirty = true;
}

// ─── Sub-tile collision shapes ────────────────────────────
// What part of tile (x,y) is actually SOLID? Returns 0, 1, or 2
// rectangles in `out`. This one function feeds BOTH collision and
// drawing, so what you see is exactly what you bump into:
//   trees     → only the trunk (walk between canopies)
//   1/4 rock  → nothing (step right over the pebble)
//   1/2 rock  → one half of the tile
//   3/4 rock  → an L shape (two rects) with one open corner
//   full rock, walls, machines → the whole tile
static int WorldSolidRects(int x, int y, Rectangle out[2]) {
    TileType t = world[x][y].type;
    float ts = (float)TILE_SIZE, px = x * ts, py = y * ts;

    if (TILES[t].walkable) return 0;

    if (t == TILE_TREE) {
        float trunk = ts * 0.44f;
        out[0] = (Rectangle){ px + (ts - trunk) / 2, py + (ts - trunk) / 2, trunk, trunk };
        return 1;
    }
    if (TileIsRockLike(t)) {
        int size   = world[x][y].variant & 3;
        int corner = (world[x][y].variant >> 2) & 3;   // 0 TL, 1 TR, 2 BL, 3 BR
        bool right  = (corner & 1) != 0;
        bool bottom = (corner & 2) != 0;
        float hw = ts / 2, hh = ts / 2;
        if (size == 3) return 0;                       // quarter: walk over it
        if (size == 0) { out[0] = (Rectangle){ px, py, ts, ts }; return 1; }
        if (size == 2) {                               // half: one vertical side
            out[0] = (Rectangle){ right ? px + hw : px, py, hw, ts };
            return 1;
        }
        // three-quarter: full tile MINUS the corner square. As an
        // L shape: a full-height column on the mass side + a
        // half-height block beside it on the other side.
        out[0] = (Rectangle){ right ? px : px + hw, py, hw, ts };
        out[1] = (Rectangle){ right ? px + hw : px, bottom ? py : py + hh, hw, hh };
        return 2;
    }
    out[0] = (Rectangle){ px, py, ts, ts };            // everything else: solid
    return 1;
}

// ─── Walkability ──────────────────────────────────────────
// Shape-accurate, with ONE exception: doors block mobs (walkable ==
// false in the table) but the PLAYER is allowed through them.
// That asymmetry is what makes a walled base with a door safe.
static bool WorldPositionWalkableEx(Vector2 pos, bool allowDoors) {
    int minX = (int)((pos.x - PLAYER_RADIUS) / TILE_SIZE);
    int maxX = (int)((pos.x + PLAYER_RADIUS) / TILE_SIZE);
    int minY = (int)((pos.y - PLAYER_RADIUS) / TILE_SIZE);
    int maxY = (int)((pos.y + PLAYER_RADIUS) / TILE_SIZE);

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) continue;
            TileType t = world[x][y].type;
            if (allowDoors && t == TILE_DOOR) continue;

            Rectangle rects[2];
            int n = WorldSolidRects(x, y, rects);
            for (int r = 0; r < n; r++) {
                if (CheckCollisionCircleRec(pos, PLAYER_RADIUS, rects[r])) return false;
            }
        }
    }
    return true;
}

// The player's version (doors open for you)...
static bool WorldPositionWalkable(Vector2 pos) {
    return WorldPositionWalkableEx(pos, true);
}

// ─── Change one tile (also resets its health) ─────────────
static void WorldSetTile(int x, int y, TileType t) {
    if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) return;
    world[x][y].type    = t;
    world[x][y].health  = TILES[t].maxHealth;
    world[x][y].variant = 0;   // shapes belong to generated rocks only
    worldMinimapDirty   = true;
}

// ─── Crumbling ────────────────────────────────────────────
// A rock's SHAPE is derived from its health, not stored separately:
// chip a boulder down and it steps from full → three-quarter → half
// → a pebble you can walk over. The thresholds line up with the
// sizes worldgen hands out, so a rock that generated as a half rock
// reads as a half rock without being damaged at all.
//
// The lovely part is that this feeds straight into WorldSolidRects,
// so a rock you've mined halfway through is ALSO physically smaller:
// the gap opens up before the rock is gone.
static void WorldUpdateRockShape(int x, int y) {
    TileType t = world[x][y].type;
    if (!TileIsRockLike(t)) return;
    float full = TILES[t].maxHealth;
    if (full <= 0) return;
    float frac = world[x][y].health / full;
    int size = (frac > 0.85f) ? 0 : (frac > 0.65f) ? 1 : (frac > 0.45f) ? 2 : 3;
    int corner = (world[x][y].variant >> 2) & 3;
    int wasSize = world[x][y].variant & 3;
    unsigned char next = (unsigned char)(size | (corner << 2));
    if (next == world[x][y].variant) return;
    world[x][y].variant = next;
    // Only the way DOWN makes a noise: a boulder losing a chunk is an
    // event, a boulder slowly healing overnight is not.
    if (size > wasSize) {
        SfxPlayAt(SFX_ROCK_BREAK, (Vector2){ (x + 0.5f) * TILE_SIZE, (y + 0.5f) * TILE_SIZE },
                  worldListener, 0.22f);
    }
}

// ─── Damage a tile; returns true if it broke this call ────
// (Caller decides what to do with the drops — world.h doesn't
// know inventories exist.)
static bool WorldDamageTile(int x, int y, float damage) {
    if (!WorldInBounds(x, y)) return false;
    TileType t = world[x][y].type;
    if (!TILES[t].breakable) return false;
    world[x][y].health -= damage;
    if (world[x][y].health <= 0) {
        WorldSetTile(x, y, TILE_GRASS);   // broken tiles become grass
        WorldPushBreak(x, y, t);          // ...and entities.h makes the mess
        Vector2 at = { (x + 0.5f) * TILE_SIZE, (y + 0.5f) * TILE_SIZE };
        SfxPlayAt(t == TILE_TREE ? SFX_TREE_BREAK : SFX_ROCK_BREAK, at, worldListener, 0.55f);
        return true;
    }
    WorldUpdateRockShape(x, y);           // not dead yet, but visibly chewed
    return false;
}

// ─── Blocks knit themselves back together ─────────────────
// Anything scratched but not destroyed repairs itself, slowly. A
// wall the mobs chewed at last night is whole again by morning; a
// rock you gave up on grows back. The rate is far under any weapon's
// DPS, so it never rescues you mid-fight — it just means the world
// isn't a permanent record of every scuff.
//
// PERFORMANCE — the map is half a million tiles, so we sweep a slice
// per frame with a rolling cursor instead of touching all of it. The
// heal amount is per SWEEP, not per second, which keeps the pace the
// same however fast the machine runs.
static void WorldHealTick(void) {
    int tiles = TILE_HEAL_TILES_PER_FRAME;
    int total = worldSize * worldSize;
    if (total <= 0) return;
    if (tiles > total) tiles = total;
    for (int i = 0; i < tiles; i++) {
        if (worldHealCursor >= total) worldHealCursor = 0;
        int x = worldHealCursor % worldSize;
        int y = worldHealCursor / worldSize;
        worldHealCursor++;

        TileType t = world[x][y].type;
        if (!TILES[t].breakable) continue;
        float full = TILES[t].maxHealth;
        if (world[x][y].health >= full) continue;
        world[x][y].health += TUNE.tileHealRate;
        if (world[x][y].health > full) world[x][y].health = full;
        WorldUpdateRockShape(x, y);       // healed rocks grow back too
        // (Deliberately NOT dirtying the minimap: healing touches
        // thousands of tiles a second and the map only shows tile
        // TYPES, which healing never changes.)
    }
}

// ─── Irregular blobs (rocks, tree canopies) ───────────────
// An organic silhouette: vertices walk a circle at hash-jittered
// radii, so every stone and every canopy has its own outline —
// nothing reads as an egg or a rounded UI box.
//
// C/GRAPHICS NOTE — each wedge is drawn TWICE, with its two
// vertices swapped. Triangle visibility depends on winding order
// (clockwise vs counter-clockwise) once backface culling is on, and
// raylib's 2D Y axis points down, which flips the usual convention.
// Drawing both windings means the shape is visible either way — a
// cheap certainty rather than a guess.
#define BLOB_PTS 11
static void WorldBlobPoints(Vector2 *out, float cx, float cy, float rx, float ry,
                            unsigned int seed, float jagLo, float jagSpan) {
    for (int i = 0; i < BLOB_PTS; i++) {
        unsigned int h = seed ^ (unsigned int)(i * 2654435761u);
        float jag = jagLo + ((h >> 7) & 63) / 63.0f * jagSpan;
        float ang = (i / (float)BLOB_PTS) * 2.0f * PI + (seed % 17) * 0.09f;
        out[i] = (Vector2){ cx + cosf(ang) * rx * jag, cy + sinf(ang) * ry * jag };
    }
}

static void WorldFillBlob(const Vector2 *pts, Vector2 center, Color c) {
    for (int i = 0; i < BLOB_PTS; i++) {
        Vector2 a = pts[i], b = pts[(i + 1) % BLOB_PTS];
        DrawTriangle(center, a, b, c);
        DrawTriangle(center, b, a, c);   // opposite winding — always visible
    }
}

static void WorldOutlineBlob(const Vector2 *pts, Color c, float thick) {
    for (int i = 0; i < BLOB_PTS; i++) {
        DrawLineEx(pts[i], pts[(i + 1) % BLOB_PTS], thick, c);
    }
}

// A faceted boulder: solid body in its own color, then two interior
// facets (lit and shadowed) that follow the same jagged geometry —
// so the shading reads as stone planes, not an airbrushed egg.
static void WorldDrawRockLobe(float cx, float cy, float rx, float ry,
                              unsigned int seed, Color base, Color hi, Color lo, Color rim) {
    Vector2 body[BLOB_PTS], facet[BLOB_PTS];
    Vector2 center = { cx, cy };
    WorldBlobPoints(body, cx, cy, rx, ry, seed, 0.70f, 0.45f);
    WorldFillBlob(body, center, base);

    // Lit facet, offset toward the top-left.
    Vector2 hiC = { cx - rx * 0.22f, cy - ry * 0.26f };
    WorldBlobPoints(facet, hiC.x, hiC.y, rx * 0.52f, ry * 0.44f, seed * 7u + 3u, 0.72f, 0.4f);
    WorldFillBlob(facet, hiC, hi);

    // Shadowed facet along the bottom-right edge.
    Vector2 loC = { cx + rx * 0.26f, cy + ry * 0.30f };
    WorldBlobPoints(facet, loC.x, loC.y, rx * 0.42f, ry * 0.32f, seed * 13u + 7u, 0.7f, 0.4f);
    WorldFillBlob(facet, loC, lo);

    WorldOutlineBlob(body, rim, 1.6f);
}

// ─── Draw the visible tiles ───────────────────────────────
// The world is 384x384 = 147k tiles; drawing them ALL every frame
// would waste ~98% of the work off-screen. The caller (main.c)
// tells us the visible WORLD-space rectangle and we only draw that
// slice ("culling"). TWO passes: ground first for every tile, then
// features (rocks, trees, walls) — so a tree's canopy can hang over
// its neighbors' ground without being painted over.
// `focus` is the player's position: canopies near it fade so you
// can see yourself walking under trees.
static void WorldDraw(Vector2 viewTopLeft, Vector2 viewBottomRight, Vector2 focus) {
    int x0 = (int)(viewTopLeft.x / TILE_SIZE) - 2;
    int y0 = (int)(viewTopLeft.y / TILE_SIZE) - 2;
    int x1 = (int)(viewBottomRight.x / TILE_SIZE) + 2;
    int y1 = (int)(viewBottomRight.y / TILE_SIZE) + 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > WORLD_SIZE - 1) x1 = WORLD_SIZE - 1;
    if (y1 > WORLD_SIZE - 1) y1 = WORLD_SIZE - 1;

    const float ts = (float)TILE_SIZE;

    // PASS 1 — ground everywhere, with hash-seeded speckles for a
    // less flat, higher-resolution look. No grid lines: the texture
    // variation does the "this is terrain" job now.
    for (int x = x0; x <= x1; x++) {
        for (int y = y0; y <= y1; y++) {
            float px = x * ts, py = y * ts;
            Color g = WorldGroundColor(x, y);   // biome + ore-field tint
            DrawRectangle((int)px, (int)py, (int)ts + 1, (int)ts + 1, g);

            unsigned int h = WorldHash(x, y);
            // Two little speckles per tile, deterministic per tile.
            Color darker  = (Color){ (unsigned char)(g.r * 4 / 5), (unsigned char)(g.g * 4 / 5),
                                     (unsigned char)(g.b * 4 / 5), 255 };
            Color lighter = (Color){ (unsigned char)(g.r + (255 - g.r) / 6),
                                     (unsigned char)(g.g + (255 - g.g) / 6),
                                     (unsigned char)(g.b + (255 - g.b) / 6), 255 };
            DrawRectangle((int)(px + (h % 19)), (int)(py + ((h >> 5) % 19)), 2, 2, darker);
            DrawRectangle((int)(px + ((h >> 10) % 19)), (int)(py + ((h >> 15) % 19)), 2, 2, lighter);

            // Ore fields get chunky lumps so they read like ore from
            // a distance, Factorio-style.
            if (world[x][y].ore != 0) {
                DrawRectangle((int)(px + (h % 15)), (int)(py + ((h >> 7) % 15)), 4, 3,
                              ItemArtShade(g, 0.66f));
                DrawRectangle((int)(px + ((h >> 11) % 16)), (int)(py + ((h >> 3) % 16)), 3, 3,
                              ItemArtShade(g, 1.35f));
            }
        }
    }

    // PASS 2 — shadows. Every solid thing drops a soft ellipse (or
    // square, for square things) offset by the current sun angle.
    // A separate pass so shadows always land UNDER features, even a
    // neighbor's.
    for (int x = x0; x <= x1; x++) {
        for (int y = y0; y <= y1; y++) {
            TileType t = world[x][y].type;
            // Flat things cast nothing: grass, a sand pool, the void.
            if (t == TILE_GRASS || t == TILE_QUICKSAND || t == TILE_VOID) continue;
            float px = x * ts, py = y * ts;
            float sx = worldShadowVec.x, sy = worldShadowVec.y;

            if (t == TILE_TREE) {
                float cx = px + ts / 2, cy = py + ts / 2;
                float r1 = ts * (0.50f + (WorldHash(x, y) % 5) * 0.03f);
                DrawEllipse((int)(cx + sx), (int)(cy + sy), r1 * 1.05f, r1 * 0.85f,
                            WORLD_SHADOW_COLOR);
            } else if (TileIsRockLike(t)) {
                Rectangle rects[2];
                int n = WorldSolidRects(x, y, rects);
                for (int r = 0; r < n; r++) {
                    DrawEllipse((int)(rects[r].x + rects[r].width / 2 + sx * 0.8f),
                                (int)(rects[r].y + rects[r].height / 2 + sy * 0.8f),
                                rects[r].width * 0.55f, rects[r].height * 0.5f,
                                WORLD_SHADOW_COLOR);
                }
                // Quarter pebbles are low — barely any shadow.
            } else {
                // Walls, machines, spawners: square-ish shadow.
                DrawRectangle((int)(px + sx * 0.7f), (int)(py + sy * 0.7f),
                              (int)ts, (int)ts, WORLD_SHADOW_COLOR);
            }
        }
    }

    // PASS 3 — the features themselves. Mining damage TINTS the
    // entity's own pixels darker (no more square overlay boxes) —
    // the darkening has exactly the shape of the tree or boulder.
    for (int x = x0; x <= x1; x++) {
        for (int y = y0; y <= y1; y++) {
            TileType t = world[x][y].type;
            if (t == TILE_GRASS) continue;
            const TileInfo *info = &TILES[t];
            float px = x * ts, py = y * ts;
            unsigned int h = WorldHash(x, y);

            // 1.0 = healthy, sinking toward 0.4 as it's mined out.
            float lum = 1.0f;
            if (info->maxHealth > 1 && world[x][y].health < info->maxHealth) {
                lum = 0.4f + 0.6f * (world[x][y].health / info->maxHealth);
            }

            if (TileIsRockLike(t)) {
                // Jagged boulders drawn FROM the collision rects, so
                // looks match physics. Ore nodes use the same shapes
                // in their ore's color, with glints on top.
                Rectangle rects[2];
                int n = WorldSolidRects(x, y, rects);
                Color stone = (t == TILE_ROCK)
                    ? (Color){ (unsigned char)(102 + h % 26), (unsigned char)(102 + h % 26),
                               (unsigned char)(112 + h % 18), 255 }
                    : TILES[t].color;
                Color base = ItemArtShade(stone, lum);
                Color hi   = ItemArtShade(base, 1.28f);
                Color lo   = ItemArtShade(base, 0.72f);
                Color rim  = ItemArtShade((Color){ 46, 46, 54, 255 }, lum);
                if (n == 0) {
                    // Quarter pebble: a small stone in its corner.
                    int corner = (world[x][y].variant >> 2) & 3;
                    float cx = px + ((corner & 1) ? ts * 0.68f : ts * 0.32f);
                    float cy = py + ((corner & 2) ? ts * 0.68f : ts * 0.32f);
                    WorldDrawRockLobe(cx, cy, ts * 0.23f, ts * 0.19f, h, base, hi, lo, rim);
                } else {
                    for (int r = 0; r < n; r++) {
                        float cx = rects[r].x + rects[r].width / 2;
                        float cy = rects[r].y + rects[r].height / 2;
                        // jitter each lobe a touch so no two match
                        cx += (float)(h % 5) - 2;  cy += (float)((h >> 3) % 5) - 2;
                        WorldDrawRockLobe(cx, cy, rects[r].width * 0.52f,
                                          rects[r].height * 0.50f, h, base, hi, lo, rim);
                        h = h * 1664525u + 1013904223u;   // next lobe differs
                    }
                }
                if (t != TILE_ROCK) {
                    // Bright mineral glints — ore should sparkle.
                    Color glint = ItemArtShade(TILES[t].color, 1.6f);
                    DrawRectangle((int)(px + 4 + (h % 10)), (int)(py + 5 + ((h >> 4) % 10)),
                                  3, 3, glint);
                    DrawRectangle((int)(px + ts * 0.55f), (int)(py + ts * 0.3f), 2, 2,
                                  (Color){ 255, 255, 255, 210 });
                }
            } else if (t == TILE_TREE) {
                // Factorio-scale tree: a tapered trunk plus a ragged
                // canopy built from irregular blobs — dark underside,
                // mid body, sunlit crown — so it reads as foliage
                // rather than a green egg. Fades when you stand close.
                float cx = px + ts / 2, cy = py + ts / 2;
                Color bark = ItemArtShade((Color){ 88, 58, 32, 255 }, lum);
                DrawRectangleRounded((Rectangle){ cx - ts * 0.10f, cy - ts * 0.05f,
                                     ts * 0.20f, ts * 0.40f }, 0.4f, 4, bark);
                DrawRectangle((int)(cx - ts * 0.02f), (int)(cy - ts * 0.05f),
                              2, (int)(ts * 0.38f), ItemArtShade(bark, 0.72f));

                float d = Vector2Distance(focus, (Vector2){ cx, cy });
                float fade = (d < ts * 2.6f) ? 0.32f + 0.68f * (d / (ts * 2.6f)) : 1.0f;
                unsigned char a = (unsigned char)(238 * fade);

                Color leaf = (world[x][y].biome == BIOME_FOREST)
                           ? (Color){ 30, 92, 42, 255 } : (Color){ 44, 116, 52, 255 };
                leaf = ItemArtShade(leaf, lum);
                Color leafDark = ItemArtShade(leaf, 0.66f);
                Color leafMid  = leaf;
                Color leafLit  = ItemArtShade(leaf, 1.32f);
                leafDark.a = a; leafMid.a = a; leafLit.a = a;

                Vector2 blob[BLOB_PTS];
                float r1 = ts * (0.48f + (h % 5) * 0.028f);
                // shaded underside, offset down
                Vector2 c0 = { cx, cy - ts * 0.04f };
                WorldBlobPoints(blob, c0.x, c0.y, r1, r1 * 0.84f, h, 0.80f, 0.34f);
                WorldFillBlob(blob, c0, leafDark);
                // main body, lifted
                Vector2 c1 = { cx - ts * 0.02f, cy - ts * 0.14f };
                WorldBlobPoints(blob, c1.x, c1.y, r1 * 0.90f, r1 * 0.78f, h * 3u + 1u, 0.82f, 0.32f);
                WorldFillBlob(blob, c1, leafMid);
                // sunlit crown toward the top-left
                Vector2 c2 = { cx - ts * 0.13f, cy - ts * 0.24f };
                WorldBlobPoints(blob, c2.x, c2.y, r1 * 0.50f, r1 * 0.42f, h * 11u + 5u, 0.80f, 0.34f);
                WorldFillBlob(blob, c2, leafLit);
            } else if (t == TILE_WALL || t == TILE_METAL_WALL) {
                // Bevelled block: light top edge, dark bottom edge.
                Color wallC = ItemArtShade(info->color, lum);
                DrawRectangle((int)px, (int)py, (int)ts, (int)ts, wallC);
                DrawRectangle((int)px, (int)py, (int)ts, 3, ItemArtShade(wallC, 1.22f));
                DrawRectangle((int)px, (int)(py + ts - 3), (int)ts, 3, ItemArtShade(wallC, 0.6f));
            } else if (t == TILE_QUICKSAND) {
                // Pale sand with slow concentric ripples. It reads as
                // "ground, but wrong" — which is the whole trick.
                DrawRectangle((int)px, (int)py, (int)ts + 1, (int)ts + 1, info->color);
                float wob = sinf((float)GetTime() * 1.3f + (h % 100) * 0.06f);
                Color ring = ItemArtShade(info->color, 0.82f);
                DrawEllipseLines((int)(px + ts / 2), (int)(py + ts / 2),
                                 ts * (0.34f + 0.05f * wob), ts * (0.26f + 0.04f * wob), ring);
                DrawEllipseLines((int)(px + ts / 2), (int)(py + ts / 2),
                                 ts * (0.18f - 0.04f * wob), ts * (0.13f - 0.03f * wob), ring);
                DrawRectangle((int)(px + (h % 17)), (int)(py + ((h >> 6) % 17)), 2, 2,
                              ItemArtShade(info->color, 1.2f));
            } else if (t == TILE_VOID) {
                // The edge of the world: empty space with a few cold
                // stars, so a shrunk map has an honest border instead
                // of a wall of nothing.
                DrawRectangle((int)px, (int)py, (int)ts + 1, (int)ts + 1, info->color);
                if ((h % 11) == 0) {
                    DrawRectangle((int)(px + (h % 19)), (int)(py + ((h >> 8) % 19)), 2, 2,
                                  (Color){ 200, 210, 240, (unsigned char)(90 + h % 120) });
                }
            } else if (t == TILE_SPAWNER) {
                // Organic nest mound (entities.h adds the pulse).
                DrawCircleV((Vector2){ px + ts / 2, py + ts / 2 }, ts * 0.46f,
                            ItemArtShade((Color){ 74, 30, 88, 255 }, lum));
                DrawCircleV((Vector2){ px + ts / 3, py + ts / 3 }, ts * 0.2f,
                            ItemArtShade((Color){ 96, 44, 110, 255 }, lum));
            } else {
                // Machines/doors: a base plate; entities.h draws the
                // item sprite and direction arrows on top.
                DrawRectangleRounded((Rectangle){ px + 1, py + 1, ts - 2, ts - 2 },
                                     0.2f, 4, ItemArtShade(info->color, lum));
            }
        }
    }
}

// ─── Minimap ──────────────────────────────────────────────
// One texture, one pixel per tile. Rebuilt ONLY when the world
// changes (worldMinimapDirty), then drawn scaled-down each frame.
static Texture2D worldMinimapTex = { 0 };
static Color     worldMinimapPixels[WORLD_SIZE * WORLD_SIZE];

// Call once after InitWindow (textures need a live GPU context).
static void WorldMinimapInit(void) {
    Image img = GenImageColor(WORLD_SIZE, WORLD_SIZE, BLACK);
    worldMinimapTex = LoadTextureFromImage(img);
    UnloadImage(img);
    worldMinimapDirty = true;
}

// Call once per frame; cheap when nothing changed, and throttled to
// 5 rebuilds/sec while exploring (revealing fog dirties it a lot).
static void WorldMinimapRefresh(void) {
    static double lastBuild = -1.0;
    if (!worldMinimapDirty || worldMinimapTex.id == 0) return;
    if (GetTime() - lastBuild < 0.2) return;
    lastBuild = GetTime();
    for (int y = 0; y < WORLD_SIZE; y++) {
        for (int x = 0; x < WORLD_SIZE; x++) {
            Color c;
            if (!worldExplored[x][y]) {
                c = (Color){ 8, 8, 14, 255 };   // fog of war: never seen
            } else {
                TileType t = world[x][y].type;
                c = (t == TILE_GRASS) ? WorldGroundColor(x, y) : TILES[t].color;
                if (t == TILE_SPAWNER) c = (Color){ 220, 60, 220, 255 };  // nests pop
            }
            worldMinimapPixels[y * WORLD_SIZE + x] = c;   // images are row-major
        }
    }
    UpdateTexture(worldMinimapTex, worldMinimapPixels);
    worldMinimapDirty = false;
}

// ─── Save file check ──────────────────────────────────────
// (WorldSave/WorldLoad used to live here — the old world-only save
// format. GameSave/GameLoad in main.c replaced them, and saves.h now
// keeps eight of them in a slots directory. All that's left here is
// "does this path open?", which is the one question about a save
// file that needs nothing but stdio.)
static bool WorldHasSaveFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return false;
    fclose(f);
    return true;
}

#endif // WORLD_H
