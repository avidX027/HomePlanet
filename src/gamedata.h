#ifndef GAMEDATA_H
#define GAMEDATA_H
// ============================================================
//  GAMEDATA.H — WHAT exists in the game: items and tiles.
//
//  THE BIG IDEA (data-driven design):
//  Every item is fully described by ONE ROW in ONE TABLE below.
//  To remove an item: delete its enum line + its table row. Done.
//  Nothing else in the codebase names specific items — all other
//  code just reads these tables. Same for tiles.
//
//  Try it: add ITEM_GOLD with a color and a recipe, recompile,
//  and it appears in the hotbar and craft menu automatically.
//
//  C CONCEPT — enum:
//  An enum invents named integer constants. ITEM_NONE is 0,
//  ITEM_WOOD is 1, and so on. The compiler counts for us.
//  The last entry ITEM_COUNT equals "how many items exist",
//  which lets us size arrays and write loops over all items.
// ============================================================

#include "raylib.h"

// ─── TILES (the world grid) ───────────────────────────────
typedef enum {
    TILE_GRASS = 0,   // walkable ground; what broken tiles turn into
    TILE_TREE,
    TILE_ROCK,
    TILE_WALL,        // player-built
    TILE_COUNT
} TileType;

// ─── ITEMS (things in your inventory) ─────────────────────
typedef enum {
    ITEM_NONE = 0,    // means "nothing" — slot 0 is reserved for it
    ITEM_WOOD,
    ITEM_STONE,
    ITEM_PICKAXE,
    ITEM_WALL,        // placeable block
    ITEM_COUNT
} ItemID;

// ============================================================
//  C CONCEPT — struct: a bundle of related fields under one name.
//  C CONCEPT — designated initializers: [ITEM_WOOD] = {...} fills
//  a specific array slot by index, so row order can't silently
//  drift out of sync with the enum. This is the glue that makes
//  "one row = one item" safe.
// ============================================================

typedef struct {
    const char *name;      // shown in UI
    Color       color;     // icon color in UI
    float       miningDPS; // >0 means it's a tool: damage/sec to tiles
    TileType    places;    // which tile RMB places (checked with placeable)
    bool        placeable; // can this item be placed as a tile?
    // Recipe to CRAFT this item (inA == ITEM_NONE means "not craftable"):
    ItemID inA;  int nA;   // needs nA of inA
    ItemID inB;  int nB;   // and nB of inB (ITEM_NONE if only one input)
    int    yield;          // how many you get per craft
} ItemInfo;

static const ItemInfo ITEMS[ITEM_COUNT] = {
    //                 name       color                    dps  places      placeable  inA        nA  inB        nB  yield
    [ITEM_NONE]    = { "Nothing", (Color){  0,  0,  0,255}, 0,  TILE_GRASS, false,     ITEM_NONE,  0, ITEM_NONE,  0, 0 },
    [ITEM_WOOD]    = { "Wood",    (Color){140, 90, 50,255}, 0,  TILE_GRASS, false,     ITEM_NONE,  0, ITEM_NONE,  0, 0 },
    [ITEM_STONE]   = { "Stone",   (Color){110,110,120,255}, 0,  TILE_GRASS, false,     ITEM_NONE,  0, ITEM_NONE,  0, 0 },
    [ITEM_PICKAXE] = { "Pickaxe", (Color){200,160,100,255}, 4,  TILE_GRASS, false,     ITEM_WOOD,  5, ITEM_STONE,10, 1 },
    [ITEM_WALL]    = { "Wall",    (Color){180,180,190,255}, 0,  TILE_WALL,  true,      ITEM_STONE, 2, ITEM_NONE,  0, 4 },
};

typedef struct {
    const char *name;
    Color       color;     // how the tile is drawn
    float       maxHealth; // hits it takes to break (health -= DPS * time)
    ItemID      drops;     // what breaking it gives you
    int         dropCount;
    bool        breakable; // grass isn't
} TileInfo;

static const TileInfo TILES[TILE_COUNT] = {
    //               name     color                    maxHP  drops       n  breakable
    [TILE_GRASS] = { "Grass", (Color){ 40,150, 60,255},  1,   ITEM_NONE,  0, false },
    [TILE_TREE]  = { "Tree",  (Color){ 90, 60, 30,255},  4,   ITEM_WOOD, 10, true  },
    [TILE_ROCK]  = { "Rock",  (Color){ 80, 80, 90,255},  8,   ITEM_STONE,10, true  },
    [TILE_WALL]  = { "Wall",  (Color){170,170,180,255}, 10,   ITEM_STONE, 1, true  },
};

#endif // GAMEDATA_H
