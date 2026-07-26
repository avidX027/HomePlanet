// ============================================================
//  MAIN.C — HOME PLANET: VOID RUNNER  (learning edition)
//
//  main.c is the GLUE. It owns the game loop and is the ONLY
//  file where player, world, and UI meet. Rules of the house:
//
//    config.h    numbers to tweak        (depends on: nothing)
//    gamedata.h  what items/tiles ARE    (depends on: nothing)
//    world.h     the tile grid           (uses config, gamedata)
//    player.h    the player              (uses config, gamedata)
//    ui.h        drawing menus           (reads player + tables)
//    main.c      input + rules           (uses everything)
//
//  Nothing lower in the list is allowed to include anything
//  higher: world.h can never know players exist. That one-way
//  flow is why editing one part can't quietly break another.
//  (In software-design terms this is called a "layered
//  architecture" or a "dependency hierarchy".)
//
//  C CONCEPT — the frame loop: a game is just
//     while (window open) { read input; update state; draw; }
//  repeated ~60x/second. dt ("delta time") is how many seconds
//  the last frame took; multiply speeds by it and the game runs
//  the same on a slow laptop and a 144Hz desktop.
// ============================================================
// KEEP IN MIND THIS IS OVER-COMMENTED

// C CONCEPT — #include literally copy-pastes another file's text
// here before compiling. <angle brackets> search system folders;
// "quotes" search your project folder first.
#include "raylib.h"     // the graphics/input/window library
#include <string.h>     // memcmp, memcpy — raw-memory helpers used by save/load
#include "config.h"     // tunable numbers (speeds, sizes, damage...)
#include "gamedata.h"   // the ITEMS[] and TILES[] data tables
#include "world.h"      // the 2D tile grid + WorldDamageTile etc.
#include "player.h"     // the Player struct + movement/inventory
#include "entities.h"   // mobs, machines, bots, projectiles, bombs
#include "ui.h"         // buttons, hotbar, menus
#include "debug.h"      // the F3 live-tuning console

// C CONCEPT — file-scope state: variables declared OUTSIDE any
// function live for the whole program (not just one function call).
// The keyword `static` here means "private to this .c file" — no
// other file can see or touch these, which keeps the glue contained.
// Kept minimal on purpose: the player, a camera, one screen flag.

// C CONCEPT — enum: a set of named integer constants. Under the
// hood SCREEN_TITLE is 0, SCREEN_GAME is 1, and so on. `typedef`
// gives the type a short name so we can write `Screen` everywhere.
typedef enum { SCREEN_TITLE, SCREEN_GAME, SCREEN_PAUSE, SCREEN_SETTINGS } Screen;

// This one variable is the whole "which screen am I on" state
// machine. The main loop at the bottom switches behavior on it.
static Screen   screen = SCREEN_TITLE;
// Set when the player asks to quit (Escape on the title screen).
// We only FLAG it here and let the main loop exit normally —
// calling CloseWindow() mid-frame and then drawing would crash.
static bool     quitRequested = false;
static Player   player;              // THE player (single-player game)
static Camera2D camera = { 0 };      // raylib camera that follows the player

// C CONCEPT — `= { 0 }` zero-initializes an entire struct: every
// field becomes 0/false/NULL. Without it, file-scope statics are
// zeroed anyway, but writing it makes the intent explicit.
static UIButton titleStartButton    = { 0 };
static UIButton titleContinueButton = { 0 };
static UIButton pauseSaveButton     = { 0 };
static UIButton pauseLoadButton     = { 0 };
static UIButton pauseResumeButton   = { 0 };
static UIButton pauseSettingsButton = { 0 };
static UIButton pauseQuitButton     = { 0 };
static UIButton settingsBackButton  = { 0 };

// The controls reference shown in Settings. One table, so adding a
// binding means adding ONE row here — the menu sizes itself.
static const struct { const char *keys; const char *action; } CONTROLS[] = {
    { "W A S D",      "Move" },
    { "Left click",   "Mine / shoot" },
    { "Right click",  "Place / interact (chest, turret, research)" },
    { "Mouse wheel",  "Cycle hotbar" },
    { "Ctrl + wheel", "Zoom camera" },
    { "1 - 7",        "Select hotbar slot" },
    { "E",            "Backpack" },
    { "TAB",          "Crafting" },
    { "R",            "Rotate conveyor / inserter" },
    { "G (hold)",     "World map" },
    { "F3",           "Debug console" },
    { "F5 / F9",      "Quick save / quick load" },
    { "ESC",          "Close menu / pause" },
};
#define CONTROLS_COUNT ((int)(sizeof(CONTROLS) / sizeof(CONTROLS[0])))

// (The Projectile pool moved to entities.h — turrets fire the same
// projectiles the player does, so the pool lives with the turrets.)

// Weapon fire-rate state: counts down between automatic SMG shots.
static float smgCooldown = 0;

// Save files start with a tiny header so we can recognize our own
// files and reject garbage. `char magic[4]` holds the 4 letters
// "HPSV" (Home Planet SaVe) — a common trick called a magic number.
typedef struct {
    char magic[4];
    int  version;   // bump this when the save format changes
} SaveHeader;

#define SAVE_MAGIC "HPSV"
#define SAVE_VERSION 3   // v3: 384 world, rock variants, fog of war

// (CraftableCount and CraftableAtRow used to live here; they moved
// to gamedata.h because ui.h needs them too — pure table queries
// belong next to the table.)

