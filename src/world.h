#ifndef WORLD_H
#define WORLD_H

#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================
//  WORLD.H — Home Planet World Engine
// ============================================================

#define WORLD_SIZE      100
#define TILE_SIZE       60
#define PLAYER_REACH    150.0f
#define MAX_BELT_ITEMS  4
#define SAVE_FILE       "homeplanet.sav"
#define BLUEPRINT_AREA  5

// ─── TILE TYPES ───────────────────────────────────────────
typedef enum {
    TILE_EMPTY = 0,
    TILE_GRASS,
    TILE_DIRT,
    TILE_WOOD,
    TILE_STONE,
    TILE_TOWER,
    TILE_ROCKET_PAD,
    TILE_CONVEYOR_N,
    TILE_CONVEYOR_S,
    TILE_CONVEYOR_E,
    TILE_CONVEYOR_W,
    TILE_MINING_DRILL,
    TILE_SERVER,
    TILE_COOLING_FAN,
    TILE_CHEST,
    TILE_COUNT
} TileType;

typedef struct {
    int   itemId;
    float progress;
} BeltSlot;

typedef struct {
    TileType  type;
    float     health;
    float     maxHealth;
    Color     color;
    float     mineTimer;
    BeltSlot  belt[MAX_BELT_ITEMS];
    int       direction;
} Tile;

static Tile  world[WORLD_SIZE][WORLD_SIZE];
static float globalBandwidth = 0.0f;

static Color TileDefaultColor(TileType t) {
    switch (t) {
        case TILE_GRASS:        return (Color){40,150,60,255};
        case TILE_DIRT:         return (Color){120,80,40,255};
        case TILE_WOOD:         return BROWN;
        case TILE_STONE:        return DARKGRAY;
        case TILE_TOWER:        return YELLOW;
        case TILE_ROCKET_PAD:   return SKYBLUE;
        case TILE_CONVEYOR_N:
        case TILE_CONVEYOR_S:
        case TILE_CONVEYOR_E:
        case TILE_CONVEYOR_W:   return (Color){180,140,60,255};
        case TILE_MINING_DRILL: return (Color){80,160,200,255};
        case TILE_SERVER:       return (Color){60,60,180,255};
        case TILE_COOLING_FAN:  return (Color){100,220,220,255};
        case TILE_CHEST:        return (Color){180,120,30,255};
        default:                return BLACK;
    }
}

static void WorldInit(void) {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            int r = GetRandomValue(0, 100);
            world[x][y] = (Tile){0};
            world[x][y].type      = TILE_GRASS;
            world[x][y].color     = TileDefaultColor(TILE_GRASS);
            world[x][y].health    = 1;
            world[x][y].maxHealth = 1;

            if (r > 92) {
                world[x][y].type      = TILE_WOOD;
                world[x][y].color     = BROWN;
                world[x][y].health    = 4;
                world[x][y].maxHealth = 4;
            } else if (r < 4) {
                world[x][y].type      = TILE_STONE;
                world[x][y].color     = DARKGRAY;
                world[x][y].health    = 8;
                world[x][y].maxHealth = 8;
            }
        }
    }
    int cx = WORLD_SIZE/2 + 2, cy = WORLD_SIZE/2 + 2;
    world[cx][cy].type      = TILE_ROCKET_PAD;
    world[cx][cy].color     = SKYBLUE;
    world[cx][cy].health    = 999;
    world[cx][cy].maxHealth = 999;
}

static void WorldSetTile(int x, int y, TileType t) {
    if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) return;
    float hp = (t == TILE_STONE) ? 8
             : (t == TILE_WOOD)  ? 4
             : (t == TILE_TOWER || t == TILE_MINING_DRILL) ? 10
             : 1;
    world[x][y].type      = t;
    world[x][y].color     = TileDefaultColor(t);
    world[x][y].health    = hp;
    world[x][y].maxHealth = hp;
    world[x][y].mineTimer = 0.0f;
    for (int i = 0; i < MAX_BELT_ITEMS; i++) {
        world[x][y].belt[i].itemId   = 0;
        world[x][y].belt[i].progress = 0.0f;
    }
}

static int WorldTickMiner(int x, int y, float dt) {
    if (world[x][y].type != TILE_MINING_DRILL) return 0;
    world[x][y].mineTimer += dt;
    if (world[x][y].mineTimer < 3.0f) return 0;
    world[x][y].mineTimer = 0.0f;
    int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    for (int d = 0; d < 4; d++) {
        int nx = x + dirs[d][0], ny = y + dirs[d][1];
        if (nx < 0 || nx >= WORLD_SIZE || ny < 0 || ny >= WORLD_SIZE) continue;
        if (world[nx][ny].type == TILE_WOOD)  return 1;
        if (world[nx][ny].type == TILE_STONE) return 2;
    }
    return 0;
}

