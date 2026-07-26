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
} Tile;

// C CONCEPT — 2D array: WORLD_SIZE * WORLD_SIZE Tiles in one block
// of memory. world[x][y] picks one. ~1.8MB at 384x384 — still fine.
static Tile world[WORLD_SIZE][WORLD_SIZE];

// Fog of war: which tiles has the player ever SEEN? Drives the
// minimap and the hold-G map; the live view is never fogged.
static bool worldExplored[WORLD_SIZE][WORLD_SIZE];

// The minimap needs re-rendering whenever a tile changes; drawing
// 147,456 rectangles every frame would not fly, so we keep a texture
// and only rebuild it when this flag says the world changed.
static bool worldMinimapDirty = true;

// Deterministic per-tile "randomness" for visual variety (speckles,
// canopy sizes). Same x,y → same number, every frame, no storage.
static unsigned int WorldHash(int x, int y) {
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
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

// ─── Generate a fresh world ───────────────────────────────
// TECHNIQUE — Voronoi biomes: scatter random "seed points", each
// carrying a biome; every tile takes the biome of its NEAREST seed.
// Enemy territory is different: a handful of BIG circular
// infestation patches placed far from spawn, like biter bases.
#define BIOME_SEEDS 22
static void WorldInit(void) {
    int seedX[BIOME_SEEDS], seedY[BIOME_SEEDS];
    Biome seedB[BIOME_SEEDS];
    for (int i = 0; i < BIOME_SEEDS; i++) {
        seedX[i] = GetRandomValue(0, WORLD_SIZE - 1);
        seedY[i] = GetRandomValue(0, WORLD_SIZE - 1);
        int roll = GetRandomValue(1, 100);   // meadow half, rest split
        seedB[i] = (roll <= 50) ? BIOME_MEADOW : (roll <= 75) ? BIOME_FOREST : BIOME_ROCKLANDS;
    }

    // Infestation patch centers: far from the center spawn, not
    // hugging the border. MATH — random angle + random distance in
    // [0.33, 0.46] of the world span puts them "out there" without
    // being predictable.
    int center = WORLD_SIZE / 2;
    int patchX[INFESTATION_PATCHES], patchY[INFESTATION_PATCHES];
    for (int i = 0; i < INFESTATION_PATCHES; i++) {
        float angle = GetRandomValue(0, 6283) / 1000.0f;   // 0..2π
        float dist  = WORLD_SIZE * (0.33f + GetRandomValue(0, 130) / 1000.0f);
        int px = center + (int)(cosf(angle) * dist);
        int py = center + (int)(sinf(angle) * dist);
        if (px < INFESTATION_RADIUS + 4) px = INFESTATION_RADIUS + 4;
        if (py < INFESTATION_RADIUS + 4) py = INFESTATION_RADIUS + 4;
        if (px > WORLD_SIZE - INFESTATION_RADIUS - 4) px = WORLD_SIZE - INFESTATION_RADIUS - 4;
        if (py > WORLD_SIZE - INFESTATION_RADIUS - 4) py = WORLD_SIZE - INFESTATION_RADIUS - 4;
        patchX[i] = px; patchY[i] = py;
    }

    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
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

    // Spawner nests cluster INSIDE each infestation patch.
    for (int i = 0; i < INFESTATION_PATCHES; i++) {
        int nests = GetRandomValue(4, 7);
        for (int n = 0; n < nests; n++) {
            int sx = patchX[i] + GetRandomValue(-INFESTATION_RADIUS + 4, INFESTATION_RADIUS - 4);
            int sy = patchY[i] + GetRandomValue(-INFESTATION_RADIUS + 4, INFESTATION_RADIUS - 4);
            if (sx < 1 || sx >= WORLD_SIZE - 1 || sy < 1 || sy >= WORLD_SIZE - 1) continue;
            world[sx][sy].type   = TILE_SPAWNER;
            world[sx][sy].health = TILES[TILE_SPAWNER].maxHealth;
            world[sx][sy].variant = 0;
        }
    }

    // Fresh world → fresh fog. You know only where you're standing.
    memset(worldExplored, 0, sizeof(worldExplored));
    WorldRevealAround((Vector2){ center * (float)TILE_SIZE, center * (float)TILE_SIZE },
                      FOW_REVEAL_TILES + 6);
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
    if (t == TILE_ROCK) {
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

// ─── Damage a tile; returns true if it broke this call ────
// (Caller decides what to do with the drops — world.h doesn't
// know inventories exist.)
static bool WorldDamageTile(int x, int y, float damage) {
    if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) return false;
    if (!TILES[world[x][y].type].breakable) return false;
    world[x][y].health -= damage;
    if (world[x][y].health <= 0) {
        WorldSetTile(x, y, TILE_GRASS);   // broken tiles become grass
        return true;
    }
    return false;
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
            Color g = BIOME_GROUND[world[x][y].biome];
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
        }
    }

    // PASS 2 — features on top of the ground.
    for (int x = x0; x <= x1; x++) {
        for (int y = y0; y <= y1; y++) {
            TileType t = world[x][y].type;
            if (t == TILE_GRASS) continue;
            const TileInfo *info = &TILES[t];
            float px = x * ts, py = y * ts;
            unsigned int h = WorldHash(x, y);

            if (t == TILE_ROCK) {
                // Boulders drawn FROM their collision rects, so the
                // visible stone exactly matches what blocks you.
                Rectangle rects[2];
                int n = WorldSolidRects(x, y, rects);
                Color base = (Color){ (unsigned char)(96 + h % 24),
                                      (unsigned char)(96 + h % 24),
                                      (unsigned char)(104 + h % 20), 255 };
                if (n == 0) {
                    // Quarter pebble: small stone in its corner.
                    int corner = (world[x][y].variant >> 2) & 3;
                    float qx = px + ((corner & 1) ? ts * 0.5f : ts * 0.1f);
                    float qy = py + ((corner & 2) ? ts * 0.5f : ts * 0.1f);
                    Rectangle q = { qx, qy, ts * 0.4f, ts * 0.4f };
                    DrawRectangleRounded(q, 0.6f, 6, base);
                    DrawRectangleRoundedLines(q, 0.6f, 6, (Color){ 40, 40, 48, 255 });
                } else {
                    for (int r = 0; r < n; r++) {
                        Rectangle rr = { rects[r].x + 1, rects[r].y + 1,
                                         rects[r].width - 2, rects[r].height - 2 };
                        DrawRectangleRounded(rr, 0.35f, 6, base);
                        // top-left light, bottom-right shade = cheap 3D
                        DrawRectangleRounded((Rectangle){ rr.x + 2, rr.y + 2,
                                             rr.width * 0.45f, rr.height * 0.35f }, 0.6f, 4,
                                             (Color){ (unsigned char)(base.r + 26),
                                                      (unsigned char)(base.g + 26),
                                                      (unsigned char)(base.b + 26), 255 });
                        DrawRectangleRoundedLines(rr, 0.35f, 6, (Color){ 40, 40, 48, 255 });
                    }
                }
            } else if (t == TILE_TREE) {
                // Factorio-scale tree: small trunk + a canopy of a
                // few circles that FADES when you stand close.
                float cx = px + ts / 2, cy = py + ts / 2;
                DrawRectangleRounded((Rectangle){ cx - ts * 0.14f, cy - ts * 0.16f,
                                     ts * 0.28f, ts * 0.42f }, 0.5f, 4,
                                     (Color){ 92, 62, 34, 255 });
                float d = Vector2Distance(focus, (Vector2){ cx, cy });
                float fade = (d < ts * 2.6f) ? 0.35f + 0.65f * (d / (ts * 2.6f)) : 1.0f;
                unsigned char a = (unsigned char)(235 * fade);
                Color leaf = (world[x][y].biome == BIOME_FOREST)
                           ? (Color){ 24, 96, 40, a } : (Color){ 38, 122, 48, a };
                Color leafLight = (Color){ (unsigned char)(leaf.r + 22),
                                           (unsigned char)(leaf.g + 26),
                                           (unsigned char)(leaf.b + 18), a };
                float r1 = ts * (0.50f + (h % 5) * 0.03f);
                DrawCircleV((Vector2){ cx, cy - ts * 0.14f }, r1, leaf);
                DrawCircleV((Vector2){ cx - ts * 0.28f, cy + ts * 0.05f }, r1 * 0.62f, leaf);
                DrawCircleV((Vector2){ cx + ts * 0.26f, cy + ts * 0.02f }, r1 * 0.66f, leaf);
                DrawCircleV((Vector2){ cx - ts * 0.08f, cy - ts * 0.22f }, r1 * 0.45f, leafLight);
            } else if (t == TILE_WALL || t == TILE_METAL_WALL) {
                // Bevelled block: light top edge, dark bottom edge.
                DrawRectangle((int)px, (int)py, (int)ts, (int)ts, info->color);
                DrawRectangle((int)px, (int)py, (int)ts, 3,
                              (Color){ (unsigned char)(info->color.r + 34),
                                       (unsigned char)(info->color.g + 34),
                                       (unsigned char)(info->color.b + 34), 255 });
                DrawRectangle((int)px, (int)(py + ts - 3), (int)ts, 3,
                              (Color){ (unsigned char)(info->color.r * 3 / 5),
                                       (unsigned char)(info->color.g * 3 / 5),
                                       (unsigned char)(info->color.b * 3 / 5), 255 });
            } else if (t == TILE_SPAWNER) {
                // Organic nest mound (entities.h adds the pulse).
                DrawCircleV((Vector2){ px + ts / 2, py + ts / 2 }, ts * 0.46f,
                            (Color){ 74, 30, 88, 255 });
                DrawCircleV((Vector2){ px + ts / 3, py + ts / 3 }, ts * 0.2f,
                            (Color){ 96, 44, 110, 255 });
            } else {
                // Machines/doors: a base plate; entities.h draws the
                // item sprite and direction arrows on top.
                DrawRectangleRounded((Rectangle){ px + 1, py + 1, ts - 2, ts - 2 },
                                     0.2f, 4, info->color);
            }

            // Darken partially-mined tiles so damage is visible.
            float maxHp = info->maxHealth;
            if (maxHp > 1 && world[x][y].health < maxHp) {
                float missing = 1.0f - (world[x][y].health / maxHp);
                if (missing > 0) {
                    DrawRectangle((int)px, (int)py, (int)ts, (int)ts,
                                  (Color){ 0, 0, 0, (unsigned char)(missing * 150) });
                }
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
                c = (t == TILE_GRASS) ? BIOME_GROUND[world[x][y].biome] : TILES[t].color;
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
// format. GameSave/GameLoad in main.c replaced them; GameLoad can
// still READ old world-only files by recognizing their size.)
static bool WorldHasSave(void) {
    FILE *f = fopen(SAVE_FILE, "rb");
    if (f == NULL) return false;
    fclose(f);
    return true;
}

#endif // WORLD_H
