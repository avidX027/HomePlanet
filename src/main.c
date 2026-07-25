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
//
//  C CONCEPT — the frame loop: a game is just
//     while (window open) { read input; update state; draw; }
//  repeated ~60x/second. dt ("delta time") is how many seconds
//  the last frame took; multiply speeds by it and the game runs
//  the same on a slow laptop and a 144Hz desktop.
// ============================================================

#include "raylib.h"
#include "config.h"
#include "gamedata.h"
#include "world.h"
#include "player.h"
#include "ui.h"

// C CONCEPT — file-scope state: these live for the whole program.
// Kept minimal on purpose: the player, a camera, one screen flag.
typedef enum { SCREEN_TITLE, SCREEN_GAME, SCREEN_PAUSE } Screen;

static Screen   screen = SCREEN_TITLE;
static Player   player;
static Camera2D camera = { 0 };
static UIButton titleStartButton = { 0 };
static UIButton pauseResumeButton = { 0 };
static UIButton pauseQuitButton = { 0 };

// ─── Count how many recipes exist (for craft-menu wrapping) ──
static int CraftableCount(void) {
    int n = 0;
    for (ItemID id = 1; id < ITEM_COUNT; id++)
        if (ITEMS[id].inA != ITEM_NONE) n++;
    return n;
}

// ─── Map craft-menu row number back to an ItemID ─────────────
static ItemID CraftableAtRow(int row) {
    int n = 0;
    for (ItemID id = 1; id < ITEM_COUNT; id++) {
        if (ITEMS[id].inA == ITEM_NONE) continue;
        if (n == row) return id;
        n++;
    }
    return ITEM_NONE;
}

// ─── Mining & placing: where player rules meet world rules ───
static void UpdateMiningAndPlacing(float dt) {
    // Convert the mouse from SCREEN pixels to WORLD pixels.
    // (The camera follows the player, so these differ.)
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    int tx = (int)(mouse.x / TILE_SIZE);       // which tile is that?
    int ty = (int)(mouse.y / TILE_SIZE);
    // Check placement bounds
    if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) return;
    if (Vector2Distance(player.pos, mouse) > PLAYER_REACH) return;

    // LMB (held): damage the tile; collect drops if it broke.
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        TileType before = world[tx][ty].type;
        if (WorldDamageTile(tx, ty, PlayerMiningDPS(&player) * dt)) {
            PlayerGiveItem(&player, TILES[before].drops, TILES[before].dropCount);
        }
    }

    // RMB (click): place the selected item, if it's placeable,
    // owned, and the target tile is grass.
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        const ItemInfo *it = &ITEMS[player.selected];
        if (it->placeable &&
            player.selected != ITEM_NONE &&
            player.inventory[player.selected] > 0 &&
            world[tx][ty].type == TILE_GRASS) {
            WorldSetTile(tx, ty, it->places);
            PlayerRemoveSelectedItem(&player, 1);
        }
    }
}