static void WorldTickConveyors(float dt) {
    float speed = 0.5f;
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            TileType t = world[x][y].type;
            if (t < TILE_CONVEYOR_N || t > TILE_CONVEYOR_W) continue;
            for (int s = 0; s < MAX_BELT_ITEMS; s++) {
                if (world[x][y].belt[s].itemId == 0) continue;
                world[x][y].belt[s].progress += dt * speed;
                if (world[x][y].belt[s].progress >= 1.0f) {
                    int nx = x, ny = y;
                    if (t == TILE_CONVEYOR_N) ny--;
                    if (t == TILE_CONVEYOR_S) ny++;
                    if (t == TILE_CONVEYOR_E) nx++;
                    if (t == TILE_CONVEYOR_W) nx--;
                    world[x][y].belt[s].progress = 0.0f;
                    int id = world[x][y].belt[s].itemId;
                    world[x][y].belt[s].itemId = 0;
                    if (nx >= 0 && nx < WORLD_SIZE && ny >= 0 && ny < WORLD_SIZE) {
                        TileType nt = world[nx][ny].type;
                        if (nt >= TILE_CONVEYOR_N && nt <= TILE_CONVEYOR_W) {
                            for (int ns = 0; ns < MAX_BELT_ITEMS; ns++) {
                                if (world[nx][ny].belt[ns].itemId == 0) {
                                    world[nx][ny].belt[ns].itemId   = id;
                                    world[nx][ny].belt[ns].progress = 0.0f;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static void WorldDraw(void) {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            Rectangle r = { (float)(x * TILE_SIZE), (float)(y * TILE_SIZE),
                            TILE_SIZE - 1.0f, TILE_SIZE - 1.0f };
            DrawRectangleRec(r, world[x][y].color);

            float cx = r.x + TILE_SIZE / 2.0f;
            float cy = r.y + TILE_SIZE / 2.0f;
            float t  = (float)GetTime();

            switch (world[x][y].type) {
                case TILE_TOWER:
                    DrawCircle(cx, cy, 10, GOLD);
                    DrawCircleLines(cx, cy, 15 + (int)(t * 10) % 20, YELLOW);
                    break;
                case TILE_ROCKET_PAD:
                    DrawRectangleLinesEx(r, 3, WHITE);
                    DrawText("PAD", (int)cx - 18, (int)cy - 8, 16, WHITE);
                    break;
                case TILE_MINING_DRILL:
                    DrawCircle(cx, cy, 12, (Color){80, 200, 255, 255});
                    DrawLine(cx, cy, cx + cosf(t*3)*20, cy + sinf(t*3)*20,
                             (Color){200, 240, 255, 255});
                    break;
                case TILE_CONVEYOR_N: DrawText("^", (int)cx-6, (int)cy-12, 24, WHITE); break;
                case TILE_CONVEYOR_S: DrawText("v", (int)cx-6, (int)cy-12, 24, WHITE); break;
                case TILE_CONVEYOR_E: DrawText(">", (int)cx-6, (int)cy-12, 24, WHITE); break;
                case TILE_CONVEYOR_W: DrawText("<", (int)cx-6, (int)cy-12, 24, WHITE); break;
                case TILE_CHEST:
                    DrawRectangle(cx - 15, cy - 12, 30, 24, (Color){180, 120, 30, 255});
                    DrawRectangleLines(cx - 15, cy - 12, 30, 24, WHITE);
                    break;
                case TILE_WOOD:
                    DrawCircle(cx, cy, 16, BROWN);
                    DrawCircle(cx, cy, 8, DARKBROWN);
                    break;
                case TILE_STONE:
                    DrawPoly((Vector2){cx, cy}, 6, 16, 30.0f, DARKGRAY);
                    break;
                default: break;
            }

            // Damage / cracks as block is mined
            if (world[x][y].health < world[x][y].maxHealth && world[x][y].maxHealth > 1) {
                float frac = 1.0f - (world[x][y].health / world[x][y].maxHealth);
                unsigned char a = (unsigned char)(frac * 180);
                DrawRectangleRec(r, (Color){0, 0, 0, a});
                DrawLine(r.x + 8, r.y + 10, r.x + TILE_SIZE - 10, r.y + TILE_SIZE - 14,
                         (Color){255,255,255,a});
                if (frac > 0.5f)
                    DrawLine(r.x + TILE_SIZE - 12, r.y + 8, r.x + 10, r.y + TILE_SIZE - 12,
                             (Color){255,255,255,a});
            }

            if (world[x][y].type >= TILE_CONVEYOR_N && world[x][y].type <= TILE_CONVEYOR_W) {
                for (int s = 0; s < MAX_BELT_ITEMS; s++) {
                    if (world[x][y].belt[s].itemId == 0) continue;
                    Color ic = (world[x][y].belt[s].itemId == 1) ? BROWN : GRAY;
                    float bx = r.x + TILE_SIZE * world[x][y].belt[s].progress;
                    DrawCircle(bx, cy, 6, ic);
                }
            }
        }
    }
}

static void WorldSave(void) {
    FILE *f = fopen(SAVE_FILE, "wb");
    if (!f) return;
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            fwrite(&world[x][y].type,      sizeof(int),   1, f);
            fwrite(&world[x][y].health,    sizeof(float), 1, f);
            fwrite(&world[x][y].maxHealth, sizeof(float), 1, f);
        }
    }
    fclose(f);
}

static void WorldLoad(void) {
    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) return;
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            int type;
            float hp, mhp;
            fread(&type, sizeof(int),   1, f);
            fread(&hp,   sizeof(float), 1, f);
            fread(&mhp,  sizeof(float), 1, f);
            WorldSetTile(x, y, (TileType)type);
            world[x][y].health    = hp;
            world[x][y].maxHealth = mhp;
        }
    }
    fclose(f);
}