// ─── Mining: where player rules meet world rules ─────────────
// Called the moment the PLAYER breaks a tile by hand. `before` is
// what the tile WAS (captured before the break — afterwards the
// grid already says grass). The loot rolls live in gamedata.h
// (RollTileBreakDrops) because drills and bots share them; here we
// just hand the results over — and salvage any machine contents.
static void GiveTileBreakDrops(int tx, int ty, TileType before) {
    int drops[ITEM_COUNT];
    RollTileBreakDrops(before, drops);
    for (int d = 1; d < ITEM_COUNT; d++) {
        if (drops[d] > 0) PlayerGiveItem(&player, (ItemID)d, drops[d]);
    }
    // Mining your own chest hands back what was inside it.
    RemoveMachineAt(tx, ty, &player);
}

// ─── Save-file hygiene ───────────────────────────────────────
// A save file is untrusted input: it might be from an older game
// version, hand-edited, or corrupted on disk. This function drags
// every loaded value back into a legal range so bad data can't
// crash the game (e.g. an out-of-range slot index would otherwise
// be used to index an array — see the memory-corruption warning
// in UpdateProjectiles).
static void ValidateLoadedPlayer(Player *p) {
    // Clamp position inside the world (accounting for the player's
    // radius so they can't be half-embedded in the border).
    float minPos = PLAYER_RADIUS;
    float maxPos = (WORLD_SIZE * TILE_SIZE) - PLAYER_RADIUS;
    if (p->pos.x < minPos) p->pos.x = minPos;
    if (p->pos.y < minPos) p->pos.y = minPos;
    if (p->pos.x > maxPos) p->pos.x = maxPos;
    if (p->pos.y > maxPos) p->pos.y = maxPos;

    // Force sane hotbar values.
    p->slotCount = HOTBAR_MAX_SLOTS;
    if (p->selectedSlot < 0 || p->selectedSlot >= HOTBAR_MAX_SLOTS) p->selectedSlot = 0;

    // No negative item counts.
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (p->inventory[i] < 0) p->inventory[i] = 0;
    }

    // Every hotbar entry must be a real ItemID (or ITEM_NONE).
    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
        if (p->hotbar[i] < ITEM_NONE || p->hotbar[i] >= ITEM_COUNT) {
            p->hotbar[i] = ITEM_NONE;
        }
    }

    // Same for the grid inventory, and keep slot/amount consistent:
    // an invalid slot loses its count; an empty slot can't have a count.
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (p->inventorySlots[i] < ITEM_NONE || p->inventorySlots[i] >= ITEM_COUNT) {
            p->inventorySlots[i] = ITEM_NONE;
            p->inventoryAmounts[i] = 0;
            continue;
        }
        if (p->inventoryAmounts[i] < 0) p->inventoryAmounts[i] = 0;
        if (p->inventorySlots[i] == ITEM_NONE) p->inventoryAmounts[i] = 0;
    }

    // Menu cursors back into range.
    if (p->inventoryCursor < 0 || p->inventoryCursor >= INVENTORY_SIZE) p->inventoryCursor = 0;
    if (p->inventoryDragIndex < -1 || p->inventoryDragIndex >= INVENTORY_SIZE) p->inventoryDragIndex = -1;
    if (p->craftSel < 0) p->craftSel = 0;
    if (p->craftScroll < 0) p->craftScroll = 0;

    // Craft-menu selection can't exceed the number of recipes
    // (which could have shrunk between game versions).
    int craftCount = CraftableCount();
    if (craftCount <= 0) {
        p->craftSel = 0;
        p->craftScroll = 0;
    } else {
        if (p->craftSel >= craftCount) p->craftSel = craftCount - 1;
        if (p->craftScroll >= craftCount) p->craftScroll = craftCount - 1;
    }

    // Recompute `selected` (the item in hand) from the slot data
    // instead of trusting the saved value — derived state should
    // always be derived, never loaded.
    ItemID selectedId = ITEM_NONE;
    if (p->selectedSlot >= 0 && p->selectedSlot < HOTBAR_MAX_SLOTS) {
        ItemID slotId = p->inventorySlots[p->selectedSlot];
        if (slotId != ITEM_NONE && p->inventoryAmounts[p->selectedSlot] > 0) {
            selectedId = slotId;
        }
    }
    p->selected = selectedId;

    // Survival + research state back into legal ranges.
    if (p->hp <= 0 || p->hp > PLAYER_MAX_HP) p->hp = PLAYER_MAX_HP;
    p->hurtTimer = 0;
    p->invulnTimer = 0;
    p->regenDelay = 0;
    p->placeDir &= 3;
    if (p->techSel < 0 || p->techSel >= TECH_COUNT - 1) p->techSel = 0;
    if (p->techScroll < 0 || p->techScroll >= TECH_COUNT - 1) p->techScroll = 0;

    // Never resume mid-drag or mid-menu: transient UI state is
    // meaningless after a load (and a hand-edited save could claim
    // both menus were open at once, which we never allow).
    PlayerCloseMenus(p);
}

// ─── Save & load ─────────────────────────────────────────────
// STRATEGY — binary snapshot: fwrite dumps the raw bytes of the
// player struct and the world array straight to disk; fread pulls
// them back. Dead simple, but the file only makes sense to the
// exact same build (struct layout, sizes). Fine for a toy save;
// real games use versioned/serialized formats. (The magic+version
// header is our small nod toward that.)
static void GameSave(void) {
    // "wb" = Write, Binary. fopen returns NULL on failure (disk
    // full, no permission...), so check before using it.
    FILE *f = fopen(SAVE_FILE, "wb");
    if (f == NULL) return;
    SaveHeader header = { {'H','P','S','V'}, SAVE_VERSION };
    // fwrite(pointer-to-data, size-of-one, how-many, file)
    fwrite(&header, sizeof(header), 1, f);
    fwrite(&player, sizeof(player), 1, f);
    fwrite(world,  sizeof(world),  1, f);  // `world` is an array, so it
                                           // already "decays" to a pointer —
                                           // no & needed (unlike the structs).
    fwrite(worldExplored, sizeof(worldExplored), 1, f);   // your scouted fog
    EntitiesWrite(f);   // machines, mobs, bots, the game clock
    fclose(f);  // ALWAYS close what you open, or data may never hit disk.
}

