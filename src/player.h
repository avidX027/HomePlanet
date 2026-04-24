#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "raymath.h"
#include "world.h"

// ============================================================
//  PLAYER.H — Home Planet Player Engine
// ============================================================

#define INVENTORY_SLOTS  32
#define HOTBAR_MAX       7       // grows from 1 up to 7
#define MAX_STACK        999

// Slot 0 is ALWAYS Hands. Other slots grow as items collected.

typedef enum {
    ITEM_NONE = 0,
    ITEM_HANDS,
    ITEM_WOOD,
    ITEM_STONE,
    ITEM_ELECTRONICS,
    ITEM_CIRCUIT,
    ITEM_CABLE,
    ITEM_ANTENNA,
    ITEM_PING_CORE,
    ITEM_PICKAXE,
    ITEM_COUNT
} ItemID;

static const char *ItemNames[ITEM_COUNT] = {
    "Nothing", "Hands", "Wood", "Stone",
    "Electronics", "Circuit", "Cable", "Antenna",
    "Ping Core", "Pickaxe"
};

static Color ItemColors[ITEM_COUNT] = {
    BLACK,
    (Color){255,220,100,255},   // Hands = yellow
    BROWN,
    DARKGRAY,
    (Color){80,200,255,255},
    (Color){60,220,100,255},
    (Color){220,180,40,255},
    (Color){180,80,220,255},
    GOLD,
    (Color){200,160,100,255}    // Pickaxe
};

// How much damage the item does per second when mining
static float ItemMiningDPS[ITEM_COUNT] = {
    0,     // NONE
    1.0f,  // HANDS — 1 dps
    0,     // WOOD
    0,     // STONE
    0, 0, 0, 0, 0,
    4.0f   // PICKAXE — 4 dps
};

typedef struct {
    ItemID id;
    int    count;
} ItemStack;

typedef struct {
    ItemID  output;
    int     outputCount;
    ItemID  inputA;
    int     countA;
    ItemID  inputB;
    int     countB;
} Recipe;

#define RECIPE_COUNT 6
static Recipe Recipes[RECIPE_COUNT] = {
    { ITEM_PICKAXE,     1, ITEM_WOOD,        5, ITEM_STONE,      10 },
    { ITEM_ELECTRONICS, 1, ITEM_WOOD,        3, ITEM_STONE,       2 },
    { ITEM_CIRCUIT,     1, ITEM_ELECTRONICS, 2, ITEM_NONE,        0 },
    { ITEM_CABLE,       2, ITEM_WOOD,        2, ITEM_ELECTRONICS, 1 },
    { ITEM_ANTENNA,     1, ITEM_CIRCUIT,     1, ITEM_STONE,       3 },
    { ITEM_PING_CORE,   1, ITEM_ANTENNA,     1, ITEM_CIRCUIT,     2 }
};

typedef struct {
    Vector2   pos;
    float     speed;
    float     radioSignal;
    ItemStack inv[INVENTORY_SLOTS];

    // HOTBAR — slot 0 is always Hands. Slots 1..6 fill as new items are picked up.
    ItemID    hotbarItem[HOTBAR_MAX];  // what item lives in each hotbar slot
    int       hotbarCount;             // how many slots are currently visible (1..HOTBAR_MAX)
    int       hotbarSel;               // selected slot 0..hotbarCount-1

    bool      craftMenuOpen;
    int       craftSel;
} Player;

// ─── INVENTORY HELPERS ────────────────────────────────────
static int PlayerCountItem(Player *p, ItemID id) {
    int total = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++)
        if (p->inv[i].id == id) total += p->inv[i].count;
    return total;
}

// Add to hotbar if this is a NEW item type AND we have room
static void PlayerMaybeAddToHotbar(Player *p, ItemID id) {
    if (id == ITEM_NONE || id == ITEM_HANDS) return;
    for (int i = 0; i < p->hotbarCount; i++) {
        if (p->hotbarItem[i] == id) return; // already there
    }
    if (p->hotbarCount < HOTBAR_MAX) {
        p->hotbarItem[p->hotbarCount] = id;
        p->hotbarCount++;
    }
}

