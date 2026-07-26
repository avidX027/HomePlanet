#ifndef CONFIG_H
#define CONFIG_H
// ============================================================
//  CONFIG.H — Every number you might want to tweak, in ONE place.
//
//  THE RULE OF THIS FILE: changing any value here changes exactly
//  one visible thing in the game and can never break the code.
//  This is your "instant feedback" playground.
//
//  C CONCEPT — #define:
//  These are *macros*. Before compiling, the preprocessor literally
//  copy-pastes the value wherever the name appears. They aren't
//  variables — they use no memory and can't be changed at runtime.
// ============================================================

#include "raylib.h"   // needed for the Color type below

// ─── Window ───────────────────────────────────────────────
#define WINDOW_WIDTH   1280
#define WINDOW_HEIGHT  720
#define WINDOW_TITLE   "HOME PLANET: VOID RUNNER"
#define TARGET_FPS     60

// ─── World ────────────────────────────────────────────────
#define WORLD_SIZE     640         // world is WORLD_SIZE x WORLD_SIZE tiles
#define TILE_SIZE      24          // pixels per tile (between the old chunky
                                   // 30 and Factorio's fine grain)
#define TREE_CHANCE    2           // % of MEADOW tiles that start as trees
#define ROCK_CHANCE    1           // % of MEADOW tiles that start as rocks
#define SPAWN_CLEAR_RADIUS 9       // obstacle-free tiles around the player spawn
// Enemy territory: a few BIG infestation patches far from spawn
// (like biter bases), not an edge ring.
#define INFESTATION_PATCHES 10     // how many enemy patches to scatter
#define INFESTATION_RADIUS  14     // patch radius in tiles
#define FOW_REVEAL_TILES    26     // fog-of-war reveal radius around you
#define DAY_LENGTH_SECONDS  480.0f // one in-game day; shadows swing around it
// Ore fields, Factorio-style: colored ground patches; drills placed
// ON them pump ore forever, and solid ore NODES cluster on/near them.
#define ORE_SULFUR_PATCHES  16     // sulfur fields scattered on the map
#define ORE_METAL_PATCHES   18     // metal fields
#define ORE_COAL_PATCHES    20     // coal fields — the fuel economy needs them
#define ORE_PATCH_RADIUS_MIN 4     // patches vary in size (tiles)
#define ORE_PATCH_RADIUS_MAX 9
#define ORE_NODE_CHANCE      7     // % per patch tile to grow a solid node

// ─── Inventory & logistics ────────────────────────────────
#define STACK_MAX           100    // items per slot, player and machines
#define BELT_CAPACITY       4      // items one belt tile can carry at once
#define BELT_CARRY_SPEED    95.0f  // px/s a belt drags anything standing on it
#define COAL_FUEL_SECONDS   30.0f  // run-time one coal buys a machine
#define MACHINE_FUEL_MAX    5      // coal a machine can hold at once

// ─── Player ─────────────────────────────────────────────────
#define PLAYER_SPEED   330.0f      // pixels per second (~14 tiles/s)
#define PLAYER_RADIUS  7.0f        // size of the player circle
#define PLAYER_REACH   70.0f       // max distance (px) you can mine/place
#define SLINGSHOT_REACH 280.0f      // slingshot shoots FAR now
#define PISTOL_REACH        320.0f      // gun range in pixels
#define SLINGSHOT_SPEED     800.0f      // projectile speed
#define PISTOL_BULLET_SPEED 1500.0f     // pistol bullet speed
#define SLINGSHOT_BASE_DAMAGE 4.0f      // max damage from a stone projectile
#define PISTOL_BULLET_DAMAGE 8.0f       // max damage from a pistol bullet
#define HAND_DPS            1.0f        // mining damage/second with empty hands
#define PLAYER_MAX_HP       100.0f      // you can die now — build a base!
#define SMG_FIRE_INTERVAL   0.12f       // seconds between SMG shots (hold LMB)
#define SHOTGUN_PELLETS     6           // pellets per shotgun blast
// Every weapon auto-fires while you HOLD the button, but a touch
// slower than clicking — click-spam stays the skill ceiling.
#define WEAPON_HOLD_PENALTY 1.25f
#define UNARMED_SPEED_BONUS 1.07f       // hands free → run a bit faster

// ─── Mobs & waves (Factorio-style escalation) ─────────────
#define MOB_BASE_HP         11.0f       // mob health at evolution 0 (scales up)
#define MOB_SPEED           95.0f       // mob walk speed, px/s
#define MOB_CONTACT_DPS     12.0f       // damage/second touching the player
#define MOB_CHEW_DPS        6.0f        // damage/second gnawing walls/machines
#define EVOLUTION_MINUTES   25.0f       // minutes of play until max evolution
#define WAVE_SIZE           6.0f        // brood size that triggers a march —
                                        // waves are EMERGENT (mobs bunch up at
                                        // their nest, then set off), not a
                                        // scheduled game event
