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
    TILE_SULFUR_NODE,   // solid ore chunk; clusters on sulfur fields
    TILE_METAL_NODE,    // solid ore chunk; clusters on metal fields
    TILE_COAL_NODE,     // solid coal chunk; clusters on coal fields
    TILE_WALL,          // player-built stone wall
    TILE_METAL_WALL,    // much tougher wall (DEFENSE tech)
    TILE_DOOR,          // YOU can walk through it; mobs can't
    TILE_CHEST,         // item storage machine
    TILE_DRILL,         // auto-mines adjacent rocks/trees
    TILE_CONVEYOR,      // moves items along its direction
    TILE_CONVEYOR_CORNER, // same, but drawn as a 90° turn (4 rotations)
    TILE_INSERTER,      // robotic hand: moves items between neighbors
    TILE_TURRET,        // shoots bullets at mobs (needs ammo)
    TILE_LASER_TURRET,  // zaps mobs, no ammo, slower
    TILE_RESEARCH,      // the research computer (opens the tech tree)
    TILE_SPAWNER,       // mob nest — generated at the map edges
    TILE_COUNT
} TileType;

// ─── ITEMS (things in your inventory) ─────────────────────
typedef enum {
    ITEM_NONE = 0,    // means "nothing" — slot 0 is reserved for it
    ITEM_WOOD,
    ITEM_RUBBER,
    ITEM_SMALL_STONE,
    ITEM_STONE,
    ITEM_COAL,
    ITEM_SULFUR_ORE,
    ITEM_SULFUR,
    ITEM_METAL_ORE,
    ITEM_METAL,
    ITEM_FURNACE,
    ITEM_GUNPOWDER,
    ITEM_PISTOL,
    ITEM_BULLET,
    ITEM_PICKAXE,
    ITEM_SLINGSHOT,
    ITEM_WALL,        // placeable block
    ITEM_SMG,             // automatic — hold LMB
    ITEM_SHOTGUN,         // 6-pellet spread per shot
    ITEM_BOMB,            // timed explosive: raids spawners AND bases
    ITEM_DRILL,           // places TILE_DRILL
    ITEM_CHEST,           // places TILE_CHEST
    ITEM_CONVEYOR,        // places TILE_CONVEYOR (R rotates)
    ITEM_CONVEYOR_CORNER, // places TILE_CONVEYOR_CORNER (R rotates)
    ITEM_INSERTER,        // places TILE_INSERTER (R rotates)
    ITEM_MINING_BOT,      // deploys an autonomous mining drone
    ITEM_TURRET,          // places TILE_TURRET
    ITEM_LASER_TURRET,    // places TILE_LASER_TURRET
    ITEM_RESEARCH_COMPUTER, // places TILE_RESEARCH
    ITEM_METAL_WALL,      // places TILE_METAL_WALL
    ITEM_DOOR,            // places TILE_DOOR
    ITEM_GEM,
    ITEM_COUNT
} ItemID;

// ─── TECH TREE ────────────────────────────────────────────
// Researched at a placed Research Computer (RMB it). Each tech has
// ONE prerequisite and an item cost, and items reference the tech
// that unlocks them (tech == TECH_NONE means "always available").
typedef enum {
    TECH_NONE = 0,
    TECH_AUTOMATION,      // drill, chest
    TECH_LOGISTICS,       // conveyor, inserter
    TECH_ROBOTICS,        // mining bot
    TECH_BALLISTICS,      // pistol, bullets
    TECH_DEFENSE,         // gun turret, metal wall
    TECH_ADV_WEAPONS,     // SMG, shotgun
    TECH_EXPLOSIVES,      // bomb
    TECH_LASERS,          // laser turret
    TECH_COUNT
} TechID;

typedef struct {
    const char *name;
    const char *desc;      // one-line "why you want this"
    TechID      requires;  // TECH_NONE = a root of the tree
    ItemID costA; int nA;  // research cost, paid at the computer
    ItemID costB; int nB;
} TechInfo;

