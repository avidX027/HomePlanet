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

#define CRAFT_QUEUE_MAX 100   // how many builds can be waiting at once
#define HOTBAR_MAX_SLOTS 7
#define INVENTORY_COLS 7    // 7 wide — the top row IS the hotbar
#define INVENTORY_ROWS 7
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
    // Magazines: rounds currently loaded in each weapon, plus the
    // reload in progress (if any).
    int     mag[ITEM_COUNT];
    float   reloadTimer;               // seconds left on the reload
    float   reloadTotal;               // ...out of this many (drives the ring)
    ItemID  reloadingItem;
    // Crafting is a QUEUE now, not an instant swap: ingredients are
    // taken up front, then each item builds for its own time.
    ItemID  craftQueue[CRAFT_QUEUE_MAX];
    int     craftQueueCount;
    float   craftProgress;             // seconds into the head item
} Player;

// ─── Pickup toasts ────────────────────────────────────────
// "+12 Stone" floating up over the player, Factorio-style. Repeats
// of the same item MERGE into the live toast rather than stacking
// up a column of near-identical lines.
#define MAX_TOASTS 10
typedef struct {
    bool   active;
    ItemID id;
    int    amount;
    float  age, life;
    float  driftX;
} PlayerToast;
static PlayerToast playerToasts[MAX_TOASTS];

static void PlayerPushToast(ItemID id, int amount) {
    if (id <= ITEM_NONE || id >= ITEM_COUNT || amount <= 0) return;
    // Merge into a recent toast of the same item.
    for (int i = 0; i < MAX_TOASTS; i++) {
        if (playerToasts[i].active && playerToasts[i].id == id &&
            playerToasts[i].age < 0.9f) {
            playerToasts[i].amount += amount;
            playerToasts[i].age = 0;         // restart its clock
            return;
        }
    }
    for (int i = 0; i < MAX_TOASTS; i++) {
        if (!playerToasts[i].active) {
            playerToasts[i] = (PlayerToast){ true, id, amount, 0, 1.8f,
                                             (float)GetRandomValue(-14, 14) };
            return;
        }
    }
}

static void PlayerUpdateToasts(float dt) {
    for (int i = 0; i < MAX_TOASTS; i++) {
        if (playerToasts[i].active && (playerToasts[i].age += dt) >= playerToasts[i].life)
            playerToasts[i].active = false;
    }
}

// Drawn in WORLD space, rising from just above the player.
static void PlayerDrawToasts(Vector2 anchor) {
    int lane = 0;
    for (int i = 0; i < MAX_TOASTS; i++) {
        PlayerToast *t = &playerToasts[i];
        if (!t->active) continue;
        float k = t->age / t->life;                    // 0..1
        float rise = 16.0f + k * 30.0f;
        unsigned char a = (unsigned char)(255 * (1.0f - k * k));   // fade late
        const char *txt = TextFormat("+%d %s", t->amount, ITEMS[t->id].name);
        int tw = MeasureText(txt, 14);
        float tx = anchor.x - tw / 2.0f + t->driftX;
        float ty = anchor.y - rise - lane * 15.0f;
        DrawText(txt, (int)tx + 1, (int)ty + 1, 14, (Color){ 0, 0, 0, (unsigned char)(a / 2) });
        DrawText(txt, (int)tx, (int)ty, 14, (Color){ 255, 255, 255, a });
        lane++;
    }
}

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
    for (int i = 0; i < ITEM_COUNT; i++) p->mag[i] = 0;
    p->reloadTimer   = 0;
    p->reloadTotal   = 0;
    p->reloadingItem = ITEM_NONE;
    p->craftQueueCount = 0;
    p->craftProgress   = 0;
    for (int i = 0; i < CRAFT_QUEUE_MAX; i++) p->craftQueue[i] = ITEM_NONE;
    for (int i = 0; i < MAX_TOASTS; i++) playerToasts[i].active = false;
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
// RULE — the backpack and craft menu can be open TOGETHER (they
// sit side by side on screen); the tech terminal and the debug
// console still want the stage to themselves.
static void PlayerCloseMenus(Player *p) {
    p->inventoryOpen = false;
    p->craftMenuOpen = false;
    p->techMenuOpen  = false;
    p->inventoryDragging = false;   // a closing menu abandons its drag
    p->inventoryDragIndex = -1;
}

