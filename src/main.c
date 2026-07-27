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
    { "W A S D",      "Move (works with menus open)" },
    { "Left click",   "Build (holding a block) / mine / shoot" },
    { "Right click",  "Open block panels; in crafting, craft 5" },
    { "Q",            "Close menus, draw / cycle weapons" },
    { "Z (drag)",     "Feed coal to drills / inserters" },
    { "F (hold)",     "Pull items off nearby belts" },
    { "Drag / Ctrl+click", "Move stacks between any open panels" },
    { "Mouse wheel",  "Cycle hotbar" },
    { "Ctrl + wheel", "Zoom camera" },
    { "1 - 7",        "Select hotbar slot" },
    { "E",            "Backpack" },
    { "TAB",          "Crafting" },
    { "Arrow keys",   "Navigate menus" },
    { "R",            "Reload gun; else rotate belt/arm or ghost" },
    { "G (hold)",     "World map" },
    { "F3",           "Debug console" },
    { "F5 / F9",      "Quick save / quick load" },
    { "ESC",          "Close menu / pause" },
};
#define CONTROLS_COUNT ((int)(sizeof(CONTROLS) / sizeof(CONTROLS[0])))

// (The Projectile pool moved to entities.h — turrets fire the same
// projectiles the player does, so the pool lives with the turrets.)

// Weapon fire-rate state: counts down between shots of whatever
// you're holding (all weapons auto-fire while the button is held).
static float weaponCooldown = 0;
// Last tile the Z-drag fuel sweep touched, as ty*WORLD_SIZE+tx.
// -1 = no drag in progress. This is what makes coal feeding
// one-lump-per-machine instead of one-per-frame.
static int   zFuelLastTile = -1;

// Save files start with a tiny header so we can recognize our own
// files and reject garbage. `char magic[4]` holds the 4 letters
// "HPSV" (Home Planet SaVe) — a common trick called a magic number.
typedef struct {
    char magic[4];
    int  version;   // bump this when the save format changes
} SaveHeader;

#define SAVE_MAGIC "HPSV"
// BUMP THIS whenever a saved struct's SIZE or LAYOUT changes —
// including the machine pool's length. A stale block that still
// passes the version check reads as garbage or fails halfway, and
// the recovery path throws away state that looked fine on disk.
#define SAVE_VERSION 6   // v6: 8192-machine pool, rage, craft queue

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
    // Tearing down a nest by hand enrages everything nearby.
    if (before == TILE_SPAWNER) {
        EnrageMobsAround((Vector2){ (tx + 0.5f) * TILE_SIZE, (ty + 0.5f) * TILE_SIZE });
    }
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

    // Rebuild every derived value (item totals, `selected`, the
    // hotbar mirror) from the slots, and scrub any 0x ghosts the
    // file may carry. Derived state is never loaded, only recomputed
    // — that's what stops a hand-edited save from inventing items.
    PlayerRecount(p);

    // Survival + research state back into legal ranges.
    if (p->hp <= 0 || p->hp > PLAYER_MAX_HP) p->hp = PLAYER_MAX_HP;
    p->hurtTimer = 0;
    p->invulnTimer = 0;
    p->regenDelay = 0;
    p->placeDir &= 3;

    // Magazines can't exceed their weapon's capacity, and a reload
    // never survives a load.
    for (int i = 0; i < ITEM_COUNT; i++) {
        int cap = ItemMagSize((ItemID)i);
        if (p->mag[i] < 0)   p->mag[i] = 0;
        if (p->mag[i] > cap) p->mag[i] = cap;
    }
    p->reloadTimer = 0;
    p->reloadTotal = 0;
    p->reloadingItem = ITEM_NONE;
    machineUiX = machineUiY = -1;   // no panel open across a load

    // A craft queue from disk is untrusted: clamp its length and
    // drop any entry that isn't a real recipe.
    if (p->craftQueueCount < 0 || p->craftQueueCount > CRAFT_QUEUE_MAX) p->craftQueueCount = 0;
    int keep = 0;
    for (int i = 0; i < p->craftQueueCount; i++) {
        ItemID q = p->craftQueue[i];
        if (q > ITEM_NONE && q < ITEM_COUNT && ITEMS[q].inA != ITEM_NONE) {
            p->craftQueue[keep++] = q;
        }
    }
    p->craftQueueCount = keep;
    for (int i = keep; i < CRAFT_QUEUE_MAX; i++) p->craftQueue[i] = ITEM_NONE;
    if (p->craftProgress < 0) p->craftProgress = 0;
    for (int i = 0; i < MAX_TOASTS; i++) playerToasts[i].active = false;
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