static const TechInfo TECHS[TECH_COUNT] = {
    [TECH_NONE]        = { "-", "-", TECH_NONE, ITEM_NONE, 0, ITEM_NONE, 0 },
    [TECH_AUTOMATION]  = { "Automation",       "Mining drills and storage chests",   TECH_NONE,       ITEM_STONE,     15, ITEM_METAL,      8 },
    [TECH_LOGISTICS]   = { "Logistics",        "Conveyor belts and inserter hands",  TECH_AUTOMATION, ITEM_METAL,     12, ITEM_RUBBER,     6 },
    [TECH_ROBOTICS]    = { "Robotics",         "Autonomous mining bots",             TECH_LOGISTICS,  ITEM_METAL,     20, ITEM_RUBBER,    10 },
    [TECH_BALLISTICS]  = { "Ballistics",       "The pistol and bullets",             TECH_NONE,       ITEM_WOOD,      10, ITEM_METAL,      5 },
    [TECH_DEFENSE]     = { "Base Defense",     "Gun turrets and metal walls",        TECH_BALLISTICS, ITEM_METAL,     12, ITEM_STONE,     20 },
    [TECH_ADV_WEAPONS] = { "Advanced Weapons", "The SMG and the shotgun",            TECH_BALLISTICS, ITEM_METAL,     15, ITEM_GUNPOWDER, 10 },
    [TECH_EXPLOSIVES]  = { "Explosives",       "Bombs: crack nests and walls",       TECH_BALLISTICS, ITEM_GUNPOWDER, 15, ITEM_STONE,     10 },
    [TECH_LASERS]      = { "Laser Weaponry",   "Ammo-free laser turrets",            TECH_DEFENSE,    ITEM_METAL,     25, ITEM_COAL,      15 },
};

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
    TechID tech;           // research needed to craft it (TECH_NONE = none).
                           // Old rows omit this field — C fills missing
                           // trailing initializers with 0, i.e. TECH_NONE.
} ItemInfo;