static bool GameLoad(void) {
    FILE *f = fopen(SAVE_FILE, "rb");   // Read, Binary
    if (f == NULL) return false;        // no save file → report failure

    // Measure the file: seek to the END, ask "where am I?" (ftell),
    // then seek back to the START to actually read. Every step can
    // fail, and every failure path must fclose(f) first — forgetting
    // that leaks a file handle. (This repetition is why people joke
    // that half of C is error handling.)
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long fileSize = ftell(f);
    if (fileSize < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    (void)fileSize;   // kept for the honest measurement above

    // Format v2: header + player + world + entities.
    // IMPORTANT PATTERN — read into TEMPORARY variables first, and
    // only copy into the real `player`/`world` once EVERYTHING has
    // been read and verified. If we read straight into the live
    // state and the file turned out truncated halfway, we'd be left
    // with a half-loaded, corrupted game.
    // C GOTCHA — the world copy is `static`: at 256x256 tiles it's
    // ~0.8MB, which would overflow the ~1MB Windows stack if it
    // lived there as a local variable.
    SaveHeader header = { 0 };
    Player loadedPlayer = { 0 };
    static Tile loadedWorld[WORLD_SIZE][WORLD_SIZE];
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return false;
    }
    // memcmp compares raw bytes: is the magic "HPSV" and the
    // version one we understand? If not, refuse the file. (Saves
    // from before the big world/mob update are version 1 — the
    // world size and player struct changed, so they can't load.)
    if (memcmp(header.magic, SAVE_MAGIC, 4) != 0 || header.version != SAVE_VERSION) {
        fclose(f);
        return false;
    }
    if (fread(&loadedPlayer, sizeof(loadedPlayer), 1, f) != 1) {
        fclose(f);
        return false;
    }
    if (fread(loadedWorld, sizeof(loadedWorld), 1, f) != 1) {
        fclose(f);
        return false;
    }

    // All core reads succeeded → NOW commit to live state.
    player = loadedPlayer;                        // structs copy with `=`
    memcpy(world, loadedWorld, sizeof(world));    // arrays don't; use memcpy
    ValidateLoadedPlayer(&player);                // sanitize untrusted data

    // The fog grid; if it's missing, start unscouted around you.
    if (fread(worldExplored, sizeof(worldExplored), 1, f) != 1) {
        memset(worldExplored, 0, sizeof(worldExplored));
        WorldRevealAround(player.pos, FOW_REVEAL_TILES);
    }

    // Entities come last; if that block is missing or truncated we
    // rebuild a fresh entity state on the loaded map instead of
    // failing the whole load.
    if (!EntitiesRead(f)) {
        EntitiesReset();
        EntitiesRegisterWorldMachines();
    }
    fclose(f);
    worldMinimapDirty = true;   // new map on screen → new minimap
    return true;
}

// Everything a brand-new world needs, in the right order.
static void NewGame(void) {
    WorldInit();
    PlayerInit(&player);
    EntitiesReset();
    EntitiesRegisterWorldMachines();   // give every spawner its brain
}