// ─── One slot, anywhere ──────────────────────────────────────
// A SlotRef points at a stack wherever it lives — the backpack, the
// hotbar, a chest, a drill. Transfers are written ONCE against this
// abstraction, so every panel-to-panel move behaves identically
// instead of each pairing growing its own special case.
typedef struct { ItemID *id; int *count; } SlotRef;

static SlotRef PlayerSlotRef(Player *p, int i) {
    return (SlotRef){ &p->inventorySlots[i], &p->inventoryAmounts[i] };
}
static SlotRef MachineSlotRef(Machine *m, int i) {
    return (SlotRef){ &m->slots[i], &m->counts[i] };
}

// Move `from` onto `to`. Same item → merge up to STACK_MAX;
// anything else (including an empty target) → swap. Dropping on a
// specific slot therefore lands in THAT slot, never "the first free
// one somewhere else".
static void SlotTransfer(SlotRef from, SlotRef to) {
    if (*from.id == ITEM_NONE || *from.count <= 0) return;
    if (*to.id == *from.id && *to.count > 0) {
        int room = STACK_MAX - *to.count;
        if (room <= 0) return;                     // target stack is full
        int move = (*from.count < room) ? *from.count : room;
        *to.count   += move;
        *from.count -= move;
        if (*from.count <= 0) { *from.id = ITEM_NONE; *from.count = 0; }
    } else {
        ItemID ti = *to.id;  int tc = *to.count;
        *to.id   = *from.id; *to.count   = *from.count;
        *from.id = ti;       *from.count = tc;
    }
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

    ItemID held = player.selected;
    Vector2 muzzle = Vector2Add(player.pos,
        Vector2Scale(Vector2Normalize(Vector2Subtract(mouse, player.pos)), PLAYER_RADIUS + 8));
    TileType hoverTile = world[tx][ty].type;
    bool inReach = Vector2Distance(player.pos, mouse) <= TUNE.playerReach;

    // ── Machine access comes FIRST, weapon or no weapon ──────
    // Opening a chest or a drill shouldn't require holstering your
    // gun, so these run before the weapon block.
    if (inReach && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if (hoverTile == TILE_RESEARCH) {
            PlayerToggleTechMenu(&player);
            return;
        }
        // Chest, drill, inserter, belt → open its panel.
        if (hoverTile == TILE_CHEST || TileNeedsFuel(hoverTile) || TileIsBelt(hoverTile)) {
            if (MachineAt(tx, ty) != NULL) {
                if (machineUiX == tx && machineUiY == ty) {
                    machineUiX = machineUiY = -1;    // same tile → close
                } else {
                    machineUiX = tx; machineUiY = ty;
                    player.techMenuOpen = false;
                }
                return;
            }
        }
    }

    // ── Z: drag-feed coal, Factorio style ────────────────────
    // Hold Z with coal in hand and sweep the cursor over machines:
    // each NEW machine tile the cursor touches takes exactly one
    // lump. `zFuelLastTile` is what makes it one-per-touch instead
    // of one-per-frame.
    if (IsKeyDown(KEY_Z) && player.inventory[ITEM_COAL] > 0 && inReach) {
        int tile = ty * WORLD_SIZE + tx;
        if (tile != zFuelLastTile && TileNeedsFuel(hoverTile)) {
            Machine *m = MachineAt(tx, ty);
            if (m != NULL && MachineAddCoal(m)) {
                PlayerRemoveItem(&player, ITEM_COAL, 1);
                zFuelLastTile = tile;
            }
        } else if (tile != zFuelLastTile && !TileNeedsFuel(hoverTile)) {
            zFuelLastTile = tile;   // passing over scenery re-arms the drag
        }
    } else if (!IsKeyDown(KEY_Z)) {
        zFuelLastTile = -1;
    }

    // ── Weapons: fire toward the cursor, no reach gate ────────
    // Where you CLICK is a direction, not a destination. (The old
    // code ran the reach gate first, so clicking past your reach
    // silently ate the shot — the "nothing fires" bug.)
    if (ItemIsWeapon(held)) {
        // EVERY weapon auto-fires while held. Clicking fires at the
        // weapon's full rate; holding runs WEAPON_HOLD_PENALTY times
        // slower, so click-spam stays marginally better than leaning
        // on the button.
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool holding = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        int magSize = ItemMagSize(held);

        if ((clicked || holding) && weaponCooldown <= 0 && player.reloadTimer <= 0) {
            float interval = ItemFireInterval(held);
            if (!clicked) interval *= WEAPON_HOLD_PENALTY;

            // Magazine guns burn a loaded round; the slingshot feeds
            // straight from your stone pile.
            int shotCost = (held == ITEM_SHOTGUN) ? 2 : 1;
            bool haveShot = (magSize > 0) ? (player.mag[held] >= shotCost)
                                          : (player.inventory[ITEM_SMALL_STONE] > 0);

            if (!haveShot) {
                PlayerStartReload(&player, held);   // dry → rack a fresh mag
            } else {
                if (magSize > 0) player.mag[held] -= shotCost;

                if (held == ITEM_SLINGSHOT) {
                    PlayerRemoveItem(&player, ITEM_SMALL_STONE, 1);   // ammo consumed...
                    SpawnProjectile(player.pos, mouse, ITEM_SMALL_STONE, false);  // ...and flies
                    AddEffect(EFFECT_FLASH, muzzle, 5, 0.05f);
                } else if (held == ITEM_PISTOL) {
                    SpawnProjectile(player.pos, mouse, ITEM_BULLET, false);
                    AddEffect(EFFECT_FLASH, muzzle, 7, 0.06f);
                    entShake += 0.8f;
                } else if (held == ITEM_SMG) {
                    // A pinch of recoil spread so it feels like an SMG.
                    Vector2 spread = Vector2Rotate(Vector2Subtract(mouse, player.pos),
                                                   GetRandomValue(-40, 40) / 1000.0f);
                    SpawnProjectile(player.pos, Vector2Add(player.pos, spread), ITEM_BULLET, false);
                    AddEffect(EFFECT_FLASH, muzzle, 6, 0.05f);
                    entShake += 0.35f;
                } else if (held == ITEM_SHOTGUN) {
                    for (int pellet = 0; pellet < SHOTGUN_PELLETS; pellet++) {
                        Vector2 spread = Vector2Rotate(Vector2Subtract(mouse, player.pos),
                                                       GetRandomValue(-140, 140) / 1000.0f);
                        SpawnProjectile(player.pos, Vector2Add(player.pos, spread), ITEM_BULLET, false);
                    }
                    AddEffect(EFFECT_FLASH, muzzle, 10, 0.08f);
                    entShake += 2.6f;
                }
                weaponCooldown = interval;
                // Ran the mag dry with that shot → start reloading now.
                if (magSize > 0 && player.mag[held] < shotCost) {
                    PlayerStartReload(&player, held);
                }
            }
        }
        return;   // holding a weapon: no mining/placing with this click
    }

    // ── Everything below (mine, place) needs REACH ────────────
    if (!inReach) return;

    // ── RMB interactions (Pressed = deliberate single actions) ──
    TileType targetTile = hoverTile;
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
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

    // LMB (held) with a PLACEABLE in hand = build; otherwise = mine.
    // Holding a belt means you're building a belt, so left-click
    // lays track instead of digging. Put the belt away (empty hand
    // or a tool) and the same button mines again.
    bool holdingPlaceable = (held != ITEM_NONE && ITEMS[held].placeable &&
                             player.inventory[held] > 0);

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !holdingPlaceable) {
        TileType before = world[tx][ty].type;   // capture BEFORE it breaks
        if (WorldDamageTile(tx, ty, PlayerMiningDPS(&player) * dt)) {
            GiveTileBreakDrops(tx, ty, before);
        }
    }

    // Build on LMB (held, so you can drag out a whole belt line).
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && holdingPlaceable) {
        const ItemInfo *it = &ITEMS[player.selected];
        // Never build on the tile you're standing in — that used to
        // seal you inside your own wall. (PlayerUnstick would dig
        // you out, but refusing the placement is the honest fix.)
        Rectangle targetTile = { (float)(tx * TILE_SIZE), (float)(ty * TILE_SIZE),
                                 (float)TILE_SIZE, (float)TILE_SIZE };
        bool standingHere = CheckCollisionCircleRec(player.pos, PLAYER_RADIUS + 1.0f, targetTile);

        if (it->placeable &&                          // is this item placeable at all?
            player.selected != ITEM_NONE &&           // holding something?
            player.inventory[player.selected] > 0 &&  // actually own one?
            !standingHere &&                          // not under your own feet
            world[tx][ty].type == TILE_GRASS) {       // only build on open ground
            // Machines need their per-instance record (a Machine) —
            // belts/inserters/drills take the current R-rotation
            // facing. Claim the record FIRST: if the pool is full the
            // build is refused outright, because a machine tile with
            // no record is a dead stub that merely looks placeable.
            if (TileIsMachine(it->places)) {
                if (AddMachineAt(tx, ty, it->places, player.placeDir) == NULL) return;
            }
            WorldSetTile(tx, ty, it->places);         // grass → the item's tile
            PlayerRemoveSelectedItem(&player, 1);     // consume one from inventory
        }
    }
}