// NOTE — these tables used to be `const`. The F3 debug console
// edits them live (mining speed, recipes, colors...), so they are
// now ordinary mutable arrays. The values below are the defaults
// the console's RESET button restores.
static ItemInfo ITEMS[ITEM_COUNT] = {
    //                 name       color                    dps  places      placeable  inA        nA  inB        nB  yield
    [ITEM_NONE]    = { "Nothing", (Color){  0,  0,  0,255}, 0,  TILE_GRASS, false,     ITEM_NONE,  0, ITEM_NONE,  0, 0 },
    [ITEM_WOOD]         = { "Wood",        (Color){140, 90, 50,255}, 0,  TILE_GRASS, false,     ITEM_NONE,       0, ITEM_NONE,    0, 0 },
    [ITEM_RUBBER]       = { "Rubber",      (Color){ 30,150, 30,255}, 0,  TILE_GRASS, false,     ITEM_NONE,       0, ITEM_NONE,    0, 0 },
    [ITEM_SMALL_STONE]  = { "Small Stone", (Color){120,120,120,255}, 0,  TILE_GRASS, false,     ITEM_STONE,      1, ITEM_NONE,    0, 4 },
    [ITEM_STONE]        = { "Stone",       (Color){110,110,120,255}, 0,  TILE_GRASS, false,     ITEM_NONE,       0, ITEM_NONE,    0, 0 },
    [ITEM_COAL]         = { "Coal",        (Color){ 60, 60, 60,255}, 0,  TILE_GRASS, false,     ITEM_NONE,       0, ITEM_NONE,    0, 0 },
    [ITEM_SULFUR_ORE]   = { "Sulfur Ore",  (Color){220,200, 30,255}, 0,  TILE_GRASS, false,     ITEM_NONE,       0, ITEM_NONE,    0, 0 },
    [ITEM_SULFUR]       = { "Sulfur",      (Color){200,180, 40,255}, 0,  TILE_GRASS, false,     ITEM_SULFUR_ORE, 1, ITEM_FURNACE, 1, 1 },
    [ITEM_METAL_ORE]    = { "Metal Ore",   (Color){140,140,180,255}, 0,  TILE_GRASS, false,     ITEM_NONE,       0, ITEM_NONE,    0, 0 },
    [ITEM_METAL]        = { "Metal",       (Color){190,190,220,255}, 0,  TILE_GRASS, false,     ITEM_METAL_ORE,  1, ITEM_FURNACE, 1, 1 },
    [ITEM_FURNACE]      = { "Furnace",     (Color){120,120,130,255}, 0,  TILE_GRASS, false,     ITEM_STONE,      6, ITEM_WOOD,    2, 1 },
    [ITEM_GUNPOWDER]    = { "Gunpowder",   (Color){100,100,100,255}, 0,  TILE_GRASS, false,     ITEM_COAL,       1, ITEM_SULFUR,  1, 1 },
    [ITEM_PISTOL]       = { "Pistol",      (Color){130,130,160,255}, 0,  TILE_GRASS, false,     ITEM_WOOD,       1, ITEM_METAL,   4, 1, TECH_BALLISTICS },
    [ITEM_BULLET]       = { "Bullet",      (Color){220,190, 70,255}, 0,  TILE_GRASS, false,     ITEM_METAL,      1, ITEM_GUNPOWDER,1, 4, TECH_BALLISTICS },
    [ITEM_PICKAXE]      = { "Pickaxe",     (Color){200,160,100,255}, 4,  TILE_GRASS, false,     ITEM_WOOD,       5, ITEM_STONE,  10, 1 },
    [ITEM_SLINGSHOT]    = { "Slingshot",   (Color){120, 70, 30,255}, 0,  TILE_GRASS, false,     ITEM_WOOD,       3, ITEM_RUBBER, 1, 1 },
    [ITEM_WALL]         = { "Wall",        (Color){180,180,190,255}, 0,  TILE_WALL,  true,      ITEM_STONE,      2, ITEM_NONE,    0, 4 },
    [ITEM_SMG]          = { "SMG",         (Color){ 95, 95,115,255}, 0,  TILE_GRASS, false,     ITEM_METAL,      6, ITEM_RUBBER,  2, 1, TECH_ADV_WEAPONS },
    [ITEM_SHOTGUN]      = { "Shotgun",     (Color){150,110, 60,255}, 0,  TILE_GRASS, false,     ITEM_WOOD,       4, ITEM_METAL,   6, 1, TECH_ADV_WEAPONS },
    [ITEM_BOMB]         = { "Bomb",        (Color){ 70, 60, 60,255}, 0,  TILE_GRASS, false,     ITEM_GUNPOWDER,  4, ITEM_METAL,   2, 1, TECH_EXPLOSIVES },
    [ITEM_DRILL]        = { "Mining Drill",(Color){185,145, 60,255}, 0,  TILE_DRILL, true,      ITEM_METAL,      8, ITEM_STONE,   4, 1, TECH_AUTOMATION },
    [ITEM_CHEST]        = { "Chest",       (Color){170,130, 60,255}, 0,  TILE_CHEST, true,      ITEM_WOOD,       6, ITEM_METAL,   2, 1, TECH_AUTOMATION },
    [ITEM_CONVEYOR]     = { "Conveyor",    (Color){190,170, 60,255}, 0,  TILE_CONVEYOR, true,   ITEM_METAL,      2, ITEM_RUBBER,  1, 2, TECH_LOGISTICS },
    [ITEM_CONVEYOR_CORNER] = { "Belt Corner", (Color){205,150, 55,255}, 0, TILE_CONVEYOR_CORNER, true, ITEM_METAL, 2, ITEM_RUBBER, 1, 2, TECH_LOGISTICS },
    [ITEM_INSERTER]     = { "Inserter",    (Color){225,160, 50,255}, 0,  TILE_INSERTER, true,   ITEM_METAL,      3, ITEM_RUBBER,  1, 1, TECH_LOGISTICS },
    [ITEM_MINING_BOT]   = { "Mining Bot",  (Color){ 80,200,220,255}, 0,  TILE_GRASS, false,     ITEM_METAL,     12, ITEM_RUBBER,  6, 1, TECH_ROBOTICS },
    [ITEM_TURRET]       = { "Gun Turret",  (Color){150,150,165,255}, 0,  TILE_TURRET, true,     ITEM_METAL,     10, ITEM_STONE,   5, 1, TECH_DEFENSE },
    [ITEM_LASER_TURRET] = { "Laser Turret",(Color){220, 85, 85,255}, 0,  TILE_LASER_TURRET, true, ITEM_METAL,   15, ITEM_COAL,    8, 1, TECH_LASERS },
    [ITEM_RESEARCH_COMPUTER] = { "Research Computer", (Color){ 70,160,220,255}, 0, TILE_RESEARCH, true, ITEM_WOOD, 8, ITEM_METAL, 4, 1 },
    [ITEM_METAL_WALL]   = { "Metal Wall",  (Color){155,155,175,255}, 0,  TILE_METAL_WALL, true, ITEM_METAL,      3, ITEM_NONE,    0, 2, TECH_DEFENSE },
    [ITEM_DOOR]         = { "Door",        (Color){150,100, 55,255}, 0,  TILE_DOOR,  true,      ITEM_WOOD,       4, ITEM_NONE,    0, 1 },
    [ITEM_GEM]          = { "Gem",         (Color){3, 165, 252, 255}, 0, TILE_GRASS, false,    ITEM_NONE, 0, ITEM_NONE, 0, 1}
};

typedef struct {
    const char *name;
    Color       color;     // how the tile is drawn
    float       maxHealth; // hits it takes to break (health -= DPS * time)
    ItemID      drops;     // what breaking it gives you
    int         dropCount;
    bool        breakable; // grass isn't
    bool        walkable;  // can things walk over it? (the DOOR is a
                           // special case: false here blocks MOBS, but
                           // the player's collision test allows doors)
} TileInfo;

