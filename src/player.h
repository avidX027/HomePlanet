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
    // Survival: mobs can hurt you now.
    float   hp;
    float   hurtTimer;                 // red-flash countdown after a hit
    float   invulnTimer;               // grace period after respawning
    float   regenDelay;                // seconds until regen kicks back in
    // Research: which techs are unlocked + the tech menu's UI state.
    bool    techUnlocked[TECH_COUNT];
    bool    techMenuOpen;
    int     techSel;
    int     techScroll;
    int     placeDir;                  // 0=E 1=S 2=W 3=N — conveyor/inserter facing
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
    p->hp            = PLAYER_MAX_HP;
    p->hurtTimer     = 0;
    p->invulnTimer   = 0;
    p->regenDelay    = 0;
    for (int i = 0; i < TECH_COUNT; i++) p->techUnlocked[i] = false;
    p->techMenuOpen  = false;
    p->techSel       = 0;
    p->techScroll    = 0;
    p->placeDir      = 0;
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

// ─── Menus ────────────────────────────────────────────────
// RULE — only ONE menu can be open at a time. Both toggles below
// close everything first, then open their own menu if it wasn't
// already the open one. Keeping the rule HERE (not in main.c's
// input code) means no future key binding can break it.
static void PlayerCloseMenus(Player *p) {
    p->inventoryOpen = false;
    p->craftMenuOpen = false;
    p->techMenuOpen  = false;
    p->inventoryDragging = false;   // a closing menu abandons its drag
    p->inventoryDragIndex = -1;
}

static void PlayerToggleInventory(Player *p) {
    bool open = !p->inventoryOpen;
    PlayerCloseMenus(p);
    p->inventoryOpen = open;
}

static void PlayerToggleCraftMenu(Player *p) {
    bool open = !p->craftMenuOpen;
    PlayerCloseMenus(p);
    p->craftMenuOpen = open;
}

static void PlayerToggleTechMenu(Player *p) {
    bool open = !p->techMenuOpen;
    PlayerCloseMenus(p);
    p->techMenuOpen = open;
}

// ─── Health ───────────────────────────────────────────────
static void PlayerDamage(Player *p, float dmg) {
    if (p->invulnTimer > 0 || dmg <= 0) return;
    p->hp -= dmg;
    p->hurtTimer  = 0.4f;
    p->regenDelay = 8.0f;      // getting hit pauses regeneration
    if (p->hp <= 0) {
        // Death: respawn at the world center with full health and a
        // grace period. You KEEP your items — the real penalty is
        // the walk back (and whatever the mobs chew through while
        // you're gone).
        p->pos = (Vector2){ WORLD_SIZE * TILE_SIZE / 2.0f,
                            WORLD_SIZE * TILE_SIZE / 2.0f };
        p->hp = PLAYER_MAX_HP;
        p->invulnTimer = 3.0f;
    }
}

// Tick the health timers; slow regen after 8s without damage.
static void PlayerUpdateVitals(Player *p, float dt) {
    if (p->hurtTimer   > 0) p->hurtTimer   -= dt;
    if (p->invulnTimer > 0) p->invulnTimer -= dt;
    if (p->regenDelay  > 0) p->regenDelay  -= dt;
    else if (p->hp < PLAYER_MAX_HP) {
        p->hp += 2.0f * dt;
        if (p->hp > PLAYER_MAX_HP) p->hp = PLAYER_MAX_HP;
    }
}

// ─── Research ─────────────────────────────────────────────
static bool PlayerHasTech(const Player *p, TechID t) {
    if (t <= TECH_NONE || t >= TECH_COUNT) return true;   // ungated
    return p->techUnlocked[t];
}

static bool PlayerCanResearch(const Player *p, TechID t) {
    if (t <= TECH_NONE || t >= TECH_COUNT) return false;
    if (p->techUnlocked[t]) return false;                  // already done
    if (!PlayerHasTech(p, TECHS[t].requires)) return false; // prereq missing
    if (p->inventory[TECHS[t].costA] < TECHS[t].nA) return false;
    if (TECHS[t].costB != ITEM_NONE && p->inventory[TECHS[t].costB] < TECHS[t].nB) return false;
    return true;
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
        Vector2 nextPosX = Vector2Add(p->pos, Vector2Scale((Vector2){dir.x, 0}, TUNE.playerSpeed * dt));
        Vector2 nextPosY = Vector2Add(p->pos, Vector2Scale((Vector2){0, dir.y}, TUNE.playerSpeed * dt));

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

// ─── Held item ────────────────────────────────────────────
// Draw whatever is selected floating beside the player, on the side
// facing the mouse — so it's always obvious what you're holding.
// `aimWorld` is the mouse in WORLD pixels; main.c computes it
// (player.h knows nothing about the camera, on purpose).
static void PlayerDrawHeldItem(const Player *p, Vector2 aimWorld) {
    ItemID id = p->selected;
    if (id == ITEM_NONE || p->inventory[id] <= 0) return;

    Vector2 dir = Vector2Subtract(aimWorld, p->pos);
    if (Vector2Length(dir) < 0.001f) dir = (Vector2){ 1, 0 };  // mouse ON player → face right
    dir = Vector2Normalize(dir);

    Vector2 held = Vector2Add(p->pos, Vector2Scale(dir, PLAYER_RADIUS + 6.0f));
    float size = 12.0f;

    // Weapons and tools ROTATE to point at the cursor; materials and
    // blocks just hover in place (a spinning wood plank looks silly).
    bool aims = (id == ITEM_PISTOL || id == ITEM_SLINGSHOT || ITEMS[id].miningDPS > 0);
    if (aims) {
        // atan2 gives the angle of `dir`; +90 compensates for art
        // that faces UP in its 8x8 grid (the pistol's faces RIGHT).
        float angle = atan2f(dir.y, dir.x) * RAD2DEG;
        if (id != ITEM_PISTOL) angle += 90.0f;
        DrawItemSpriteRot(id, held, size, angle);
    } else {
        DrawItemSprite(id, held.x - size / 2, held.y - size / 2, size);
    }
}

// ─── Mining power of whatever is selected ─────────────────
static float PlayerMiningDPS(const Player *p) {
    ItemID s = p->selected;
    // A tool only counts if you actually own one.
    if (ITEMS[s].miningDPS > 0 && p->inventory[s] > 0) return ITEMS[s].miningDPS;
    return TUNE.handDPS;   // bare hands (tunable in the debug console)
}

// ─── Crafting ─────────────────────────────────────────────
// All recipe data comes from the ITEMS table — these functions
// would not change if you added 50 new items.
static bool PlayerIsTool(ItemID id) {
    return id == ITEM_FURNACE;
}

// Spend the research cost and unlock a tech. (Lives here, below
// PlayerRemoveItem, because C reads top-to-bottom.)
static bool PlayerResearch(Player *p, TechID t) {
    if (!PlayerCanResearch(p, t)) return false;
    PlayerRemoveItem(p, TECHS[t].costA, TECHS[t].nA);
    if (TECHS[t].costB != ITEM_NONE) PlayerRemoveItem(p, TECHS[t].costB, TECHS[t].nB);
    p->techUnlocked[t] = true;
    return true;
}

static bool PlayerCanCraft(const Player *p, ItemID id) {
    const ItemInfo *it = &ITEMS[id];
    if (it->inA == ITEM_NONE) return false;                    // no recipe
    if (!PlayerHasTech(p, it->tech)) return false;             // tech locked
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