// ─── Mouse actions in the world: mine, place, shoot ──────────
static void UpdateMiningAndPlacing(float dt) {
    // Mouse over the hotbar? That click belongs to the UI — don't
    // ALSO mine/shoot/place through it into the world behind it.
    if (UiHotbarSlotAt(&player, GetMousePosition()) >= 0) return;

    // Convert the mouse from SCREEN pixels to WORLD pixels.
    // (The camera follows the player, so these differ: the same
    // screen pixel points at different world spots as you walk.)
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    int tx = (int)(mouse.x / TILE_SIZE);       // which tile is that?
    int ty = (int)(mouse.y / TILE_SIZE);
    // Check placement bounds — never index world[][] out of range.
    if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) return;

    // ── Weapons FIRST, before any reach check ────────────────
    // Weapons fire toward the cursor 100% of the time — where you
    // CLICK is a direction, not a destination. (The old code ran
    // the reach gate first, so clicking past your reach silently
    // ate the shot. That was the "sometimes nothing fires" bug.)
    ItemID held = player.selected;
    Vector2 muzzle = Vector2Add(player.pos,
        Vector2Scale(Vector2Normalize(Vector2Subtract(mouse, player.pos)), PLAYER_RADIUS + 8));

    if (held == ITEM_SLINGSHOT) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && player.inventory[ITEM_SMALL_STONE] > 0) {
            PlayerRemoveItem(&player, ITEM_SMALL_STONE, 1);   // ammo is consumed...
            SpawnProjectile(player.pos, mouse, ITEM_SMALL_STONE, false);  // ...and flies
            AddEffect(EFFECT_FLASH, muzzle, 5, 0.05f);
        }
        return;
    }
    if (held == ITEM_PISTOL) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && player.inventory[ITEM_BULLET] > 0) {
            PlayerRemoveItem(&player, ITEM_BULLET, 1);
            SpawnProjectile(player.pos, mouse, ITEM_BULLET, false);
            AddEffect(EFFECT_FLASH, muzzle, 7, 0.06f);
            entShake += 0.8f;
        }
        return;
    }
    if (held == ITEM_SMG) {
        // Full auto: fires as long as LMB is DOWN, one bullet per
        // SMG_FIRE_INTERVAL. The cooldown ticks in UpdateGame.
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && smgCooldown <= 0 &&
            player.inventory[ITEM_BULLET] > 0) {
            PlayerRemoveItem(&player, ITEM_BULLET, 1);
            // A pinch of recoil spread so it feels like an SMG.
            Vector2 spread = Vector2Rotate(Vector2Subtract(mouse, player.pos),
                                           GetRandomValue(-40, 40) / 1000.0f);
            SpawnProjectile(player.pos, Vector2Add(player.pos, spread), ITEM_BULLET, false);
            AddEffect(EFFECT_FLASH, muzzle, 6, 0.05f);
            entShake += 0.35f;
            smgCooldown = SMG_FIRE_INTERVAL;
        }
        return;
    }
    if (held == ITEM_SHOTGUN) {
        // One trigger pull → a fan of pellets, 2 bullets of ammo.
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && player.inventory[ITEM_BULLET] >= 2) {
            PlayerRemoveItem(&player, ITEM_BULLET, 2);
            for (int pellet = 0; pellet < SHOTGUN_PELLETS; pellet++) {
                Vector2 spread = Vector2Rotate(Vector2Subtract(mouse, player.pos),
                                               GetRandomValue(-140, 140) / 1000.0f);
                SpawnProjectile(player.pos, Vector2Add(player.pos, spread), ITEM_BULLET, false);
            }
            AddEffect(EFFECT_FLASH, muzzle, 10, 0.08f);
            entShake += 2.6f;
        }
        return;
    }

    // ── Everything below (mine, place, interact) needs REACH ──
    if (Vector2Distance(player.pos, mouse) > TUNE.playerReach) return;

    // ── RMB interactions (Pressed = deliberate single actions) ──
    TileType targetTile = world[tx][ty].type;
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        // Research computer → open the tech tree.
        if (targetTile == TILE_RESEARCH) {
            PlayerToggleTechMenu(&player);
            return;
        }
        // Turret + bullets in hand → load 10.
        if (targetTile == TILE_TURRET && held == ITEM_BULLET) {
            Machine *m = MachineAt(tx, ty);
            if (m != NULL && player.inventory[ITEM_BULLET] > 0) {
                int load = player.inventory[ITEM_BULLET] < 10 ? player.inventory[ITEM_BULLET] : 10;
                PlayerRemoveItem(&player, ITEM_BULLET, load);
                m->ammo += load;
            }
            return;
        }
        // Chest/drill: empty hand withdraws everything; holding a
        // (non-placeable) item deposits that stack.
        if (targetTile == TILE_CHEST || targetTile == TILE_DRILL) {
            Machine *m = MachineAt(tx, ty);
            if (m != NULL) {
                if (held == ITEM_NONE) {
                    MachineGiveContentsTo(m, &player);
                } else if (targetTile == TILE_CHEST && !ITEMS[held].placeable &&
                           player.inventory[held] > 0) {
                    int amount = player.inventory[held];
                    if (MachineAddItem(m, held, amount) > 0)
                        PlayerRemoveItem(&player, held, amount);
                }
            }
            return;
        }
        // Bomb → arm it on the target tile and RUN.
        if (held == ITEM_BOMB && player.inventory[ITEM_BOMB] > 0) {
            PlayerRemoveItem(&player, ITEM_BOMB, 1);
            PlaceBomb((Vector2){ (tx + 0.5f) * TILE_SIZE, (ty + 0.5f) * TILE_SIZE });
            return;
        }
        // Mining bot → deploy the drone right here.
        if (held == ITEM_MINING_BOT && player.inventory[ITEM_MINING_BOT] > 0) {
            PlayerRemoveItem(&player, ITEM_MINING_BOT, 1);
            SpawnBot((Vector2){ (tx + 0.5f) * TILE_SIZE, (ty + 0.5f) * TILE_SIZE });
            return;
        }
    }

    // LMB (held): damage the tile; collect drops if it broke.
    // DPS * dt = damage this frame — same dt trick as movement, so
    // mining speed is identical at 30fps and 144fps.
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        TileType before = world[tx][ty].type;   // capture BEFORE it breaks
        if (WorldDamageTile(tx, ty, PlayerMiningDPS(&player) * dt)) {
            GiveTileBreakDrops(tx, ty, before);
        }
    }

    // RMB (held): place the selected item continuously while dragging.
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        const ItemInfo *it = &ITEMS[player.selected];
        if (it->placeable &&                          // is this item placeable at all?
            player.selected != ITEM_NONE &&           // holding something?
            player.inventory[player.selected] > 0 &&  // actually own one?
            world[tx][ty].type == TILE_GRASS) {       // only build on open ground
            WorldSetTile(tx, ty, it->places);         // grass → the item's tile
            // Machines get their per-instance state (a Machine); the
            // conveyor/inserter take the current R-rotation facing.
            if (it->places == TILE_CHEST || it->places == TILE_DRILL ||
                it->places == TILE_CONVEYOR || it->places == TILE_INSERTER ||
                it->places == TILE_TURRET || it->places == TILE_LASER_TURRET ||
                it->places == TILE_RESEARCH) {
                AddMachineAt(tx, ty, it->places, player.placeDir);
            }
            PlayerRemoveSelectedItem(&player, 1);     // consume one from inventory
        }
    }
}