static TileInfo TILES[TILE_COUNT] = {
    //                      name             color                    maxHP  drops                     n  breakable walkable
    [TILE_GRASS]        = { "Grass",         (Color){ 40,150, 60,255},  1,   ITEM_NONE,                0, false, true  },
    [TILE_TREE]         = { "Tree",          (Color){ 90, 60, 30,255},  4,   ITEM_WOOD,               10, true,  false },
    [TILE_ROCK]         = { "Rock",          (Color){ 80, 80, 90,255},  8,   ITEM_STONE,              10, true,  false },
    [TILE_SULFUR_NODE]  = { "Sulfur Node",   (Color){198,178, 44,255}, 8,   ITEM_SULFUR_ORE,          3, true,  false },
    [TILE_METAL_NODE]   = { "Metal Node",    (Color){146,152,190,255}, 9,   ITEM_METAL_ORE,           3, true,  false },
    [TILE_COAL_NODE]    = { "Coal Node",     (Color){ 52, 50, 56,255}, 7,   ITEM_COAL,                4, true,  false },
    [TILE_WALL]         = { "Wall",          (Color){170,170,180,255}, 28,   ITEM_STONE,               1, true,  false },
    [TILE_METAL_WALL]   = { "Metal Wall",    (Color){150,150,170,255}, 45,   ITEM_METAL,               1, true,  false },
    [TILE_DOOR]         = { "Door",          (Color){150,100, 55,255}, 25,   ITEM_DOOR,                1, true,  false },
    [TILE_CHEST]        = { "Chest",         (Color){120, 90, 45,255}, 16,   ITEM_CHEST,               1, true,  false },
    [TILE_DRILL]        = { "Mining Drill",  (Color){110, 95, 55,255}, 16,   ITEM_DRILL,               1, true,  false },
    // Belts are deliberately flimsy — a tap picks one back up, so
    // re-routing a line is fast instead of a chore.
    [TILE_CONVEYOR]     = { "Conveyor",      (Color){ 70, 70, 60,255},  1,   ITEM_CONVEYOR,            1, true,  true  },
    [TILE_CONVEYOR_CORNER] = { "Belt Corner",(Color){ 74, 68, 58,255},  1,   ITEM_CONVEYOR_CORNER,     1, true,  true  },
    [TILE_INSERTER]     = { "Inserter",      (Color){ 95, 75, 40,255},  6,   ITEM_INSERTER,            1, true,  false },
    [TILE_TURRET]       = { "Gun Turret",    (Color){ 90, 90,100,255}, 18,   ITEM_TURRET,              1, true,  false },
    [TILE_LASER_TURRET] = { "Laser Turret",  (Color){110, 70, 70,255}, 18,   ITEM_LASER_TURRET,        1, true,  false },
    [TILE_RESEARCH]     = { "Research Comp.",(Color){ 45, 75,110,255}, 16,   ITEM_RESEARCH_COMPUTER,   1, true,  false },
    [TILE_SPAWNER]      = { "Mob Spawner",   (Color){ 90, 40,110,255}, 40,   ITEM_GEM,              5, true,  false },
};

// ─── Tile break loot ──────────────────────────────────────
// Roll the random drops for breaking `before` into out[ITEM_COUNT].
// Lives HERE (not main.c) because players, drills, AND mining bots
// all break tiles and must share one loot policy.
static void RollTileBreakDrops(TileType before, int out[ITEM_COUNT]) {
    for (int i = 0; i < ITEM_COUNT; i++) out[i] = 0;
    if (before == TILE_TREE) {
        out[TILES[before].drops] += TILES[before].dropCount;
        if (GetRandomValue(1, 100) <= 20) out[ITEM_RUBBER] += 1;   // bonus rubber
        return;
    }
    if (before == TILE_ROCK) {
        // Rocks always give stone, then roll independent ore chances.
        // Coal is common on purpose now — every drill and inserter
        // burns it, so the fuel economy has to keep up.
        out[ITEM_STONE] += TILES[before].dropCount;
        out[ITEM_COAL]  += GetRandomValue(1, 3);
        if (GetRandomValue(1, 100) <= 45) out[ITEM_SULFUR_ORE] += 1;
        if (GetRandomValue(1, 100) <= 35) out[ITEM_METAL_ORE] += 1;
        return;
    }
    if (before == TILE_SULFUR_NODE || before == TILE_METAL_NODE ||
        before == TILE_COAL_NODE) {
        // Ore nodes are the rich strike: a fat pile of their ore
        // plus a little stone shell.
        out[TILES[before].drops] += TILES[before].dropCount + GetRandomValue(0, 3);
        out[ITEM_STONE] += GetRandomValue(1, 2);
        return;
    }
    // Anything else: exactly what the table says.
    if (TILES[before].drops != ITEM_NONE) {
        out[TILES[before].drops] += TILES[before].dropCount;
    }
}

// Which item places this tile? (Used to draw machine tiles with
// their item's sprite.) Linear scan is fine — called per DRAWN tile.
static ItemID ItemThatPlaces(TileType t) {
    for (ItemID id = 1; id < ITEM_COUNT; id++) {
        if (ITEMS[id].placeable && ITEMS[id].places == t) return id;
    }
    return ITEM_NONE;
}