// ─── Placement ghost ─────────────────────────────────────────
// While you HOLD a placeable item, the target tile shows a
// translucent preview of what you're about to build, with its
// facing arrow — so belts and inserters show which way they'll run
// BEFORE you commit. Click (RMB) is what actually places it.
static void DrawPlacementGhost(void) {
    ItemID held = player.selected;
    if (held == ITEM_NONE || !ITEMS[held].placeable) return;
    if (player.inventory[held] <= 0) return;

    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    int tx = (int)(mouse.x / TILE_SIZE), ty = (int)(mouse.y / TILE_SIZE);
    if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) return;

    float px = (float)(tx * TILE_SIZE), py = (float)(ty * TILE_SIZE);
    Rectangle tile = { px, py, (float)TILE_SIZE, (float)TILE_SIZE };
    bool freeGround = (world[tx][ty].type == TILE_GRASS);
    bool onSelf = CheckCollisionCircleRec(player.pos, PLAYER_RADIUS + 1.0f, tile);
    bool reachable = Vector2Distance(player.pos, mouse) <= TUNE.playerReach;
    bool ok = freeGround && !onSelf && reachable;

    // Green = this click will build; red = it won't.
    Color tint = ok ? (Color){ 120, 255, 140, 70 } : (Color){ 255, 90, 90, 70 };
    DrawRectangleRec(tile, tint);
    DrawRectangleLinesEx(tile, 2, ok ? (Color){ 150, 255, 170, 220 }
                                     : (Color){ 255, 120, 120, 220 });
    DrawItemSprite(held, px + 3, py + 3, TILE_SIZE - 6);

    // Facing arrow for directional builds.
    if (TileIsDirectional(ITEMS[held].places)) {
        int d = player.placeDir;
        Vector2 c = { px + TILE_SIZE / 2.0f, py + TILE_SIZE / 2.0f };
        Vector2 tip  = { c.x + DIR_DX[d] * 12.0f, c.y + DIR_DY[d] * 12.0f };
        Vector2 left = { c.x - DIR_DY[d] * 6.0f,  c.y + DIR_DX[d] * 6.0f };
        Vector2 rght = { c.x + DIR_DY[d] * 6.0f,  c.y - DIR_DX[d] * 6.0f };
        DrawTriangle(tip, left, rght, RAYWHITE);
        DrawTriangle(tip, rght, left, RAYWHITE);
    }
}