// ─── One frame of gameplay ───────────────────────────────────
static void UpdateGame(float dt) {
    if (IsKeyPressed(KEY_E)) PlayerToggleInventory(&player);
    if (IsKeyPressed(KEY_TAB)) player.craftMenuOpen = !player.craftMenuOpen;

    for (int k = 0; k < HOTBAR_MAX_SLOTS; k++)
        if (IsKeyPressed(KEY_ONE + k)) PlayerSelectSlot(&player, k);

    if (player.inventoryOpen) {
        InventoryLayout layout = { 0 };
        UiGetInventoryLayout(&player, &layout);

        Vector2 mouse = GetMousePosition();
        int hoveredIndex = -1;
        for (int r = 0; r < INVENTORY_ROWS; r++) {
            for (int c = 0; c < INVENTORY_COLS; c++) {
                int idx = r * INVENTORY_COLS + c;
                int x = layout.startX + c * (layout.slotSize + layout.gap);
                int y = layout.startY + r * (layout.slotSize + layout.gap);
                Rectangle rect = { (float)x, (float)y, (float)layout.slotSize, (float)layout.slotSize };
                if (CheckCollisionPointRec(mouse, rect)) {
                    player.inventoryCursor = idx;
                    hoveredIndex = idx;
                }
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (hoveredIndex >= 0 && hoveredIndex < INVENTORY_SIZE) {
                ItemID id = player.inventorySlots[hoveredIndex];
                if (id != ITEM_NONE && player.inventoryAmounts[hoveredIndex] > 0) {
                    player.inventoryDragging = true;
                    player.inventoryDragIndex = hoveredIndex;
                    player.inventoryCursor = hoveredIndex;
                } else {
                    player.inventoryCursor = hoveredIndex;
                }
            }
        }

        if (player.inventoryDragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (hoveredIndex >= 0 && hoveredIndex < INVENTORY_SIZE) {
                if (hoveredIndex != player.inventoryDragIndex) {
                    PlayerInventorySwapSlots(&player, player.inventoryDragIndex, hoveredIndex);
                } else {
                    ItemID id = player.inventorySlots[player.inventoryDragIndex];
                    if (id != ITEM_NONE) {
                        PlayerSelectSlot(&player, player.inventoryDragIndex < HOTBAR_MAX_SLOTS ? player.inventoryDragIndex : player.selectedSlot);
                        if (player.inventoryDragIndex < HOTBAR_MAX_SLOTS) {
                            player.selected = id;
                        }
                    }
                }
            } else {
                ItemID id = player.inventorySlots[player.inventoryDragIndex];
                if (id != ITEM_NONE) {
                    PlayerSelectSlot(&player, player.inventoryDragIndex < HOTBAR_MAX_SLOTS ? player.inventoryDragIndex : player.selectedSlot);
                    if (player.inventoryDragIndex < HOTBAR_MAX_SLOTS) {
                        player.selected = id;
                    }
                }
            }
            player.inventoryDragging = false;
            player.inventoryDragIndex = -1;
        }
        return;
    }

    if (player.craftMenuOpen) {
        // Menu open: navigation only; the world is paused.
        int n = CraftableCount();
        if (IsKeyPressed(KEY_DOWN)) player.craftSel = (player.craftSel + 1) % n;
        if (IsKeyPressed(KEY_UP))   player.craftSel = (player.craftSel + n - 1) % n;
        if (IsKeyPressed(KEY_ENTER)) PlayerCraft(&player, CraftableAtRow(player.craftSel));
        // Mouse support: hovering over a row updates the selected recipe,
        // and clicking the row crafts it immediately.
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        int mw = 560, mh = 400;
        int mx = (sw - mw) / 2, my = (sh - mh) / 2;
        int row = 0;
        for (ItemID id = 1; id < ITEM_COUNT; id++) {
            if (ITEMS[id].inA == ITEM_NONE) continue;
            Rectangle r = { mx + 12, my + 70 + row * 64, mw - 24, 56 };
            UIButton recipeButton = { r, "", false, false };
            UiButtonUpdate(&recipeButton);

            if (recipeButton.hovered) player.craftSel = row;
            if (recipeButton.clicked) PlayerCraft(&player, id);
            row++;
        }
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const float slot = 64, gap = 6;
        float barW = player.slotCount * slot + (player.slotCount > 0 ? (player.slotCount - 1) * gap : 0);
        float x = (GetScreenWidth() - barW) / 2.0f;
        float y = GetScreenHeight() - slot - 20.0f;
        Vector2 mouse = GetMousePosition();

        for (int s = 0; s < player.slotCount; s++) {
            Rectangle r = { x, y, slot, slot };
            if (CheckCollisionPointRec(mouse, r)) {
                PlayerSelectSlot(&player, s);
                break;
            }
            x += slot + gap;
        }
    }

    int wheel = GetMouseWheelMove();
    if (wheel > 0) PlayerSelectRelative(&player, -1);
    if (wheel < 0) PlayerSelectRelative(&player, 1);

    if (IsKeyPressed(KEY_F5)) WorldSave();
    if (IsKeyPressed(KEY_F9)) WorldLoad();
    if (IsKeyPressed(KEY_ESCAPE)) screen = SCREEN_PAUSE;

    PlayerMove(&player, dt);
    UpdateMiningAndPlacing(dt);

    // Camera keeps the player centered even when the window resizes.
    camera.target = player.pos;
    camera.offset = (Vector2){ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
}

static void DrawGame(void) {
    BeginMode2D(camera);     // everything until EndMode2D is WORLD space
        WorldDraw();
        PlayerDraw(&player);
    EndMode2D();
    UiDrawHelp();            // screen space from here on
    UiDrawHotbar(&player);
    UiDrawInventory(&player);
    UiDrawCraftMenu(&player);
}

static void UpdatePause(void) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;

    pauseResumeButton.rect = (Rectangle){ cx - 110, cy - 20, 220, 56 };
    pauseResumeButton.text = "RESUME";
    pauseQuitButton.rect = (Rectangle){ cx - 110, cy + 60, 220, 56 };
    pauseQuitButton.text = "QUIT";

    if (IsKeyPressed(KEY_ESCAPE) || UiButtonUpdate(&pauseResumeButton)) {
        screen = SCREEN_GAME;
        return;
    }

    if (UiButtonUpdate(&pauseQuitButton)) {
        screen = SCREEN_TITLE;
        player.craftMenuOpen = false;
    }
}

static void DrawPause(void) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 180});
    DrawText("PAUSED",
             cx - MeasureText("PAUSED", 48) / 2, cy - 120, 48, WHITE);

    pauseResumeButton.rect = (Rectangle){ cx - 110, cy - 20, 220, 56 };
    pauseResumeButton.text = "RESUME";
    pauseQuitButton.rect = (Rectangle){ cx - 110, cy + 60, 220, 56 };
    pauseQuitButton.text = "QUIT";

    UiDrawButton(&pauseResumeButton);
    UiDrawButton(&pauseQuitButton);
}