// ─── Procedural pixel art ─────────────────────────────────
// Every item icon is an 8x8 pixel-art sprite described as TEXT:
// each string is one row, each character one pixel. No image files,
// no artist — the art is data, like everything else in this file.
//
// The characters map to colors DERIVED from the item's table color,
// so recoloring an item (e.g. in the debug console) automatically
// recolors its sprite:
//   '.' transparent      'o' dark outline     'w' white glint
//   'b' base color       'd' dark base        'l' light base
//   's' secondary color  't' dark secondary
// `secondary` is the sprite's second hue (a pickaxe's handle, the
// fire inside a furnace) and lives here because it's ART data, not
// gameplay data.
typedef struct {
    Color       secondary;
    const char *rows[8];
} ItemArt;

static const ItemArt ITEM_ART[ITEM_COUNT] = {
    [ITEM_GEM] = { { 0, 220, 255, 255 }, {    // brilliant gem / diamond
        "..oooo..",
        ".ollllbo.",
        "olbbbbbbd",
        "obllllbbd",
        ".obbbbdd.",
        "..obbdd..",
        "...obd...",
        "....o...." } },
    [ITEM_WOOD] = { { 95, 60, 35, 255 }, {      // stacked planks
        "oooooooo",
        "obblbbdo",
        "oooooooo",
        "odbbblbo",
        "oooooooo",
        "oblbbbdo",
        "oooooooo",
        "........" } },
    [ITEM_RUBBER] = { { 20, 100, 25, 255 }, {   // sap droplet
        "...oo...",
        "..obbo..",
        ".oblbbo.",
        ".obbbbo.",
        "oblbbbdo",
        "obbbbddo",
        ".obbddo.",
        "..oooo.." } },
    [ITEM_SMALL_STONE] = { { 80, 80, 90, 255 }, {  // two pebbles
        "........",
        ".ooo....",
        "oblbo...",
        "obddo...",
        ".ooo.oo.",
        "....oblo",
        "....obdo",
        ".....oo." } },
    [ITEM_STONE] = { { 80, 80, 90, 255 }, {     // boulder
        "..oooo..",
        ".oblllo.",
        "obbbblbo",
        "obdbbbbo",
        "obbbdbbo",
        "obdbbdbo",
        ".obddbo.",
        "..oooo.." } },
    [ITEM_COAL] = { { 30, 30, 35, 255 }, {      // dark chunk, glint
        "..oooo..",
        ".odbbdo.",
        "odbwbbdo",
        "obbbbddo",
        "odbdbbdo",
        "obbdbddo",
        ".oddbdo.",
        "..oooo.." } },
    [ITEM_SULFUR_ORE] = { { 110, 110, 120, 255 }, {  // gray rock, yellow veins
        "..oooo..",
        ".osssso.",
        "osbsstso",
        "ostsbsso",
        "osbstsbo",
        "ossbstso",
        ".otssbo.",
        "..oooo.." } },
    [ITEM_SULFUR] = { { 160, 140, 30, 255 }, {  // powder pile
        "........",
        "........",
        "...oo...",
        "..oblo..",
        ".obblbo.",
        "obblbbdo",
        "odbbbddo",
        ".oooooo." } },
    [ITEM_METAL_ORE] = { { 110, 110, 120, 255 }, {  // gray rock, metal veins
        "..oooo..",
        ".osssso.",
        "osbsstso",
        "ostsbsso",
        "osbstsbo",
        "ossbstso",
        ".otssbo.",
        "..oooo.." } },
    [ITEM_METAL] = { { 140, 140, 170, 255 }, {  // ingot
        "........",
        "........",
        ".oooooo.",
        "oblllbdo",
        "obbbbbdo",
        "oddddddo",
        ".oooooo.",
        "........" } },
    [ITEM_FURNACE] = { { 240, 140, 40, 255 }, { // stone box, fire inside
        ".oooooo.",
        "obbbbbdo",
        "obbdbbdo",
        "oboooodo",
        "obostsdo",
        "obosssdo",
        "oboooddo",
        ".oooooo." } },
    [ITEM_GUNPOWDER] = { { 60, 60, 60, 255 }, { // powder pile, spark
        "........",
        "...oo...",
        "..odbo..",
        ".odbwdo.",
        "obdbdbdo",
        "oddbdddo",
        ".oooooo.",
        "........" } },
    [ITEM_PISTOL] = { { 110, 75, 45, 255 }, {   // sidearm, wood grip
        "........",
        "ooooooo.",
        "obbbbbdo",
        "oddoooo.",
        ".osto...",
        ".osto...",
        "..oo....",
        "........" } },
    [ITEM_BULLET] = { { 200, 120, 60, 255 }, {  // cartridge, copper tip
        "........",
        "...oo...",
        "..osso..",
        "..otso..",
        "..obbo..",
        "..oblo..",
        "..oddo..",
        "...oo..." } },
    [ITEM_PICKAXE] = { { 130, 130, 140, 255 }, {  // slanted: head high-left,
        "oso.....",                               // haft running down-right
        "ossto...",
        ".otsto..",
        "..obdo..",
        "...obdo.",
        "....obdo",
        "....obdo",
        ".....oo." } },
    [ITEM_SLINGSHOT] = { { 50, 45, 40, 255 }, { // Y-fork with band
        ".o....o.",
        "obs..sbo",
        ".obddbo.",
        "..obbo..",
        "..obdo..",
        "..obdo..",
        "..obdo..",
        "...oo..." } },
    [ITEM_WALL] = { { 120, 120, 130, 255 }, {   // brick block
        "oooooooo",
        "oblsbbdo",
        "osssssso",
        "oblbbsdo",
        "osssssso",
        "obsbbldo",
        "oooooooo",
        "........" } },
    [ITEM_SMG] = { { 110, 75, 45, 255 }, {      // compact auto, wood grip
        "........",
        "ooooooo.",
        "obbbbbdo",
        "oddoddo.",
        ".ostodo.",
        ".osto...",
        "..oo....",
        "........" } },
    [ITEM_SHOTGUN] = { { 140, 95, 50, 255 }, {  // long barrel, wood stock
        "........",
        "oooooooo",
        "osstbbdo",
        "otsoodo.",
        "..oo....",
        "........",
        "........",
        "........" } },
    [ITEM_BOMB] = { { 255, 180, 60, 255 }, {    // round bomb, lit fuse
        ".....w..",
        "....s...",
        "..oooo..",
        ".odbbdo.",
        ".obbdddo",
        ".odddddo",
        "..oooo..",
        "........" } },
    [ITEM_DRILL] = { { 140, 140, 150, 255 }, {  // squared housing, bit inset
        "oooooooo",
        "obbbbbdo",
        "oblbbbdo",
        "obosstdo",
        "obosstdo",
        "obbbbbdo",
        "obdddddo",
        "oooooooo" } },
    [ITEM_CHEST] = { { 150, 150, 160, 255 }, {  // wood chest, metal band
        "........",
        ".oooooo.",
        "obbbbldo",
        "osssssto",
        "obbwbbdo",
        "obbbbddo",
        ".oooooo.",
        "........" } },
    [ITEM_CONVEYOR] = { { 70, 70, 75, 255 }, {  // belt with chevrons
        "oooooooo",
        "otbtbtbo",
        "obtbtbto",
        "otbtbtbo",
        "obtbtbto",
        "otbtbtbo",
        "oooooooo",
        "........" } },
    [ITEM_CONVEYOR_CORNER] = { { 70, 70, 75, 255 }, {  // belt bending right
        "oooooooo",
        "otbtbtbo",
        "obtbtbbo",
        "otbtbboo",
        "obtbbo..",
        "otbbo...",
        "obbo....",
        "oo......" } },
    [ITEM_INSERTER] = { { 150, 150, 160, 255 }, {  // claw, arm, base
        "...oo...",
        "..otto..",
        "...bb...",
        "...bb...",
        "..obbo..",
        ".obddbo.",
        ".oooooo.",
        "........" } },
    [ITEM_MINING_BOT] = { { 255, 160, 60, 255 }, {  // hover drone, eyes
        "..oooo..",
        ".oblbbo.",
        "obwbbwdo",
        "obbbbbdo",
        ".obddbo.",
        "..o..o..",
        ".s.ss.s.",
        "........" } },
    [ITEM_TURRET] = { { 90, 90, 95, 255 }, {    // boxy bunker, short barrel
        "..osso..",
        "..osso..",
        "oooooooo",
        "obbbbbdo",
        "oblbbbdo",
        "obbbbbdo",
        "obdddddo",
        "oooooooo" } },
    [ITEM_LASER_TURRET] = { { 90, 90, 95, 255 }, {  // boxy, glowing emitter
        "..owwo..",
        "..osso..",
        "oooooooo",
        "obbbbbdo",
        "oblbwbdo",
        "obbbbbdo",
        "obdddddo",
        "oooooooo" } },
    [ITEM_RESEARCH_COMPUTER] = { { 80, 220, 230, 255 }, {  // screen + keys
        ".oooooo.",
        "osssssdo",
        "oswsswdo",
        "osssssdo",
        ".oooooo.",
        "obddbddo",
        ".oooooo.",
        "........" } },
    [ITEM_METAL_WALL] = { { 100, 100, 110, 255 }, {  // riveted plate
        "oooooooo",
        "obbbbbdo",
        "owbbdbwo",
        "obbbbbdo",
        "obdbbddo",
        "owbbdbwo",
        "oddddddo",
        "oooooooo" } },
    [ITEM_DOOR] = { { 90, 60, 35, 255 }, {      // planked door + handle
        ".oooooo.",
        "obbobbdo",
        "obbobbdo",
        "obbobwdo",
        "obbobbdo",
        "obbobbdo",
        ".oooooo.",
        "........" } },
};