// ─── One frame of gameplay ───────────────────────────────────
// Reading order matters here: the function is a chain of
// "if a menu is open, handle ONLY that menu and return" blocks.
// That's how menus pause the world without a separate pause flag —
// the movement/mining code at the bottom simply never runs.
static void UpdateGame(float dt) {
    // Toggle keys work regardless of what's open. Only ONE menu can
    // be open at a time — the player.h toggles enforce it for the
    // backpack/crafting pair, and the debug console is closed here
    // by hand — so pressing E inside any menu SWITCHES to the
    // backpack instead of stacking menus on top of each other.
    if (IsKeyPressed(KEY_F3)) {
        debugMenuOpen = !debugMenuOpen;
        if (debugMenuOpen) PlayerCloseMenus(&player);
    }
    if (IsKeyPressed(KEY_E))   { debugMenuOpen = false; PlayerToggleInventory(&player); }
    if (IsKeyPressed(KEY_TAB)) { debugMenuOpen = false; PlayerToggleCraftMenu(&player); }

    // Escape backs out one layer at a time: an open menu closes
    // first; with nothing open it brings up the pause screen.
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (debugMenuOpen) {
            debugMenuOpen = false;
        } else if (player.inventoryOpen || player.craftMenuOpen || player.techMenuOpen) {
            PlayerCloseMenus(&player);
        } else {
            screen = SCREEN_PAUSE;
        }
        return;
    }

    // ── Debug console (F3) ───────────────────────────────────
    // Open console pauses the world like the other menus. All of
    // its OWN input (sliders, list clicks, wheel) happens inside
    // DebugMenuDraw — see debug.h for why it's immediate-mode.
    if (debugMenuOpen) return;

    // Number keys 1..N select hotbar slots. KEY_ONE + k works
    // because raylib key codes are consecutive: KEY_ONE+1 == KEY_TWO.
    for (int k = 0; k < HOTBAR_MAX_SLOTS; k++)
        if (IsKeyPressed(KEY_ONE + k)) PlayerSelectSlot(&player, k);

    // ── Inventory screen (drag & drop) ───────────────────────
    if (player.inventoryOpen) {
        // Ask the UI module where it draws the slots, so the click
        // detection here always matches the pixels on screen. If we
        // duplicated the layout math, the two would drift apart.
        InventoryLayout layout = { 0 };
        UiGetInventoryLayout(&player, &layout);

        // Which slot is the mouse over? Walk the grid, build each
        // slot's rectangle, test the mouse point against it.
        Vector2 mouse = GetMousePosition();  // menus live in SCREEN space (no camera)
        int hoveredIndex = -1;               // -1 = "not over any slot"
        for (int r = 0; r < INVENTORY_ROWS; r++) {
            for (int c = 0; c < INVENTORY_COLS; c++) {
                int idx = r * INVENTORY_COLS + c;   // 2D (row,col) → 1D index
                int x = layout.startX + c * (layout.slotSize + layout.gap);
                int y = layout.startY + r * (layout.slotSize + layout.gap);
                Rectangle rect = { (float)x, (float)y, (float)layout.slotSize, (float)layout.slotSize };
                if (CheckCollisionPointRec(mouse, rect)) {
                    player.inventoryCursor = idx;
                    hoveredIndex = idx;
                }
            }
        }

        // Mouse DOWN on a non-empty slot: begin dragging it.
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (hoveredIndex >= 0 && hoveredIndex < INVENTORY_SIZE) {
                ItemID id = player.inventorySlots[hoveredIndex];
                if (id != ITEM_NONE && player.inventoryAmounts[hoveredIndex] > 0) {
                    player.inventoryDragging = true;
                    player.inventoryDragIndex = hoveredIndex;  // remember the source slot
                    player.inventoryCursor = hoveredIndex;
                } else {
                    player.inventoryCursor = hoveredIndex;     // empty slot: just move cursor
                }
            }
        }

        // Mouse UP while dragging: decide what the drag meant.
        if (player.inventoryDragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            int from = player.inventoryDragIndex;
            if (hoveredIndex >= 0 && hoveredIndex != from) {
                // Dropped on a DIFFERENT slot → swap the two stacks.
                PlayerInventorySwapSlots(&player, from, hoveredIndex);
            } else if (from >= 0 && from < HOTBAR_MAX_SLOTS) {
                // Dropped back where it started (or off the grid) →
                // treat it as a plain click: a hotbar slot equips
                // its item. (PlayerSelectSlot handles empty slots.)
                PlayerSelectSlot(&player, from);
            }
            // Either way, the drag is over.
            player.inventoryDragging = false;
            player.inventoryDragIndex = -1;
        }
        return;  // inventory open → the world is paused; skip everything below
    }

    // ── Craft menu ───────────────────────────────────────────
    if (player.craftMenuOpen) {
        // Menu open: navigation only; the world is paused.
        CraftLayout craftLayout = { 0 };
        UiGetCraftLayout(&craftLayout);
        int n = CraftableCount();
        int cols = craftLayout.cols;
        int gridRows = (n + cols - 1) / cols;    // total grid rows
        int visibleRows = craftLayout.visibleRows;
        if (n > 0) {
            // The recipe count can change live (the debug console can
            // delete recipes), so drag the selection back into range.
            if (player.craftSel >= n) player.craftSel = n - 1;

            // The mouse wheel scrolls the grid WINDOW by rows, from
            // anywhere on screen; the selection stays put.
            float wheel = GetMouseWheelMove();
            if (wheel > 0) player.craftScroll--;
            if (wheel < 0) player.craftScroll++;

            // 2D navigation: A/D (or arrows) step sideways, W/S hop
            // a whole row. Clamped, not wrapped — a grid that wraps
            // in both axes is a maze.
            int move = 0;
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) move = 1;
            if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) move = -1;
            if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) move = cols;
            if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) move = -cols;
            if (move != 0) {
                int next = player.craftSel + move;
                if (next >= 0 && next < n) player.craftSel = next;
                // Keyboard drags the window along to keep the
                // selection visible; wheel scrolling doesn't.
                int selRow = player.craftSel / cols;
                if (selRow < player.craftScroll) {
                    player.craftScroll = selRow;
                } else if (selRow >= player.craftScroll + visibleRows) {
                    player.craftScroll = selRow - visibleRows + 1;
                }
            }
            // Clamp the window to the grid (second line wins when
            // the grid is shorter than the window).
            if (player.craftScroll > gridRows - visibleRows) player.craftScroll = gridRows - visibleRows;
            if (player.craftScroll < 0) player.craftScroll = 0;
        }
        if (IsKeyPressed(KEY_ENTER)) PlayerCraft(&player, CraftableAtRow(player.craftSel));

        // Mouse: hover (when actually moving) selects a cell, click
        // crafts it. Cell rects come from ui.h (UiCraftCellRect) —
        // the same function the drawing uses, so click targets always
        // match pixels.
        Vector2 mouseDelta = GetMouseDelta();
        bool mouseMoved = (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f);
        for (int r = 0; r < visibleRows; r++) {
            for (int c = 0; c < cols; c++) {
                int idx = (player.craftScroll + r) * cols + c;
                if (idx >= n) break;
                UIButton cellButton = { UiCraftCellRect(&craftLayout, r, c), "", false, false };
                UiButtonUpdate(&cellButton);
                if (cellButton.hovered && mouseMoved) player.craftSel = idx;
                if (cellButton.clicked) PlayerCraft(&player, CraftableAtRow(idx));
            }
        }
        return;  // craft menu open → world paused
    }

    // ── Tech tree (RMB a Research Computer to open) ──────────
    if (player.techMenuOpen) {
        TechLayout techLayout = { 0 };
        UiGetTechLayout(&techLayout);
        int total = TECH_COUNT - 1;

        // Same navigation scheme as the craft menu: wheel scrolls
        // the window, W/S move the selection, hover follows the
        // mouse only when it moves.
        float wheel = GetMouseWheelMove();
        if (wheel > 0) player.techScroll--;
        if (wheel < 0) player.techScroll++;
        bool moveDown = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
        bool moveUp   = IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W);
        if (moveDown) player.techSel = (player.techSel + 1) % total;
        if (moveUp)   player.techSel = (player.techSel + total - 1) % total;
        if (moveDown || moveUp) {
            if (player.techSel < player.techScroll) {
                player.techScroll = player.techSel;
            } else if (player.techSel >= player.techScroll + techLayout.visibleRows) {
                player.techScroll = player.techSel - techLayout.visibleRows + 1;
            }
        }
        if (player.techScroll > total - techLayout.visibleRows) player.techScroll = total - techLayout.visibleRows;
        if (player.techScroll < 0) player.techScroll = 0;

        if (IsKeyPressed(KEY_ENTER)) PlayerResearch(&player, (TechID)(player.techSel + 1));

        Vector2 mouseDelta = GetMouseDelta();
        bool mouseMoved = (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f);
        int endRow = player.techScroll + techLayout.visibleRows;
        if (endRow > total) endRow = total;
        for (int rIdx = player.techScroll; rIdx < endRow; rIdx++) {
            UIButton techButton = { UiTechRowRect(&techLayout, rIdx - player.techScroll), "", false, false };
            UiButtonUpdate(&techButton);
            if (techButton.hovered && mouseMoved) player.techSel = rIdx;
            if (techButton.clicked) PlayerResearch(&player, (TechID)(rIdx + 1));
        }
        return;  // tech menu open → world paused
    }

    // ── No menus open: normal gameplay input ─────────────────

    // R rotates the placement direction for conveyors/inserters.
    if (IsKeyPressed(KEY_R)) player.placeDir = (player.placeDir + 1) & 3;

    // Clicking the on-screen hotbar selects that slot. The hit-test
    // (UiHotbarSlotAt) shares its layout math with UiDrawHotbar, so
    // the clickable area always matches the drawn pixels. The same
    // helper stops the click reaching the world — see the guard at
    // the top of UpdateMiningAndPlacing.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int clickedSlot = UiHotbarSlotAt(&player, GetMousePosition());
        if (clickedSlot >= 0) PlayerSelectSlot(&player, clickedSlot);
    }

    // Mouse wheel: Ctrl+wheel zooms the camera, plain wheel cycles
    // the hotbar (wheel up = previous slot, down = next). Kept as a
    // float: trackpads report fractional scrolls that an int would
    // truncate to 0, making the wheel appear dead.
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            camera.zoom += wheel * 0.1f;
            if (camera.zoom < 0.6f) camera.zoom = 0.6f;   // clamp: not too far out
            if (camera.zoom > 2.0f) camera.zoom = 2.0f;   // ...or in
        } else {
            if (wheel > 0) PlayerSelectRelative(&player, -1);
            if (wheel < 0) PlayerSelectRelative(&player, 1);
        }
    }

    // Quick save/load hotkeys. (Escape → pause is handled at the
    // top of this function, so it can also close menus.)
    if (IsKeyPressed(KEY_F5)) GameSave();
    if (IsKeyPressed(KEY_F9)) GameLoad();

    // Finally, advance the simulation for this frame.
    if (smgCooldown > 0) smgCooldown -= dt;
    PlayerMove(&player, dt);
    PlayerUpdateVitals(&player, dt);
    WorldRevealAround(player.pos, FOW_REVEAL_TILES);   // push back the fog
    // Holding G shows the world map — mouse clicks belong to the map
    // then, not to mining/shooting under the overlay.
    if (!IsKeyDown(KEY_G)) UpdateMiningAndPlacing(dt);
    EntitiesUpdate(dt, &player);   // mobs, machines, bots, bullets, bombs

    // Camera keeps the player centered even when the window resizes.
    // target = the WORLD point to look at; offset = the SCREEN point
    // to pin it to (the center). Re-reading the screen size every
    // frame is what makes resizing "just work".
    camera.target = player.pos;
    // Screen shake: nudge the look-at point by the current shake
    // amount. It decays in EntitiesUpdate, so thumps stay thumps.
    if (entShake > 0) {
        camera.target.x += GetRandomValue(-100, 100) / 100.0f * entShake;
        camera.target.y += GetRandomValue(-100, 100) / 100.0f * entShake;
    }
    camera.offset = (Vector2){ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
}