// ─── Title screen ────────────────────────────────────────────
static void UpdateTitle(void) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;
    titleStartButton.rect = (Rectangle){ cx - 110, cy + 90, 220, 56 };
    titleStartButton.text = "START";

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        screen = SCREEN_GAME;
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        CloseWindow();
        return;
    }

    if (UiButtonUpdate(&titleStartButton)) screen = SCREEN_GAME;
}

static void DrawTitle(void) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;
    DrawText("HOME PLANET",
             cx - MeasureText("HOME PLANET", 90)/2, 180, 90,
             (Color){150,100,255,255});
    DrawText("VOID RUNNER — learning edition",
             cx - MeasureText("VOID RUNNER — learning edition", 24)/2, cy + 50, 24, GRAY);

    titleStartButton.rect = (Rectangle){ cx - 110, cy + 90, 220, 56 };
    titleStartButton.text = "START";
    UiDrawButton(&titleStartButton);
}

// ─── Entry point ─────────────────────────────────────────────
int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(TARGET_FPS);
    SetExitKey(KEY_NULL); // handle Escape ourselves in game logic
    WorldInit();
    PlayerInit(&player);
    camera.zoom = 1.0f;

    while (!WindowShouldClose()) {   // false when the X is clicked
        float dt = GetFrameTime();
        if (screen == SCREEN_TITLE) UpdateTitle();
        else if (screen == SCREEN_PAUSE) UpdatePause();
        else                        UpdateGame(dt);

        BeginDrawing();
            ClearBackground(BLACK);
            if (screen == SCREEN_TITLE) DrawTitle();
            else if (screen == SCREEN_PAUSE) DrawPause();
            else                        DrawGame();
            DrawFPS(10, GetScreenHeight() - 25);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