// Multiply a color toward black (f < 1 darkens). The int math
// clamps at 255 so f > 1 can brighten without wrapping around.
static Color ItemArtShade(Color c, float f) {
    int r = (int)(c.r * f), g = (int)(c.g * f), b = (int)(c.b * f);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, c.a };
}

// Halfway toward white — a cheap "highlight" tint.
static Color ItemArtLight(Color c) {
    return (Color){ (unsigned char)((c.r + 255) / 2),
                    (unsigned char)((c.g + 255) / 2),
                    (unsigned char)((c.b + 255) / 2), c.a };
}

// One art character → one concrete color, derived LIVE from the
// item's current table color (so color edits restyle the sprite).
static Color ItemArtPixel(ItemID id, char ch) {
    Color base = ITEMS[id].color;
    Color sec  = ITEM_ART[id].secondary;
    switch (ch) {
        case 'o': return (Color){ 25, 22, 30, 255 };
        case 'b': return base;
        case 'd': return ItemArtShade(base, 0.55f);
        case 'l': return ItemArtLight(base);
        case 's': return sec;
        case 't': return ItemArtShade(sec, 0.6f);
        case 'w': return (Color){ 245, 242, 235, 255 };
    }
    return BLANK;   // '.' or anything unknown → transparent
}

// Draw an item's sprite into a size x size square at (x, y).
// Pixel edges are computed as (int) screen positions of each
// boundary — NOT (int)(px) widths — so scaled sprites never show
// seams or overlap between the 64 little rectangles.
static void DrawItemSprite(ItemID id, float x, float y, float size) {
    if (id <= ITEM_NONE || id >= ITEM_COUNT) return;
    const ItemArt *art = &ITEM_ART[id];
    if (art->rows[0] == NULL) {   // an item with no art yet → flat square
        DrawRectangle((int)x, (int)y, (int)size, (int)size, ITEMS[id].color);
        return;
    }
    float px = size / 8.0f;
    for (int row = 0; row < 8; row++) {
        const char *line = art->rows[row];
        for (int col = 0; col < 8 && line[col] != '\0'; col++) {
            Color c = ItemArtPixel(id, line[col]);
            if (c.a == 0) continue;
            int x0 = (int)(x + col * px),      y0 = (int)(y + row * px);
            int x1 = (int)(x + (col + 1) * px), y1 = (int)(y + (row + 1) * px);
            DrawRectangle(x0, y0, x1 - x0, y1 - y0, c);
        }
    }
}

