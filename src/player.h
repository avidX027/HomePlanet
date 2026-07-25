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
#include "world.h"

#define HOTBAR_MAX_SLOTS 7
#define INVENTORY_COLS 10
#define INVENTORY_ROWS 10
#define INVENTORY_SIZE (INVENTORY_COLS * INVENTORY_ROWS)

typedef struct {
    Vector2 pos;                       // world-space position, in pixels
    int     inventory[ITEM_COUNT];     // count of each item owned
    ItemID  selected;                  // which item the hotbar has active
    ItemID  hotbar[HOTBAR_MAX_SLOTS]; // item ids in the visible slots
    int     slotCount;                 // how many slots are currently in use
    int     selectedSlot;              // which slot is currently active
    ItemID  inventorySlots[INVENTORY_SIZE];
    int     inventoryAmounts[INVENTORY_SIZE];
    int     inventorySlotCount;
    bool    inventoryOpen;
    int     inventoryCursor;           // currently hovered/selected grid slot
    bool    inventoryDragging;
    int     inventoryDragIndex;
    bool    craftMenuOpen;
    int     craftSel;                  // highlighted row in craft menu
    int     craftScroll;               // first visible craft row
} Player;

// ─── Init ─────────────────────────────────────────────────
static void PlayerInit(Player *p) {
    // C CONCEPT — pointers: `Player *p` receives the ADDRESS of a
    // Player, so p->pos edits the caller's struct, not a copy.
    // (p->pos is shorthand for (*p).pos.)
    p->pos = (Vector2){ WORLD_SIZE * TILE_SIZE / 2.0f,
                        WORLD_SIZE * TILE_SIZE / 2.0f };   // world center
    for (int i = 0; i < ITEM_COUNT; i++) p->inventory[i] = 0;
    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) p->hotbar[i] = ITEM_NONE;
    p->selected      = ITEM_NONE;      // empty hands
    p->slotCount     = HOTBAR_MAX_SLOTS;
    p->selectedSlot  = 0;
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        p->inventorySlots[i] = ITEM_NONE;
        p->inventoryAmounts[i] = 0;
    }
    p->inventorySlotCount = 0;
    p->inventoryOpen = false;
    p->inventoryCursor = 0;
    p->inventoryDragging = false;
    p->inventoryDragIndex = -1;
    p->craftMenuOpen = false;
    p->craftSel      = 0;
    p->craftScroll   = 0;
}

static void PlayerSelectSlot(Player *p, int slot) {
    if (slot < 0 || slot >= HOTBAR_MAX_SLOTS) return;
    p->selectedSlot = slot;
    if (slot < INVENTORY_SIZE && p->inventorySlots[slot] != ITEM_NONE && p->inventoryAmounts[slot] > 0) {
        p->selected = p->inventorySlots[slot];
    } else {
        p->selected = ITEM_NONE;
    }
    p->hotbar[slot] = p->selected;
}

static void PlayerToggleInventory(Player *p) {
    p->inventoryOpen = !p->inventoryOpen;
    if (!p->inventoryOpen) {
        p->inventoryDragging = false;
        p->inventoryDragIndex = -1;
    }
}

static void PlayerRefreshInventorySlotCount(Player *p) {
    p->inventorySlotCount = 0;
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (p->inventorySlots[i] != ITEM_NONE && p->inventoryAmounts[i] > 0)
            p->inventorySlotCount++;
    }
}

static void PlayerInventorySwapSlots(Player *p, int a, int b) {
    if (a < 0 || b < 0 || a >= INVENTORY_SIZE || b >= INVENTORY_SIZE || a == b) return;
    ItemID itemA = p->inventorySlots[a];
    int amtA = p->inventoryAmounts[a];
    p->inventorySlots[a] = p->inventorySlots[b];
    p->inventoryAmounts[a] = p->inventoryAmounts[b];
    p->inventorySlots[b] = itemA;
    p->inventoryAmounts[b] = amtA;
    PlayerRefreshInventorySlotCount(p);
}

static void PlayerEnsureHotbarContains(Player *p, ItemID id) {
    if (id == ITEM_NONE) return;
    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
        if (p->inventorySlots[i] == id && p->inventoryAmounts[i] > 0) {
            return;
        }
    }
    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
        if (p->inventorySlots[i] == ITEM_NONE || p->inventoryAmounts[i] <= 0) {
            p->inventorySlots[i] = id;
            p->inventoryAmounts[i] = 0;
            return;
        }
    }
}