#define BP_CHARS "0123456789ABCDEF"

static void WorldCaptureBlueprint(int ox, int oy, char *out, int outLen) {
    int pos = 0;
    if (outLen < 30) { out[0]='\0'; return; }
    out[pos++] = 'B'; out[pos++] = 'P'; out[pos++] = ':';
    for (int dy = 0; dy < BLUEPRINT_AREA && pos < outLen-1; dy++) {
        for (int dx = 0; dx < BLUEPRINT_AREA && pos < outLen-1; dx++) {
            int wx = ox + dx, wy = oy + dy;
            int t = 0;
            if (wx >= 0 && wx < WORLD_SIZE && wy >= 0 && wy < WORLD_SIZE)
                t = (int)world[wx][wy].type;
            if (t >= 16) t = 15;
            out[pos++] = BP_CHARS[t];
        }
    }
    out[pos] = '\0';
}

static void WorldPasteBlueprint(int ox, int oy, const char *code,
                                 int *woodCost, int *stoneCost) {
    *woodCost = 0; *stoneCost = 0;
    if (!code || code[0]!='B' || code[1]!='P' || code[2]!=':') return;
    const char *data = code + 3;
    int len = strlen(data);
    for (int i = 0; i < len && i < BLUEPRINT_AREA*BLUEPRINT_AREA; i++) {
        char c = data[i];
        int t = 0;
        if (c >= '0' && c <= '9') t = c - '0';
        else if (c >= 'A' && c <= 'F') t = 10 + (c - 'A');
        int dx = i % BLUEPRINT_AREA, dy = i / BLUEPRINT_AREA;
        int wx = ox+dx, wy = oy+dy;
        if (wx < 0 || wx >= WORLD_SIZE || wy < 0 || wy >= WORLD_SIZE) continue;
        TileType tt = (TileType)t;
        if (tt == TILE_TOWER) *woodCost += 20;
        if (tt >= TILE_CONVEYOR_N && tt <= TILE_CONVEYOR_W) *woodCost += 5;
        if (tt == TILE_MINING_DRILL) *stoneCost += 20;
        if (tt == TILE_CHEST) *woodCost += 10;
    }
}

static void WorldApplyBlueprint(int ox, int oy, const char *code) {
    if (!code || code[0]!='B' || code[1]!='P' || code[2]!=':') return;
    const char *data = code + 3;
    int len = strlen(data);
    for (int i = 0; i < len && i < BLUEPRINT_AREA*BLUEPRINT_AREA; i++) {
        char c = data[i];
        int t = 0;
        if (c >= '0' && c <= '9') t = c - '0';
        else if (c >= 'A' && c <= 'F') t = 10 + (c - 'A');
        int dx = i % BLUEPRINT_AREA, dy = i / BLUEPRINT_AREA;
        WorldSetTile(ox+dx, oy+dy, (TileType)t);
    }
}

#endif // WORLD_H