// Rotated version, for items held in the player's hand (a pistol
// that points at the cursor). Each pixel becomes a DrawRectanglePro
// call: `origin` is which LOCAL point of the rectangle gets pinned
// at (rec.x, rec.y) — by giving every pixel the negated offset from
// the sprite's center, all 64 pixels share one anchor and rotate
// together as a rigid sprite.
static void DrawItemSpriteRot(ItemID id, Vector2 center, float size, float rotationDeg) {
    if (id <= ITEM_NONE || id >= ITEM_COUNT) return;
    const ItemArt *art = &ITEM_ART[id];
    float px = size / 8.0f;
    if (art->rows[0] == NULL) {
        DrawRectanglePro((Rectangle){ center.x, center.y, size, size },
                         (Vector2){ size / 2, size / 2 }, rotationDeg, ITEMS[id].color);
        return;
    }
    for (int row = 0; row < 8; row++) {
        const char *line = art->rows[row];
        for (int col = 0; col < 8 && line[col] != '\0'; col++) {
            Color c = ItemArtPixel(id, line[col]);
            if (c.a == 0) continue;
            Vector2 off = { (col - 4) * px, (row - 4) * px };
            // px + 0.5f: a hair of overlap hides seams between
            // rotated pixels (rotation defeats the integer-edge
            // trick the axis-aligned version uses).
            DrawRectanglePro((Rectangle){ center.x, center.y, px + 0.5f, px + 0.5f },
                             (Vector2){ -off.x, -off.y }, rotationDeg, c);
        }
    }
}

// ─── Weapons ──────────────────────────────────────────────
// Rank doubles as "is this a weapon": 0 means no. Higher = better,
// which is what the Q quick-draw key uses to pick your best gun.
static int ItemWeaponRank(ItemID id) {
    switch (id) {
        case ITEM_SLINGSHOT: return 1;
        case ITEM_PISTOL:    return 2;
        case ITEM_SHOTGUN:   return 3;
        case ITEM_SMG:       return 4;
        default:             return 0;
    }
}

static bool ItemIsWeapon(ItemID id) { return ItemWeaponRank(id) > 0; }