// Draw one frame. Notice update and draw are SEPARATE functions:
// update changes state, draw only reads it. Keeping that split is
// one of the most useful habits in game code.
static void DrawGame(void) {
    BeginMode2D(camera);     // everything until EndMode2D is WORLD space
        // Both world and entity layers cull to the visible slice —
        // the map is 65k tiles, the screen shows a few hundred.
        Vector2 viewTL = GetScreenToWorld2D((Vector2){ 0, 0 }, camera);
        Vector2 viewBR = GetScreenToWorld2D((Vector2){ (float)GetScreenWidth(),
                                                       (float)GetScreenHeight() }, camera);
        WorldDraw(viewTL, viewBR, player.pos);  // (back-to-front: ground first, ...
        EntitiesDrawWorld(viewTL, viewBR);  // ... machines/mobs/bots/bullets, ...
        PlayerDraw(&player);            //  ... you on top)
        // The held item floats beside the player, facing the mouse;
        // we convert the mouse to world space here because player.h
        // doesn't know the camera exists.
        PlayerDrawHeldItem(&player, GetScreenToWorld2D(GetMousePosition(), camera));
    EndMode2D();

    // Getting hit flashes a red vignette (screen space from here on).
    if (player.hurtTimer > 0) {
        unsigned char a = (unsigned char)(120 * (player.hurtTimer / 0.4f));
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 200, 20, 20, a });
    }

    UiDrawHelp();
    UiDrawHealth(&player);
    UiDrawHotbar(&player);
    EntitiesDrawMinimap(&player);   // top-right map + threat readout
    if (IsKeyDown(KEY_G)) EntitiesDrawFullMap(&player);   // the big picture
    UiDrawInventory(&player);   // these draw nothing if their menu is closed
    UiDrawCraftMenu(&player);
    UiDrawTechMenu(&player);
    DebugMenuDraw(&player);     // F3 console — always on top of the rest
}