static void PlayerToggleInventory(Player *p) {
    p->inventoryOpen = !p->inventoryOpen;
    p->techMenuOpen = false;
    if (!p->inventoryOpen) {
        p->inventoryDragging = false;
        p->inventoryDragIndex = -1;
    }
}

static void PlayerToggleCraftMenu(Player *p) {
    p->craftMenuOpen = !p->craftMenuOpen;
    p->techMenuOpen = false;
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

// ─── Inventory integrity ──────────────────────────────────
// THE RULE OF THIS SECTION: the SLOTS are the only truth. The
// `inventory[ITEM_COUNT]` totals and `selected` are DERIVED — never
// edited by hand, always recomputed from the slots by the function
// below. That's what makes duplication and drift impossible: there
// is no second copy of the count that can disagree.
//
// It also scrubs "ghost" entries — a slot naming an item with a
// count of 0 — which is what used to litter the hotbar with 0x
// items. (The old code deliberately created them to reserve hotbar
// space for newly discovered items; PlayerGiveItem now fills real
// hotbar slots instead, so the reservation trick is gone.)
static void PlayerRecount(Player *p) {
    for (int i = 0; i < ITEM_COUNT; i++) p->inventory[i] = 0;
    p->inventorySlotCount = 0;

    for (int i = 0; i < INVENTORY_SIZE; i++) {
        ItemID id = p->inventorySlots[i];
        // Anything invalid, empty, or zero-count becomes a clean
        // empty slot — no ghosts survive a recount.
        if (id <= ITEM_NONE || id >= ITEM_COUNT || p->inventoryAmounts[i] <= 0) {
            p->inventorySlots[i] = ITEM_NONE;
            p->inventoryAmounts[i] = 0;
            continue;
        }
        p->inventory[id] += p->inventoryAmounts[i];
        p->inventorySlotCount++;
    }

    // `selected` and the hotbar mirror are derived too.
    if (p->selectedSlot < 0 || p->selectedSlot >= HOTBAR_MAX_SLOTS) p->selectedSlot = 0;
    ItemID sel = p->inventorySlots[p->selectedSlot];
    p->selected = (sel != ITEM_NONE && p->inventoryAmounts[p->selectedSlot] > 0)
                ? sel : ITEM_NONE;
    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) p->hotbar[i] = p->inventorySlots[i];
}

static void PlayerInventorySwapSlots(Player *p, int a, int b) {
    if (a < 0 || b < 0 || a >= INVENTORY_SIZE || b >= INVENTORY_SIZE || a == b) return;
    ItemID itemA = p->inventorySlots[a];
    int amtA = p->inventoryAmounts[a];
    p->inventorySlots[a] = p->inventorySlots[b];
    p->inventoryAmounts[a] = p->inventoryAmounts[b];
    p->inventorySlots[b] = itemA;
    p->inventoryAmounts[b] = amtA;
    PlayerRecount(p);
}

static void PlayerSelectRelative(Player *p, int delta) {
    if (p->slotCount <= 0) return;
    int next = p->selectedSlot + delta;
    while (next < 0) next += p->slotCount;
    while (next >= p->slotCount) next -= p->slotCount;
    PlayerSelectSlot(p, next);
}