// ─── Magazines & reloading ────────────────────────────────
// A weapon fires from a MAGAZINE. Empty it and the gun reloads
// itself from your bullet stack, which takes time — that's the
// window the mobs are counting on. 0 = no magazine (the slingshot
// feeds straight from the stone pile).
static int ItemMagSize(ItemID id) {
    switch (id) {
        case ITEM_PISTOL:  return 12;
        case ITEM_SMG:     return 30;
        case ITEM_SHOTGUN: return 6;
        default:           return 0;
    }
}

static float ItemReloadTime(ItemID id) {
    switch (id) {
        case ITEM_PISTOL:  return 1.1f;
        case ITEM_SMG:     return 1.9f;
        case ITEM_SHOTGUN: return 2.3f;
        default:           return 0.0f;
    }
}

// Which item does this weapon feed on?
static ItemID ItemAmmoFor(ItemID id) {
    if (id == ITEM_SLINGSHOT) return ITEM_SMALL_STONE;
    if (ItemMagSize(id) > 0)  return ITEM_BULLET;
    return ITEM_NONE;
}

// ─── Crafting time ────────────────────────────────────────
// Nothing is instant any more: every recipe takes real seconds,
// derived from how much it consumes so the table stays the single
// source of truth (a pricier recipe is automatically a slower one).
// A few landmark builds get an explicit, longer time.
static float ItemCraftTime(ItemID id) {
    if (id <= ITEM_NONE || id >= ITEM_COUNT) return 0.5f;
    switch (id) {
        case ITEM_PISTOL:       return 3.0f;
        case ITEM_SMG:          return 4.5f;
        case ITEM_SHOTGUN:      return 4.0f;
        case ITEM_MINING_BOT:   return 6.0f;
        case ITEM_LASER_TURRET: return 6.0f;
        case ITEM_TURRET:       return 4.0f;
        case ITEM_RESEARCH_COMPUTER: return 5.0f;
        case ITEM_DRILL:        return 3.5f;
        case ITEM_BOMB:         return 2.0f;
        default: break;
    }
    const ItemInfo *it = &ITEMS[id];
    float t = 0.35f + 0.05f * (float)(it->nA + it->nB);
    if (t > 4.0f) t = 4.0f;
    return t;
}

// ─── Tile family helpers ──────────────────────────────────
// Both belt kinds behave identically in the logistics code; only
// their drawing differs. One predicate keeps that honest.
static bool TileIsBelt(TileType t) {
    return t == TILE_CONVEYOR || t == TILE_CONVEYOR_CORNER;
}

// Machines that hold state, burn fuel, or store items — i.e. the
// ones that get a Machine record and a UI.
static bool TileIsMachine(TileType t) {
    return t == TILE_CHEST || t == TILE_DRILL || TileIsBelt(t) ||
           t == TILE_INSERTER || t == TILE_TURRET || t == TILE_LASER_TURRET ||
           t == TILE_RESEARCH;
}

// Machines whose facing matters — R rotates these in place.
// Drills are directional too: they spit their output onto whatever
// sits in FRONT of them, so a drill can feed a belt with no arm.
static bool TileIsDirectional(TileType t) {
    return TileIsBelt(t) || t == TILE_INSERTER || t == TILE_DRILL;
}

// Machines that run on coal.
static bool TileNeedsFuel(TileType t) {
    return t == TILE_DRILL || t == TILE_INSERTER;
}

// Seconds between shots when you CLICK. Holding the button fires
// slightly slower (see WEAPON_HOLD_PENALTY in config.h).
static float ItemFireInterval(ItemID id) {
    switch (id) {
        case ITEM_SMG:       return SMG_FIRE_INTERVAL;
        case ITEM_PISTOL:    return 0.22f;
        case ITEM_SHOTGUN:   return 0.65f;
        case ITEM_SLINGSHOT: return 0.40f;
        default:             return 0.25f;
    }
}

// ─── Queries over the ITEMS table ─────────────────────────
// These live here (not in main.c or ui.h) because they depend on
// nothing but the table above — and both main.c and ui.h need them.
// An item is "craftable" if its recipe has at least one input (inA).
// Starting at id = 1 skips ITEM_NONE (id 0), the "empty" sentinel.

// How many recipes exist (craft menu row count).
static int CraftableCount(void) {
    int n = 0;
    for (ItemID id = 1; id < ITEM_COUNT; id++)
        if (ITEMS[id].inA != ITEM_NONE) n++;
    return n;
}

// Map a craft-menu row number back to an ItemID. The menu shows
// recipes as rows 0,1,2,... but ITEMS[] has gaps (not every item is
// craftable), so we walk the table counting only craftable items.
static ItemID CraftableAtRow(int row) {
    int n = 0;
    for (ItemID id = 1; id < ITEM_COUNT; id++) {
        if (ITEMS[id].inA == ITEM_NONE) continue;  // skip non-craftables
        if (n == row) return id;
        n++;
    }
    return ITEM_NONE;  // row out of range → "nothing"
}

#endif // GAMEDATA_H
