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
#define WORLD_SIZE     64          // world is WORLD_SIZE x WORLD_SIZE tiles
#define TILE_SIZE      60          // pixels per tile
#define TREE_CHANCE    8           // % of tiles that start as trees
#define ROCK_CHANCE    4           // % of tiles that start as rocks

// ─── Player ───────────────────────────────────────────────
#define PLAYER_SPEED   450.0f      // pixels per second
#define PLAYER_RADIUS  20.0f       // size of the player circle
#define PLAYER_REACH   150.0f      // max distance (px) you can mine/place
#define HAND_DPS       1.0f        // mining damage/second with empty hands

// C CONCEPT — compound literals: (Color){r,g,b,a} builds a Color
// struct value in place. 4 numbers, each 0-255. a=alpha (opacity).
#define PLAYER_COLOR    (Color){  60, 120, 255, 255 }   // body
#define PLAYER_OUTLINE  (Color){   0, 255, 255, 255 }   // ring around body

// ─── Files ────────────────────────────────────────────────
#define SAVE_FILE      "homeplanet.sav"

#endif // CONFIG_H