static int PlayerAddItem(Player *p, ItemID id, int count) {
    bool isNewType = (PlayerCountItem(p, id) == 0);

    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        if (p->inv[i].id == id && p->inv[i].count < MAX_STACK) {
            int room = MAX_STACK - p->inv[i].count;
            int add  = (count <= room) ? count : room;
            p->inv[i].count += add;
            count -= add;
            if (count <= 0) {
                if (isNewType) PlayerMaybeAddToHotbar(p, id);
                return 1;
            }
        }
    }
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        if (p->inv[i].id == ITEM_NONE) {
            p->inv[i].id    = id;
            p->inv[i].count = (count > MAX_STACK) ? MAX_STACK : count;
            count -= p->inv[i].count;
            if (count <= 0) {
                if (isNewType) PlayerMaybeAddToHotbar(p, id);
                return 1;
            }
        }
    }
    return 0;
}

static int PlayerRemoveItem(Player *p, ItemID id, int count) {
    if (PlayerCountItem(p, id) < count) return 0;
    for (int i = 0; i < INVENTORY_SLOTS && count > 0; i++) {
        if (p->inv[i].id != id) continue;
        int take = (p->inv[i].count >= count) ? count : p->inv[i].count;
        p->inv[i].count -= take;
        count -= take;
        if (p->inv[i].count <= 0) { p->inv[i].id = ITEM_NONE; p->inv[i].count = 0; }
    }
    return 1;
}

static int PlayerCraft(Player *p, int recipeIdx) {
    if (recipeIdx < 0 || recipeIdx >= RECIPE_COUNT) return 0;
    Recipe *r = &Recipes[recipeIdx];
    if (r->inputA != ITEM_NONE && PlayerCountItem(p, r->inputA) < r->countA) return 0;
    if (r->inputB != ITEM_NONE && PlayerCountItem(p, r->inputB) < r->countB) return 0;
    if (r->inputA != ITEM_NONE) PlayerRemoveItem(p, r->inputA, r->countA);
    if (r->inputB != ITEM_NONE) PlayerRemoveItem(p, r->inputB, r->countB);
    PlayerAddItem(p, r->output, r->outputCount);
    return 1;
}

static void PlayerInit(Player *p) {
    p->pos         = (Vector2){ (WORLD_SIZE * TILE_SIZE)/2.0f,
                                (WORLD_SIZE * TILE_SIZE)/2.0f };
    p->speed       = 450.0f;
    p->radioSignal = 0.0f;
    p->craftMenuOpen = false;
    p->craftSel    = 0;

    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        p->inv[i].id    = ITEM_NONE;
        p->inv[i].count = 0;
    }

    // Slot 0: Hands, always present
    p->hotbarItem[0] = ITEM_HANDS;
    for (int i = 1; i < HOTBAR_MAX; i++) p->hotbarItem[i] = ITEM_NONE;
    p->hotbarCount = 1;
    p->hotbarSel   = 0;
}

static ItemID PlayerSelectedItem(Player *p) {
    if (p->hotbarSel < 0 || p->hotbarSel >= p->hotbarCount) return ITEM_NONE;
    return p->hotbarItem[p->hotbarSel];
}

static void PlayerMove(Player *p, float dt) {
    Vector2 move = {0};
    if (IsKeyDown(KEY_W)) move.y -= 1;
    if (IsKeyDown(KEY_S)) move.y += 1;
    if (IsKeyDown(KEY_A)) move.x -= 1;
    if (IsKeyDown(KEY_D)) move.x += 1;
    if (Vector2Length(move) > 0) {
        p->pos = Vector2Add(p->pos,
                    Vector2Scale(Vector2Normalize(move), p->speed * dt));
    }
    float maxPos = (WORLD_SIZE - 1) * (float)TILE_SIZE;
    if (p->pos.x < 0)      p->pos.x = 0;
    if (p->pos.x > maxPos) p->pos.x = maxPos;
    if (p->pos.y < 0)      p->pos.y = 0;
    if (p->pos.y > maxPos) p->pos.y = maxPos;
}

#endif // PLAYER_H