// Add items. Merges into an existing stack first; otherwise takes
// the first free slot, HOTBAR SLOTS FIRST — that's how a newly
// discovered item lands on your bar with a real count instead of
// the old 0x placeholder.
// Stacks cap at STACK_MAX and spill into further slots, so a big
// haul fills several slots instead of one impossible pile.
static void PlayerGiveItem(Player *p, ItemID id, int amount) {
    if (id <= ITEM_NONE || id >= ITEM_COUNT || amount <= 0) return;
    int wanted = amount;   // remember, so the toast shows what LANDED

    for (int i = 0; i < INVENTORY_SIZE && amount > 0; i++) {   // top up stacks
        if (p->inventorySlots[i] == id && p->inventoryAmounts[i] > 0 &&
            p->inventoryAmounts[i] < STACK_MAX) {
            int room = STACK_MAX - p->inventoryAmounts[i];
            int take = amount < room ? amount : room;
            p->inventoryAmounts[i] += take;
            amount -= take;
        }
    }
    for (int i = 0; i < INVENTORY_SIZE && amount > 0; i++) {   // then empty slots
        if (p->inventorySlots[i] == ITEM_NONE || p->inventoryAmounts[i] <= 0) {
            int take = amount < STACK_MAX ? amount : STACK_MAX;
            p->inventorySlots[i] = id;
            p->inventoryAmounts[i] = take;
            amount -= take;
        }
    }
    PlayerRecount(p);
    // Anything still left over: the inventory is full and the
    // remainder is simply lost — so the toast reports what actually
    // made it in, not what was offered.
    if (wanted - amount > 0) PlayerPushToast(id, wanted - amount);
}

// Remove up to `amount`, draining stacks from the BACK so your
// hotbar keeps its working stack for as long as possible.
static void PlayerRemoveItem(Player *p, ItemID id, int amount) {
    if (id <= ITEM_NONE || id >= ITEM_COUNT || amount <= 0) return;
    for (int i = INVENTORY_SIZE - 1; i >= 0 && amount > 0; i--) {
        if (p->inventorySlots[i] != id || p->inventoryAmounts[i] <= 0) continue;
        int take = amount < p->inventoryAmounts[i] ? amount : p->inventoryAmounts[i];
        p->inventoryAmounts[i] -= take;
        amount -= take;
        if (p->inventoryAmounts[i] <= 0) {
            p->inventorySlots[i] = ITEM_NONE;
            p->inventoryAmounts[i] = 0;
        }
    }
    PlayerRecount(p);
}

static void PlayerRemoveSelectedItem(Player *p, int amount) {
    if (p->selectedSlot < 0 || p->selectedSlot >= HOTBAR_MAX_SLOTS || amount <= 0) return;
    int slot = p->selectedSlot;
    if (p->inventorySlots[slot] == ITEM_NONE || p->inventoryAmounts[slot] <= 0) return;
    int take = amount < p->inventoryAmounts[slot] ? amount : p->inventoryAmounts[slot];
    p->inventoryAmounts[slot] -= take;
    if (p->inventoryAmounts[slot] <= 0) {
        p->inventorySlots[slot] = ITEM_NONE;
        p->inventoryAmounts[slot] = 0;
    }
    PlayerRecount(p);
}

// ─── Reloading ────────────────────────────────────────────
// A gun fires from p->mag[gun]. Empty it and the reload starts;
// when the timer runs out, rounds move from your stack into the
// magazine. The whole thing is one timer, which is also exactly
// what the on-screen ring reads from.
static void PlayerStartReload(Player *p, ItemID gun) {
    if (ItemMagSize(gun) <= 0) return;             // slingshot: nothing to reload
    if (p->reloadTimer > 0) return;                // already reloading
    if (p->mag[gun] >= ItemMagSize(gun)) return;   // already full
    ItemID ammo = ItemAmmoFor(gun);
    if (p->inventory[ammo] <= 0) return;           // no rounds to load

    p->reloadTotal   = ItemReloadTime(gun);
    p->reloadTimer   = p->reloadTotal;
    p->reloadingItem = gun;
}

static void PlayerUpdateReload(Player *p, float dt) {
    if (p->reloadTimer <= 0) return;
    p->reloadTimer -= dt;
    if (p->reloadTimer > 0) return;

    // Reload finished: move rounds from the stack into the mag.
    ItemID gun = p->reloadingItem;
    p->reloadTimer = 0;
    p->reloadingItem = ITEM_NONE;
    int size = ItemMagSize(gun);
    if (size <= 0) return;
    ItemID ammo = ItemAmmoFor(gun);
    int want = size - p->mag[gun];
    int have = p->inventory[ammo];
    int load = want < have ? want : have;
    if (load <= 0) return;
    PlayerRemoveItem(p, ammo, load);
    p->mag[gun] += load;
}