// ─── One frame of gameplay ───────────────────────────────────
// DESIGN CHANGE — menus no longer pause the world. Mobs keep
// marching while you dig through your backpack (rust-like: the
// world doesn't wait for you). Each open menu handles ITS input;
// the simulation at the bottom runs every frame regardless. Menus
// claim the MOUSE only — they navigate with the arrow keys, so
// WASD keeps walking you around with panels open.
static void UpdateGame(float dt) {
    // Toggle keys. Backpack and crafting can be open TOGETHER (they
    // sit side by side); the tech terminal and the debug console
    // still take the stage alone.
    if (IsKeyPressed(KEY_F3)) {
        debugMenuOpen = !debugMenuOpen;
        if (debugMenuOpen) PlayerCloseMenus(&player);
    }
    if (IsKeyPressed(KEY_E))   { debugMenuOpen = false; PlayerToggleInventory(&player); }
    if (IsKeyPressed(KEY_TAB)) { debugMenuOpen = false; PlayerToggleCraftMenu(&player); }

    // Q — the "back to the fight" key: slams every menu shut and
    // draws your best weapon, or cycles to the next one if a gun is
    // already in hand.
    if (IsKeyPressed(KEY_Q)) {
        debugMenuOpen = false;
        PlayerQuickWeapon(&player);
    }

    // Escape: close whatever is open; with nothing open, pause.
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (debugMenuOpen) {
            debugMenuOpen = false;
        } else if (machineUiX >= 0) {
            machineUiX = machineUiY = -1;
        } else if (player.inventoryOpen || player.craftMenuOpen || player.techMenuOpen) {
            PlayerCloseMenus(&player);
        } else {
            screen = SCREEN_PAUSE;
            return;
        }
    }

    // Number keys 1..N select hotbar slots. KEY_ONE + k works
    // because raylib key codes are consecutive: KEY_ONE+1 == KEY_TWO.
    for (int k = 0; k < HOTBAR_MAX_SLOTS; k++)
        if (IsKeyPressed(KEY_ONE + k)) PlayerSelectSlot(&player, k);

    // ── Drag & drop across EVERY slot surface ────────────────
    // Backpack grid, the detached hotbar row, the HUD hotbar and any
    // open machine panel all speak the same drag protocol: pick a
    // stack up on press, drop it wherever you release. Each surface
    // is hit-tested through the same rect function the drawing uses,
    // so click targets can never drift from pixels.
    {
        Vector2 mouse = GetMousePosition();   // menus live in SCREEN space
        bool panelsOpen = player.inventoryOpen || machineUiX >= 0;

        InventoryLayout invLayout = { 0 };
        bool haveInv = player.inventoryOpen;
        if (haveInv) UiGetInventoryLayout(&player, &invLayout);

        Machine *panelM = (machineUiX >= 0) ? MachineAt(machineUiX, machineUiY) : NULL;
        MachinePanelLayout machLayout = { 0 };
        if (panelM != NULL) UiGetMachinePanelLayout(&player, panelM, &machLayout);

        // Which slot, on which surface, is under the cursor?
        int hoverPlayer = -1, hoverMachine = -1;
        if (haveInv) {
            for (int idx = 0; idx < INVENTORY_SIZE; idx++) {
                if (CheckCollisionPointRec(mouse, UiInventorySlotRect(&invLayout, idx))) {
                    hoverPlayer = idx;
                    player.inventoryCursor = idx;
                    break;
                }
            }
        }
        if (hoverPlayer < 0 && panelsOpen) {
            // The HUD hotbar counts as player slots 0..6 — that's
            // what makes dragging to/from the bar work.
            int hb = UiHotbarSlotAt(&player, mouse);
            if (hb >= 0) hoverPlayer = hb;
        }
        if (panelM != NULL) {
            for (int i = 0; i < machLayout.slotCount &&
                            i < machLayout.cols * machLayout.rows; i++) {
                if (CheckCollisionPointRec(mouse, UiMachineSlotRect(&machLayout, i))) {
                    hoverMachine = i;
                    break;
                }
            }
        }

        // CTRL+CLICK — instant transfer, no dragging. A slot jumps to
        // "the other side": your stack into the open machine, a
        // machine stack into your pockets, and with no machine open,
        // between the hotbar row and storage.
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (panelsOpen && ctrl && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (hoverMachine >= 0 && panelM != NULL &&
                panelM->slots[hoverMachine] != ITEM_NONE && panelM->counts[hoverMachine] > 0) {
                PlayerGiveItem(&player, panelM->slots[hoverMachine], panelM->counts[hoverMachine]);
                panelM->slots[hoverMachine] = ITEM_NONE;
                panelM->counts[hoverMachine] = 0;
                return;
            }
            if (hoverPlayer >= 0 && player.inventorySlots[hoverPlayer] != ITEM_NONE &&
                player.inventoryAmounts[hoverPlayer] > 0) {
                if (panelM != NULL) {
                    int put = MachineAddItem(panelM, player.inventorySlots[hoverPlayer],
                                             player.inventoryAmounts[hoverPlayer]);
                    if (put > 0) {
                        player.inventoryAmounts[hoverPlayer] -= put;
                        if (player.inventoryAmounts[hoverPlayer] <= 0) {
                            player.inventorySlots[hoverPlayer] = ITEM_NONE;
                            player.inventoryAmounts[hoverPlayer] = 0;
                        }
                        PlayerRecount(&player);
                    }
                } else {
                    // No machine open → hop between hotbar and storage.
                    bool fromHotbar = (hoverPlayer < HOTBAR_MAX_SLOTS);
                    int lo = fromHotbar ? HOTBAR_MAX_SLOTS : 0;
                    int hi = fromHotbar ? INVENTORY_SIZE : HOTBAR_MAX_SLOTS;
                    for (int i = lo; i < hi; i++) {
                        if (player.inventorySlots[i] == ITEM_NONE ||
                            player.inventoryAmounts[i] <= 0) {
                            PlayerInventorySwapSlots(&player, hoverPlayer, i);
                            break;
                        }
                    }
                }
                return;
            }
        }

        // PRESS — pick up whatever is under the cursor.
        if (panelsOpen && !ctrl && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (hoverMachine >= 0 && panelM != NULL &&
                panelM->slots[hoverMachine] != ITEM_NONE && panelM->counts[hoverMachine] > 0) {
                uiDragKind  = DRAG_MACHINE;
                uiDragIndex = hoverMachine;
                uiDragItem  = panelM->slots[hoverMachine];
                uiDragCount = panelM->counts[hoverMachine];
            } else if (hoverPlayer >= 0 &&
                       player.inventorySlots[hoverPlayer] != ITEM_NONE &&
                       player.inventoryAmounts[hoverPlayer] > 0) {
                uiDragKind  = DRAG_PLAYER;
                uiDragIndex = hoverPlayer;
                uiDragItem  = player.inventorySlots[hoverPlayer];
                uiDragCount = player.inventoryAmounts[hoverPlayer];
                player.inventoryDragging = true;          // ui.h ghost
                player.inventoryDragIndex = hoverPlayer;
            }
        }

        // RELEASE — resolve the drop. Every combination goes through
        // the same SlotTransfer, so backpack→chest, chest→hotbar and
        // chest→chest all behave the same way and land exactly where
        // you aimed.
        if (uiDragKind != DRAG_NONE && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            bool srcPlayer = (uiDragKind == DRAG_PLAYER);
            bool haveSrc = srcPlayer ? (uiDragIndex >= 0 && uiDragIndex < INVENTORY_SIZE)
                                     : (panelM != NULL && uiDragIndex >= 0);
            if (haveSrc) {
                SlotRef from = srcPlayer ? PlayerSlotRef(&player, uiDragIndex)
                                         : MachineSlotRef(panelM, uiDragIndex);
                if (hoverPlayer >= 0) {
                    if (!(srcPlayer && hoverPlayer == uiDragIndex)) {
                        SlotTransfer(from, PlayerSlotRef(&player, hoverPlayer));
                    } else if (uiDragIndex < HOTBAR_MAX_SLOTS) {
                        PlayerSelectSlot(&player, uiDragIndex);   // dropped where it started = equip
                    }
                } else if (hoverMachine >= 0 && panelM != NULL) {
                    if (!(!srcPlayer && hoverMachine == uiDragIndex)) {
                        SlotTransfer(from, MachineSlotRef(panelM, hoverMachine));
                    }
                }
                PlayerRecount(&player);   // totals are always derived
            }
            UiDragClear();
            player.inventoryDragging = false;
            player.inventoryDragIndex = -1;
        }
    }

    // ── Craft menu ───────────────────────────────────────────
    if (player.craftMenuOpen) {
        CraftLayout craftLayout = { 0 };
        UiGetCraftLayout(&player, &craftLayout);
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

            // 2D navigation with the ARROW keys only — WASD stays
            // yours for walking while the menu is open. Clamped,
            // not wrapped: a grid that wraps in both axes is a maze.
            int move = 0;
            if (IsKeyPressed(KEY_RIGHT)) move = 1;
            if (IsKeyPressed(KEY_LEFT))  move = -1;
            if (IsKeyPressed(KEY_DOWN))  move = cols;
            if (IsKeyPressed(KEY_UP))    move = -cols;
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


        
        // Mouse: hover (when actually moving) selects a cell; left
        // click crafts ONE, right click crafts a batch of FIVE.
        // Cell rects come from ui.h (UiCraftCellRect) — the same
        // function the drawing uses, so click targets match pixels.
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
                if (cellButton.hovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    for (int batch = 0; batch < 5; batch++) {
                        if (!PlayerCraft(&player, CraftableAtRow(idx))) break;
                    }
                }
            }
        }
    }

    // ── Tech tree (RMB a Research Computer to open) ──────────
    if (player.techMenuOpen) {
        TechLayout techLayout = { 0 };
        UiGetTechLayout(&techLayout);
        int total = TECH_COUNT - 1;

        // Dismissal: the header [X], or EITHER mouse button clicked
        // anywhere outside the panel. (E and TAB also close it and
        // open their own menu — see PlayerToggleInventory/CraftMenu.)
        Rectangle techPanel = { (float)techLayout.x, (float)techLayout.y,
                                (float)techLayout.w, (float)techLayout.h };
        bool clickedAny = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                          IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        if (clickedAny) {
            Vector2 mp = GetMousePosition();
            bool onClose = CheckCollisionPointRec(mp,
                UiPanelCloseRect(techLayout.x, techLayout.y, techLayout.w));
            if (onClose || !CheckCollisionPointRec(mp, techPanel)) {
                player.techMenuOpen = false;
                return;   // that click was "close", not "research"
            }
        }

        // Same navigation scheme as the craft menu: wheel scrolls
        // the window, W/S move the selection, hover follows the
        // mouse only when it moves.
        float wheel = GetMouseWheelMove();
        if (wheel > 0) player.techScroll--;
        if (wheel < 0) player.techScroll++;
        bool moveDown = IsKeyPressed(KEY_DOWN);   // arrows only — WASD walks
        bool moveUp   = IsKeyPressed(KEY_UP);
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
    }

    // Craft-queue [x] buttons: cancel and refund that entry. Checked
    // before world clicks so the button always wins.
    if (player.craftQueueCount > 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int shown = player.craftQueueCount < UI_QUEUE_MAX_SHOWN
                  ? player.craftQueueCount : UI_QUEUE_MAX_SHOWN;
        for (int i = 0; i < shown; i++) {
            if (CheckCollisionPointRec(GetMousePosition(), UiCraftQueueCancelRect(i))) {
                PlayerCancelCraftAt(&player, i);
                return;
            }
        }
    }

    // Walk away and the block's panel shuts itself — you can't
    // manage a chest from across the base.
    if (machineUiX >= 0 && machineUiY >= 0) {
        Vector2 machinePos = { (machineUiX + 0.5f) * TILE_SIZE,
                               (machineUiY + 0.5f) * TILE_SIZE };
        if (Vector2Distance(player.pos, machinePos) > 7.0f * TILE_SIZE) {
            machineUiX = machineUiY = -1;
        }
    }

    // ── Machine panel (chest / drill / inserter / belt) ───────
    // Click a slot to move a stack: full slot + empty hand takes it
    // out, held item + slot puts it in. Clicking the coal slot with
    // coal in hand tops the hopper right up.
    if (machineUiX >= 0 && machineUiY >= 0) {
        Machine *m = MachineAt(machineUiX, machineUiY);
        // Right-click closes the panel again (the same button that
        // opened it), since world interaction is suppressed while
        // it's up. Escape works too.
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            machineUiX = machineUiY = -1;
            m = NULL;
        }
        if (m == NULL) {
            machineUiX = machineUiY = -1;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            MachinePanelLayout ml = { 0 };
            UiGetMachinePanelLayout(&player, m, &ml);
            Vector2 mp = GetMousePosition();
            ItemID held = player.selected;

            // The header [X] closes the panel.
            if (CheckCollisionPointRec(mp, UiPanelCloseRect(ml.x, ml.y, ml.w))) {
                machineUiX = machineUiY = -1;
                m = NULL;
            }
            // (Slot-to-slot moves belong to the shared drag system
            // above; this block only owns the coal slot.)
            if (m != NULL && ml.hasFuel && CheckCollisionPointRec(mp, ml.fuelRect)) {
                if (held == ITEM_COAL) {
                    while (player.inventory[ITEM_COAL] > 0 && MachineAddCoal(m)) {
                        PlayerRemoveItem(&player, ITEM_COAL, 1);
                    }
                } else if (m->coal > 0) {
                    PlayerGiveItem(&player, ITEM_COAL, m->coal);
                    m->coal = 0;
                }
            }
        }
    }

    // ── Gameplay input (only when no menu wants the mouse) ───
    // Menus take the MOUSE, never the keyboard: they navigate with
    // the arrow keys, leaving WASD free so you can keep walking
    // while you craft or shuffle your backpack.
    bool anyMenu = player.inventoryOpen || player.craftMenuOpen ||
                   player.techMenuOpen || debugMenuOpen || machineUiX >= 0;

    if (!anyMenu) {
        // R means three things, resolved by what's under your hand:
        // holding a gun → RELOAD; hovering a placed belt/arm →
        // rotate it in place; otherwise → spin the build ghost.
        if (IsKeyPressed(KEY_R)) {
            Vector2 mw = GetScreenToWorld2D(GetMousePosition(), camera);
            int htx = (int)(mw.x / TILE_SIZE), hty = (int)(mw.y / TILE_SIZE);
            Machine *hovered = NULL;
            if (htx >= 0 && htx < WORLD_SIZE && hty >= 0 && hty < WORLD_SIZE &&
                TileIsDirectional(world[htx][hty].type)) {
                hovered = MachineAt(htx, hty);
            }
            if (ItemIsWeapon(player.selected)) {
                PlayerStartReload(&player, player.selected);
            } else if (hovered != NULL) {
                hovered->dir = (hovered->dir + 1) & 3;
            } else {
                player.placeDir = (player.placeDir + 1) & 3;
            }
        }

        // Clicking the on-screen hotbar selects that slot. The
        // hit-test (UiHotbarSlotAt) shares its layout math with
        // UiDrawHotbar, so click targets always match pixels.
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int clickedSlot = UiHotbarSlotAt(&player, GetMousePosition());
            if (clickedSlot >= 0) PlayerSelectSlot(&player, clickedSlot);
        }

        // Mouse wheel: Ctrl+wheel zooms the camera, plain wheel
        // cycles the hotbar. Kept as a float: trackpads report
        // fractional scrolls that an int would truncate to 0.
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

        // Quick save/load hotkeys.
        if (IsKeyPressed(KEY_F5)) GameSave();
        if (IsKeyPressed(KEY_F9)) GameLoad();

        // Holding G shows the world map — mouse clicks belong to
        // the map then, not to mining/shooting under the overlay.
        if (!IsKeyDown(KEY_G)) UpdateMiningAndPlacing(dt);
    }

    // ── The simulation ALWAYS runs — menus don't pause the world.
    if (weaponCooldown > 0) weaponCooldown -= dt;
    PlayerMove(&player, dt);   // walk even with menus open

    // Belts drag whatever stands on them, you included. Applied
    // after PlayerMove and re-checked for walkability so a belt
    // can never shove you inside a wall.
    Vector2 carry = BeltCarry(player.pos, dt);
    if (carry.x != 0.0f || carry.y != 0.0f) {
        Vector2 rideX = { player.pos.x + carry.x, player.pos.y };
        Vector2 rideY = { player.pos.x, player.pos.y + carry.y };
        if (WorldPositionWalkable(rideX)) player.pos.x = rideX.x;
        if (WorldPositionWalkable(rideY)) player.pos.y = rideY.y;
    }

    // Hold F to scoop cargo off nearby belts.
    if (IsKeyDown(KEY_F)) BeltPickupNear(&player, TUNE.playerReach);

    PlayerUpdateVitals(&player, dt);
    PlayerUpdateReload(&player, dt);
    PlayerUpdateCrafting(&player, dt);
    PlayerUpdateToasts(dt);
    WorldRevealAround(player.pos, FOW_REVEAL_TILES);   // push back the fog
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
        // Sun position for this frame — shadows swing as play time
        // passes (entGameTime pauses with the game, so does the sun).
        WorldSetClock(entGameTime);
        // Both world and entity layers cull to the visible slice —
        // the map is 147k tiles, the screen shows a few hundred.
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
        DrawPlacementGhost();   // preview of what RMB will build
        PlayerDrawToasts(player.pos);   // "+12 Stone" floating up
    EndMode2D();

    // Getting hit flashes a red vignette (screen space from here on).
    if (player.hurtTimer > 0) {
        unsigned char a = (unsigned char)(120 * (player.hurtTimer / 0.4f));
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 200, 20, 20, a });
    }

    UiDrawHealth(&player);
    UiDrawCraftQueue(&player);      // what's building right now
    UiDrawAmmo(&player);            // ammo count / circular reload gauge
    UiDrawHotbar(&player);
    EntitiesDrawMinimap(&player);   // top-right map + threat readout
    if (IsKeyDown(KEY_G)) EntitiesDrawFullMap(&player);   // the big picture

    // No dimming: the world stays fully lit and readable behind the
    // panels, because it's still running and you're still in it.
    UiDrawInventory(&player);   // these draw nothing if their menu is closed
    UiDrawCraftMenu(&player);
    UiDrawMachinePanel(&player);   // chest / drill / inserter / belt
    UiDrawTechMenu(&player);
    UiDrawDragGhost();          // the stack riding the cursor
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
             cx - MeasureText("PAUSED", 48) / 2, cy - 250, 48, WHITE);

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