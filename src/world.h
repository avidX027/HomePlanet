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
#include "raylib.h"
#include "config.h"
#include "gamedata.h"

// Each cell only stores what VARIES per tile: its type and current
// health. Color/name/maxHealth are looked up in TILES[] — derive,
// don't copy. Copied data is data that can go stale.
typedef struct {
    TileType type;
    float    health;
} Tile;

// C CONCEPT — 2D array: WORLD_SIZE * WORLD_SIZE Tiles in one block
// of memory. world[x][y] picks one. This is ~32KB for 64x64.
static Tile world[WORLD_SIZE][WORLD_SIZE];

// ─── Generate a fresh world ───────────────────────────────
static void WorldInit(void) {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            int roll = GetRandomValue(1, 100);          // 1..100 dice
            TileType t = TILE_GRASS;
            if      (roll <= TREE_CHANCE)               t = TILE_TREE;
            else if (roll <= TREE_CHANCE + ROCK_CHANCE) t = TILE_ROCK;
            world[x][y].type   = t;
            world[x][y].health = TILES[t].maxHealth;
        }
    }
}

// ─── Change one tile (also resets its health) ─────────────
static void WorldSetTile(int x, int y, TileType t) {
    if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_SIZE) return;
    world[x][y].type   = t;
    world[x][y].health = TILES[t].maxHealth;
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

// ─── Draw every tile ──────────────────────────────────────
static void WorldDraw(void) {
    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int y = 0; y < WORLD_SIZE; y++) {
            const TileInfo *info = &TILES[world[x][y].type];
            Rectangle r = { (float)(x * TILE_SIZE), (float)(y * TILE_SIZE),
                            TILE_SIZE - 1.0f, TILE_SIZE - 1.0f };
            DrawRectangleRec(r, info->color);

            // Darken partially-mined tiles so damage is visible.
            float maxHp = info->maxHealth;
            if (maxHp > 1 && world[x][y].health < maxHp) {
                float missing = 1.0f - (world[x][y].health / maxHp); // 0..1
                DrawRectangleRec(r, (Color){0,0,0,(unsigned char)(missing*160)});
            }
        }
    }
}

// ─── Save / Load ──────────────────────────────────────────
// C CONCEPT — binary file I/O: fwrite copies raw bytes from memory
// to disk; fread copies them back. Because `world` is one solid
// array, ONE call saves the whole thing. (Caveat: such files only
// reliably reload on the same machine/compiler — fine for a game
// save, wrong for a portable file format.)
static void WorldSave(void) {
    FILE *f = fopen(SAVE_FILE, "wb");     // "wb" = write binary
    if (f == NULL) return;                // disk error? just skip
    fwrite(world, sizeof(world), 1, f);
    fclose(f);                            // ALWAYS close what you open
}

static bool WorldLoad(void) {
    FILE *f = fopen(SAVE_FILE, "rb");
    if (f == NULL) return false;          // no save file yet
    fread(world, sizeof(world), 1, f);
    fclose(f);
    return true;
}

#endif // WORLD_H