// ─── Q: quick-draw / cycle weapons ────────────────────────
// Empty-handed (or holding a tool) → snap to your BEST gun.
// Already holding one → cycle to the next gun on the bar. Either
// way it shuts every menu, so Q is the universal "back to the
// fight" key.
static void PlayerQuickWeapon(Player *p) {
    PlayerCloseMenus(p);

    if (ItemIsWeapon(p->selected)) {
        // Cycle: walk the bar from the next slot, wrapping.
        for (int step = 1; step <= HOTBAR_MAX_SLOTS; step++) {
            int slot = (p->selectedSlot + step) % HOTBAR_MAX_SLOTS;
            ItemID id = p->inventorySlots[slot];
            if (p->inventoryAmounts[slot] > 0 && ItemIsWeapon(id)) {
                PlayerSelectSlot(p, slot);
                return;
            }
        }
        return;   // it's the only gun you have; keep holding it
    }

    // Quick-draw: highest-ranked weapon on the bar.
    int bestSlot = -1, bestRank = 0;
    for (int slot = 0; slot < HOTBAR_MAX_SLOTS; slot++) {
        if (p->inventoryAmounts[slot] <= 0) continue;
        int rank = ItemWeaponRank(p->inventorySlots[slot]);
        if (rank > bestRank) { bestRank = rank; bestSlot = slot; }
    }
    if (bestSlot >= 0) PlayerSelectSlot(p, bestSlot);
}

// ─── Anti-stuck ───────────────────────────────────────────
// If the player ever ends up INSIDE geometry — a wall placed on
// them, a tile grown under them, a wedge between two boulders —
// the plain "can I move there?" test blocks every direction and
// they're trapped forever. So before moving, check the CURRENT
// spot: if it's illegal, spiral outward and teleport to the
// nearest legal one. Being stuck becomes impossible by
// construction, not by hoping the collision math is perfect.
static void PlayerUnstick(Player *p) {
    if (WorldPositionWalkable(p->pos)) return;

    for (float radius = 2.0f; radius <= TILE_SIZE * 4.0f; radius += 2.0f) {
        for (int a = 0; a < 16; a++) {
            float ang = a * (2.0f * PI / 16.0f);
            Vector2 probe = { p->pos.x + cosf(ang) * radius,
                              p->pos.y + sinf(ang) * radius };
            if (WorldPositionWalkable(probe)) {
                p->pos = probe;
                return;
            }
        }
    }
    // Nowhere within 4 tiles is free (buried alive) → world center.
    p->pos = (Vector2){ WORLD_SIZE * TILE_SIZE / 2.0f,
                        WORLD_SIZE * TILE_SIZE / 2.0f };
}