static void PlayerSelectRelative(Player *p, int delta) {
    if (p->slotCount <= 0) return;
    int next = p->selectedSlot + delta;
    while (next < 0) next += p->slotCount;
    while (next >= p->slotCount) next -= p->slotCount;
    PlayerSelectSlot(p, next);
}

static void PlayerGiveItem(Player *p, ItemID id, int amount) {
    if (id == ITEM_NONE || amount <= 0) return;
    p->inventory[id] += amount;

    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (p->inventorySlots[i] == id && p->inventoryAmounts[i] > 0) {
            p->inventoryAmounts[i] += amount;
            PlayerEnsureHotbarContains(p, id);
            PlayerRefreshInventorySlotCount(p);
            return;
        }
    }

    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (p->inventorySlots[i] == ITEM_NONE || p->inventoryAmounts[i] <= 0) {
            p->inventorySlots[i] = id;
            p->inventoryAmounts[i] = amount;
            PlayerEnsureHotbarContains(p, id);
            PlayerRefreshInventorySlotCount(p);
            return;
        }
    }

    PlayerEnsureHotbarContains(p, id);
}

static void PlayerRemoveItem(Player *p, ItemID id, int amount) {
    if (id == ITEM_NONE || amount <= 0) return;
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (p->inventorySlots[i] != id || p->inventoryAmounts[i] <= 0) continue;
        int take = amount < p->inventoryAmounts[i] ? amount : p->inventoryAmounts[i];
        p->inventoryAmounts[i] -= take;
        p->inventory[id] -= take;
        amount -= take;
        if (p->inventoryAmounts[i] <= 0) {
            p->inventorySlots[i] = ITEM_NONE;
            p->inventoryAmounts[i] = 0;
        }
        if (amount <= 0) break;
    }
    PlayerRefreshInventorySlotCount(p);
}

static void PlayerRemoveSelectedItem(Player *p, int amount) {
    if (p->selectedSlot < 0 || p->selectedSlot >= HOTBAR_MAX_SLOTS) return;
    int slot = p->selectedSlot;
    ItemID id = p->inventorySlots[slot];
    if (id == ITEM_NONE || p->inventoryAmounts[slot] <= 0) return;
    int take = amount < p->inventoryAmounts[slot] ? amount : p->inventoryAmounts[slot];
    p->inventoryAmounts[slot] -= take;
    p->inventory[id] -= take;
    if (p->inventoryAmounts[slot] <= 0) {
        p->inventorySlots[slot] = ITEM_NONE;
        p->inventoryAmounts[slot] = 0;
        p->selected = ITEM_NONE;
    }
    PlayerRefreshInventorySlotCount(p);
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
        dir = Vector2Normalize(dir);
        Vector2 nextPosX = Vector2Add(p->pos, Vector2Scale((Vector2){dir.x, 0}, PLAYER_SPEED * dt));
        Vector2 nextPosY = Vector2Add(p->pos, Vector2Scale((Vector2){0, dir.y}, PLAYER_SPEED * dt));

        if (WorldPositionWalkable(nextPosX)) {
            p->pos.x = nextPosX.x;
        }
        if (WorldPositionWalkable(nextPosY)) {
            p->pos.y = nextPosY.y;
        }
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
static bool PlayerIsTool(ItemID id) {
    return id == ITEM_FURNACE;
}

static bool PlayerCanCraft(const Player *p, ItemID id) {
    const ItemInfo *it = &ITEMS[id];
    if (it->inA == ITEM_NONE) return false;                    // no recipe
    if (p->inventory[it->inA] < it->nA) return false;          // missing A
    if (it->inB != ITEM_NONE) {
        if (!PlayerIsTool(it->inB)) {
            if (p->inventory[it->inB] < it->nB) return false;  // missing B
        } else {
            if (p->inventory[it->inB] <= 0) return false;      // tool missing
        }
    }
    return true;
}

static bool PlayerCraft(Player *p, ItemID id) {
    if (!PlayerCanCraft(p, id)) return false;
    const ItemInfo *it = &ITEMS[id];
    PlayerRemoveItem(p, it->inA, it->nA);
    if (it->inB != ITEM_NONE && !PlayerIsTool(it->inB))
        PlayerRemoveItem(p, it->inB, it->nB);
    PlayerGiveItem(p, id, it->yield);
    return true;
}

#endif // PLAYER_H
