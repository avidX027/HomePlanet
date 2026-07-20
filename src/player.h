#ifndef PLAYER_H
#define PLAYER_H
// ============================================================
//  PLAYER.H — the player: position, inventory, crafting.
//
//  Owns: the Player struct. Knows NOTHING about tiles or UI.
//
//  DESIGN CHOICE — the simplest inventory that works:
//  `int inventory[ITEM_COUNT]` — one counter per item type.
//  inventory[ITEM_WOOD] == 37 means "you have 37 wood".
//  No slots, no stacks, no searching. When you later want
//  Minecraft-style slots, THIS is the file you'd grow.
// ============================================================

#include "raylib.h"
#include "raymath.h"    // Vector2Add, Vector2Normalize, ...
#include "config.h"
#include "gamedata.h"

typedef struct {
    Vector2 pos;                       // world-space position, in pixels
    int     inventory[ITEM_COUNT];     // count of each item owned
    ItemID  selected;                  // which item the hotbar has active
    bool    craftMenuOpen;
    int     craftSel;                  // highlighted row in craft menu
} Player;

// ─── Init ─────────────────────────────────────────────────
static void PlayerInit(Player *p) {
    // C CONCEPT — pointers: `Player *p` receives the ADDRESS of a
    // Player, so p->pos edits the caller's struct, not a copy.
    // (p->pos is shorthand for (*p).pos.)
    p->pos = (Vector2){ WORLD_SIZE * TILE_SIZE / 2.0f,
                        WORLD_SIZE * TILE_SIZE / 2.0f };   // world center
    for (int i = 0; i < ITEM_COUNT; i++) p->inventory[i] = 0;
    p->selected      = ITEM_NONE;      // empty hands
    p->craftMenuOpen = false;
    p->craftSel      = 0;
}

// ─── Movement (WASD), clamped to world edges ──────────────
static void PlayerMove(Player *p, float dt) {
    Vector2 dir = { 0, 0 };
    if (IsKeyDown(KEY_W)) dir.y -= 1;
    if (IsKeyDown(KEY_S)) dir.y += 1;
    if (IsKeyDown(KEY_A)) dir.x -= 1;
    if (IsKeyDown(KEY_D)) dir.x += 1;

    // Normalize so diagonal movement isn't faster (length 1.41 -> 1).
    // Multiply by dt so speed is per-SECOND, independent of framerate.
    if (dir.x != 0 || dir.y != 0) {
        dir    = Vector2Normalize(dir);
        p->pos = Vector2Add(p->pos, Vector2Scale(dir, PLAYER_SPEED * dt));
    }

    float max = (WORLD_SIZE * TILE_SIZE) - PLAYER_RADIUS;
    if (p->pos.x < PLAYER_RADIUS) p->pos.x = PLAYER_RADIUS;
    if (p->pos.y < PLAYER_RADIUS) p->pos.y = PLAYER_RADIUS;
    if (p->pos.x > max)           p->pos.x = max;
    if (p->pos.y > max)           p->pos.y = max;
}

// ─── Draw ─────────────────────────────────────────────────
// The player's look lives HERE; its colors live in config.h.
static void PlayerDraw(const Player *p) {
    DrawCircleV(p->pos, PLAYER_RADIUS, PLAYER_COLOR);
    DrawCircleLinesV(p->pos, PLAYER_RADIUS + 2, PLAYER_OUTLINE);
}

// ─── Mining power of whatever is selected ─────────────────
static float PlayerMiningDPS(const Player *p) {
    ItemID s = p->selected;
    // A tool only counts if you actually own one.
    if (ITEMS[s].miningDPS > 0 && p->inventory[s] > 0) return ITEMS[s].miningDPS;
    return HAND_DPS;
}

// ─── Crafting ─────────────────────────────────────────────
// All recipe data comes from the ITEMS table — these functions
// would not change if you added 50 new items.
static bool PlayerCanCraft(const Player *p, ItemID id) {
    const ItemInfo *it = &ITEMS[id];
    if (it->inA == ITEM_NONE) return false;                    // no recipe
    if (p->inventory[it->inA] < it->nA) return false;          // missing A
    if (it->inB != ITEM_NONE && p->inventory[it->inB] < it->nB)
        return false;                                          // missing B
    return true;
}

static bool PlayerCraft(Player *p, ItemID id) {
    if (!PlayerCanCraft(p, id)) return false;
    const ItemInfo *it = &ITEMS[id];
    p->inventory[it->inA] -= it->nA;
    if (it->inB != ITEM_NONE) p->inventory[it->inB] -= it->nB;
    p->inventory[id] += it->yield;
    return true;
}

#endif // PLAYER_H