// ─── Movement (WASD), clamped to world edges ──────────────
static void PlayerMove(Player *p, float dt) {
    PlayerUnstick(p);   // always escape first, then walk

    Vector2 dir = { 0, 0 };
    if (IsKeyDown(KEY_W)) dir.y -= 1;
    if (IsKeyDown(KEY_S)) dir.y += 1;
    if (IsKeyDown(KEY_A)) dir.x -= 1;
    if (IsKeyDown(KEY_D)) dir.x += 1;

    // Normalize so diagonal movement isn't faster (length 1.41 -> 1).
    // Multiply by dt so speed is per-SECOND, independent of framerate.
    if (dir.x != 0 || dir.y != 0) {
        dir = Vector2Normalize(dir);
        // Empty hands (or a tool) run a hair faster than shouldering
        // a weapon — a small nudge toward putting the gun away.
        float speed = TUNE.playerSpeed;
        if (!ItemIsWeapon(p->selected)) speed *= UNARMED_SPEED_BONUS;

        float stepX = dir.x * speed * dt;
        float stepY = dir.y * speed * dt;
        Vector2 nextPosX = { p->pos.x + stepX, p->pos.y };
        Vector2 nextPosY = { p->pos.x, p->pos.y + stepY };

        bool movedX = WorldPositionWalkable(nextPosX);
        bool movedY = WorldPositionWalkable(nextPosY);
        if (movedX) p->pos.x = nextPosX.x;
        if (movedY) p->pos.y = nextPosY.y;

        // CORNER NUDGE — the classic "caught on a corner" fix. If a
        // pure-axis move is blocked, try shifting slightly along the
        // OTHER axis too: that slides you around the lip of a tile
        // instead of sticking to it. Two probes, cheap, and it makes
        // squeezing through the gaps in 3/4 rocks feel smooth.
        const float nudge = TILE_SIZE * 0.34f;
        if (!movedX && stepX != 0.0f) {
            for (int s = -1; s <= 1; s += 2) {
                Vector2 slip = { p->pos.x + stepX, p->pos.y + s * nudge };
                if (WorldPositionWalkable(slip)) {
                    p->pos.x = slip.x;
                    p->pos.y += s * nudge * 0.35f;   // ease around, don't teleport
                    break;
                }
            }
        }
        if (!movedY && stepY != 0.0f) {
            for (int s = -1; s <= 1; s += 2) {
                Vector2 slip = { p->pos.x + s * nudge, p->pos.y + stepY };
                if (WorldPositionWalkable(slip)) {
                    p->pos.y = slip.y;
                    p->pos.x += s * nudge * 0.35f;
                    break;
                }
            }
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
    // Day-cycle shadow first (worldShadowVec comes from world.h).
    DrawEllipse((int)(p->pos.x + worldShadowVec.x * 0.5f),
                (int)(p->pos.y + worldShadowVec.y * 0.5f),
                PLAYER_RADIUS * 1.05f, PLAYER_RADIUS * 0.8f, WORLD_SHADOW_COLOR);
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

// Crafting takes TIME now. The ingredients are spent immediately
// (so you can't queue ten things you can only afford once), the
// build joins the queue, and the item appears when its timer runs
// out — Factorio's model, and Rust's.
static bool PlayerCraft(Player *p, ItemID id) {
    if (p->craftQueueCount >= CRAFT_QUEUE_MAX) return false;
    if (!PlayerCanCraft(p, id)) return false;
    const ItemInfo *it = &ITEMS[id];
    PlayerRemoveItem(p, it->inA, it->nA);
    if (it->inB != ITEM_NONE && !PlayerIsTool(it->inB))
        PlayerRemoveItem(p, it->inB, it->nB);
    p->craftQueue[p->craftQueueCount++] = id;
    return true;
}

// Advance the head of the queue; deliver it when its time is up.
static void PlayerUpdateCrafting(Player *p, float dt) {
    if (p->craftQueueCount <= 0) { p->craftProgress = 0; return; }
    ItemID head = p->craftQueue[0];
    float need = ItemCraftTime(head);
    p->craftProgress += dt;
    if (p->craftProgress < need) return;

    p->craftProgress = 0;
    PlayerGiveItem(p, head, ITEMS[head].yield);
    for (int i = 1; i < p->craftQueueCount; i++) p->craftQueue[i - 1] = p->craftQueue[i];
    p->craftQueueCount--;
    p->craftQueue[p->craftQueueCount] = ITEM_NONE;
}

// Cancel one queued build and refund it exactly — same numbers we
// charged when it was queued.
static void PlayerCancelCraftAt(Player *p, int index) {
    if (index < 0 || index >= p->craftQueueCount) return;
    ItemID id = p->craftQueue[index];
    const ItemInfo *it = &ITEMS[id];
    PlayerGiveItem(p, it->inA, it->nA);
    if (it->inB != ITEM_NONE && !PlayerIsTool(it->inB)) PlayerGiveItem(p, it->inB, it->nB);

    for (int i = index + 1; i < p->craftQueueCount; i++) p->craftQueue[i - 1] = p->craftQueue[i];
    p->craftQueueCount--;
    p->craftQueue[p->craftQueueCount] = ITEM_NONE;
    // Cancelling the one in progress restarts the next from zero.
    if (index == 0) p->craftProgress = 0;
}

#endif // PLAYER_H