#define SPAWN_INTERVAL_START 14.0f      // spawner cooldown at evolution 0
#define SPAWN_INTERVAL_END    5.0f      // ...and at evolution 1

// ─── Machines ─────────────────────────────────────────────
#define TURRET_RANGE        260.0f
#define TURRET_COOLDOWN     0.5f        // seconds between turret shots
#define LASER_RANGE         300.0f
#define LASER_COOLDOWN      0.9f
#define LASER_DAMAGE        16.0f
#define DRILL_INTERVAL      2.2f        // seconds per drill "bite"
#define DRILL_BITE          4.0f        // damage per bite to the adjacent rock
#define CONVEYOR_INTERVAL   0.35f       // seconds for an item to hop one belt
#define INSERTER_INTERVAL   0.9f        // seconds per robotic-hand swing
#define BOT_SPEED           140.0f      // mining bot flight speed
#define BOT_DPS             6.0f        // mining bot dig speed
#define BOMB_FUSE           2.5f
#define BOMB_RADIUS         60.0f       // explosion radius in px (2.5 tiles)

// C CONCEPT — compound literals: (Color){r,g,b,a} builds a Color
// struct value in place. 4 numbers, each 0-255. a=alpha (opacity).
#define PLAYER_COLOR    (Color){  60, 120, 255, 255 }   // body
#define PLAYER_OUTLINE  (Color){   0, 255, 255, 255 }   // ring around body

// ─── Files ────────────────────────────────────────────────
#define SAVE_FILE      "homeplanet.sav"

// ─── Live tuning (the F3 debug console) ───────────────────
// The #defines above are the DEFAULTS. The game actually READS the
// values from this struct, so the debug console can drag them
// around with sliders while the game runs. (A #define is baked into
// the machine code at compile time and can never change again; a
// struct field is ordinary memory, so it can.)
typedef struct {
    float playerSpeed;
    float playerReach;
    float handDPS;
    float slingshotReach;
    float slingshotSpeed;
    float slingshotDamage;
    float pistolReach;
    float pistolBulletSpeed;
    float pistolBulletDamage;
    // mobs & waves
    float mobHp;
    float mobSpeed;
    float mobContactDPS;
    float mobChewDPS;
    float evolutionMinutes;
    float waveSize;
    float spawnIntervalStart;
    float spawnIntervalEnd;
    // machines & bots
    float turretRange;
    float turretCooldown;
    float laserRange;
    float laserCooldown;
    float laserDamage;
    float drillInterval;
    float conveyorInterval;
    float inserterInterval;
    float botSpeed;
    float botDPS;
} Tuning;

// C CONCEPT — designated initializers (.field = value) fill the
// struct by NAME, so reordering the struct can't silently put a
// speed into a damage slot.
static Tuning TUNE = {
    .playerSpeed        = PLAYER_SPEED,
    .playerReach        = PLAYER_REACH,
    .handDPS            = HAND_DPS,
    .slingshotReach     = SLINGSHOT_REACH,
    .slingshotSpeed     = SLINGSHOT_SPEED,
    .slingshotDamage    = SLINGSHOT_BASE_DAMAGE,
    .pistolReach        = PISTOL_REACH,
    .pistolBulletSpeed  = PISTOL_BULLET_SPEED,
    .pistolBulletDamage = PISTOL_BULLET_DAMAGE,
    .mobHp              = MOB_BASE_HP,
    .mobSpeed           = MOB_SPEED,
    .mobContactDPS      = MOB_CONTACT_DPS,
    .mobChewDPS         = MOB_CHEW_DPS,
    .evolutionMinutes   = EVOLUTION_MINUTES,
    .waveSize           = WAVE_SIZE,
    .spawnIntervalStart = SPAWN_INTERVAL_START,
    .spawnIntervalEnd   = SPAWN_INTERVAL_END,
    .turretRange        = TURRET_RANGE,
    .turretCooldown     = TURRET_COOLDOWN,
    .laserRange         = LASER_RANGE,
    .laserCooldown      = LASER_COOLDOWN,
    .laserDamage        = LASER_DAMAGE,
    .drillInterval      = DRILL_INTERVAL,
    .conveyorInterval   = CONVEYOR_INTERVAL,
    .inserterInterval   = INSERTER_INTERVAL,
    .botSpeed           = BOT_SPEED,
    .botDPS             = BOT_DPS,
};

#endif // CONFIG_H