// ─── Pause screen ────────────────────────────────────────────
// Buttons are re-positioned every frame (the window may have been
// resized) and stacked in a centered column, 70px apart. ONE
// function positions them, called from both update and draw, so the
// clickable area always matches the drawn pixels. It only touches
// rect/text — resetting .hovered here would erase the hover state
// the draw code needs.
static void LayoutPauseButtons(void) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;
    pauseResumeButton.rect   = (Rectangle){ cx - 110, cy - 130, 220, 54 };
    pauseResumeButton.text   = "RESUME";
    pauseSaveButton.rect     = (Rectangle){ cx - 110, cy - 64, 220, 54 };
    pauseSaveButton.text     = "SAVE";
    pauseLoadButton.rect     = (Rectangle){ cx - 110, cy + 2, 220, 54 };
    pauseLoadButton.text     = "LOAD";
    pauseSettingsButton.rect = (Rectangle){ cx - 110, cy + 68, 220, 54 };
    pauseSettingsButton.text = "SETTINGS";
    pauseQuitButton.rect     = (Rectangle){ cx - 110, cy + 134, 220, 54 };
    pauseQuitButton.text     = "QUIT";
}

static void UpdatePause(void) {
    LayoutPauseButtons();

    // UiButtonUpdate returns true when the button was clicked this
    // frame. Escape doubles as "resume".
    if (IsKeyPressed(KEY_ESCAPE) || UiButtonUpdate(&pauseResumeButton)) {
        screen = SCREEN_GAME;
        return;
    }

    if (UiButtonUpdate(&pauseSaveButton)) {
        GameSave();
    }

    if (UiButtonUpdate(&pauseLoadButton)) {
        GameLoad();
        screen = SCREEN_GAME;   // jump straight back into the loaded game
    }

    if (UiButtonUpdate(&pauseSettingsButton)) {
        screen = SCREEN_SETTINGS;
    }

    if (UiButtonUpdate(&pauseQuitButton)) {
        screen = SCREEN_TITLE;
        PlayerCloseMenus(&player);  // don't reopen any menu next game
    }
}

static void DrawPause(void) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;

    // A translucent black overlay (alpha 180 of 255) dims the frozen
    // game behind the menu — a cheap, effective "paused" look.
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 180});
    // MeasureText tells us how wide the string renders, so
    // cx - width/2 centers it horizontally.
    DrawText("PAUSED",
             cx - MeasureText("PAUSED", 48) / 2, cy - 120, 48, WHITE);

    LayoutPauseButtons();
    UiDrawButton(&pauseResumeButton);
    UiDrawButton(&pauseSaveButton);
    UiDrawButton(&pauseLoadButton);
    UiDrawButton(&pauseSettingsButton);
    UiDrawButton(&pauseQuitButton);
}

// ─── Settings screen (controls reference) ────────────────────
static void LayoutSettingsButtons(void) {
    int cx = GetScreenWidth() / 2;
    int sh = GetScreenHeight();
    settingsBackButton.rect = (Rectangle){ cx - 110, sh - 86.0f, 220, 54 };
    settingsBackButton.text = "BACK";
}

static void UpdateSettings(void) {
    LayoutSettingsButtons();
    if (IsKeyPressed(KEY_ESCAPE) || UiButtonUpdate(&settingsBackButton)) {
        screen = SCREEN_PAUSE;
    }
}

static void DrawSettings(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int cx = sw / 2;

    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 180 });
    DrawText("SETTINGS", cx - MeasureText("SETTINGS", 42) / 2, 40, 42, WHITE);
    DrawText("CONTROLS", cx - MeasureText("CONTROLS", 24) / 2, 100, 24, SKYBLUE);

    // Two columns: keys right-aligned toward the middle, actions to
    // the left-aligned right — the classic controls-table layout.
    // Row height squeezes on short windows so nothing runs off.
    int listTop = 140;
    int listBottom = sh - 100;
    int rowH = (listBottom - listTop) / CONTROLS_COUNT;
    if (rowH > 30) rowH = 30;
    int fontSize = (rowH >= 26) ? 18 : 14;
    for (int i = 0; i < CONTROLS_COUNT; i++) {
        int y = listTop + i * rowH;
        DrawText(CONTROLS[i].keys,
                 cx - 40 - MeasureText(CONTROLS[i].keys, fontSize), y, fontSize, GOLD);
        DrawText(CONTROLS[i].action, cx - 16, y, fontSize, LIGHTGRAY);
    }

    LayoutSettingsButtons();
    UiDrawButton(&settingsBackButton);
}

// ─── Title screen ────────────────────────────────────────────
// Same pattern as the pause screen: one layout function shared by
// update and draw.
static void LayoutTitleButtons(void) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;
    titleStartButton.rect    = (Rectangle){ cx - 110, cy + 90, 220, 56 };
    titleStartButton.text    = "NEW GAME";
    titleContinueButton.rect = (Rectangle){ cx - 110, cy + 20, 220, 56 };
    titleContinueButton.text = "CONTINUE";
}

static void UpdateTitle(void) {
    LayoutTitleButtons();

    // Only offer CONTINUE if a save file actually exists on disk.
    bool hasSave = WorldHasSave();

    // Enter/Space = keyboard shortcut for NEW GAME.
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        NewGame();
        screen = SCREEN_GAME;
        return;
    }

    // Escape on the title screen closes the whole program — but only
    // by raising the flag; the main loop sees it and exits cleanly.
    if (IsKeyPressed(KEY_ESCAPE)) {
        quitRequested = true;
        return;
    }

    // CONTINUE: try to load; if the file is corrupt or unreadable,
    // fall back to a new game rather than crashing or doing nothing.
    if (hasSave && UiButtonUpdate(&titleContinueButton)) {
        if (!GameLoad()) NewGame();
        screen = SCREEN_GAME;
        return;
    }

    if (UiButtonUpdate(&titleStartButton)) {
        NewGame();
        screen = SCREEN_GAME;
    }
}

static void DrawTitle(void) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;
    // Big title in a custom blue. C CONCEPT — (Color){...} is a
    // "compound literal": a struct value built in place, no named
    // variable needed. The 4 numbers are Red, Green, Blue, Alpha.
    DrawText("HOME PLANET",
             cx - MeasureText("HOME PLANET", 90)/2, 180, 90,
             (Color){6, 53, 148,255});
    DrawText("VOID RUNNER",
             cx - MeasureText("VOID RUNNER", 24)/2 - 22, cy - 100, 30, GRAY);

    LayoutTitleButtons();
    UiDrawButton(&titleStartButton);

    // CONTINUE is drawn only when there's a save — matching the
    // click logic in UpdateTitle so you can't click an invisible button.
    if (WorldHasSave()) {
        UiDrawButton(&titleContinueButton);
    }
}

// ─── Entry point ─────────────────────────────────────────────
// C CONCEPT — every C program starts at main(). `int main(void)`
// means "takes no arguments, returns an exit code" (0 = success,
// which the operating system receives when the program ends).
int main(void) {
    // One-time setup, in order:
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT); // `|` combines bit-flags
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);   // open the OS window
    SetTargetFPS(TARGET_FPS);                                // cap the frame rate
    SetExitKey(KEY_NULL); // handle Escape ourselves in game logic
                          // (raylib's default is "Escape quits the app")
    WorldMinimapInit();   // needs the window to exist (GPU texture)
    NewGame();            // world + player + entities, in order
    camera.zoom = 1.2f;   // start slightly zoomed in

    // THE game loop — everything above ran once; this runs until
    // the X is clicked (WindowShouldClose) or the title screen's
    // Escape raises quitRequested.
    while (!WindowShouldClose() && !quitRequested) {
        float dt = GetFrameTime();   // seconds since last frame ("delta time")

        // 1) UPDATE — route input to whichever screen is active.
        if      (screen == SCREEN_TITLE)    UpdateTitle();
        else if (screen == SCREEN_PAUSE)    UpdatePause();
        else if (screen == SCREEN_SETTINGS) UpdateSettings();
        else                                UpdateGame(dt);

        // 2) DRAW — raylib requires all drawing between Begin/EndDrawing.
        BeginDrawing();
            ClearBackground(BLACK);  // wipe last frame or you get smearing
            if      (screen == SCREEN_TITLE)    DrawTitle();
            else if (screen == SCREEN_PAUSE)    DrawPause();
            else if (screen == SCREEN_SETTINGS) DrawSettings();
            else                                DrawGame();
            DrawFPS(10, GetScreenHeight() - 25);  // little debug counter
        EndDrawing();  // presents the finished frame and waits for vsync
    }

    CloseWindow();  // clean shutdown: give the window back to the OS
    return 0;       // exit code 0 = "everything went fine"
}