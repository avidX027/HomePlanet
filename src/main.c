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
typedef enum { SCREEN_TITLE, SCREEN_GAME } Screen;

static Screen   screen = SCREEN_TITLE;
static Player   player;
static Camera2D camera = { 0 };

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
    if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) return;
    if (Vector2Distance(player.pos, mouse) > PLAYER_REACH) return;

    // LMB (held): damage the tile; collect drops if it broke.
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        TileType before = world[tx][ty].type;
        if (WorldDamageTile(tx, ty, PlayerMiningDPS(&player) * dt)) {
            player.inventory[TILES[before].drops] += TILES[before].dropCount;
        }
    }

    // RMB (click): place the selected item, if it's placeable,
    // owned, and the target tile is grass.
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        const ItemInfo *it = &ITEMS[player.selected];
        if (it->placeable &&
            player.inventory[player.selected] > 0 &&
            world[tx][ty].type == TILE_GRASS) {
            WorldSetTile(tx, ty, it->places);
            player.inventory[player.selected]--;
        }
    }
}

// ─── One frame of gameplay ───────────────────────────────────
static void UpdateGame(float dt) {
    if (IsKeyPressed(KEY_TAB)) player.craftMenuOpen = !player.craftMenuOpen;

    if (player.craftMenuOpen) {
        // Menu open: navigation only; the world is paused.
        int n = CraftableCount();
        if (IsKeyPressed(KEY_DOWN)) player.craftSel = (player.craftSel + 1) % n;
        if (IsKeyPressed(KEY_UP))   player.craftSel = (player.craftSel + n - 1) % n;
        if (IsKeyPressed(KEY_ENTER)) PlayerCraft(&player, CraftableAtRow(player.craftSel));
        return;
    }

    // Number keys 1..9 select hotbar items (1 selects item id 1...).
    for (int k = 0; k < ITEM_COUNT - 1 && k < 9; k++)
        if (IsKeyPressed(KEY_ONE + k)) player.selected = (ItemID)(k + 1);

    if (IsKeyPressed(KEY_F5)) WorldSave();
    if (IsKeyPressed(KEY_F9)) WorldLoad();
    if (IsKeyPressed(KEY_ESCAPE)) screen = SCREEN_TITLE;

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
    UiDrawCraftMenu(&player);
}

// ─── Title screen ────────────────────────────────────────────
static void UpdateTitle(void) {
    if (IsKeyPressed(KEY_ENTER)) screen = SCREEN_GAME;
}

static void DrawTitle(void) {
    int cx = GetScreenWidth() / 2;
    DrawText("HOME PLANET",
             cx - MeasureText("HOME PLANET", 90)/2, 180, 90,
             (Color){150,100,255,255});
    DrawText("VOID RUNNER — learning edition",
             cx - MeasureText("VOID RUNNER — learning edition", 24)/2, 290, 24, GRAY);
    DrawText("[ENTER] start",
             cx - MeasureText("[ENTER] start", 30)/2, 420, 30, WHITE);
}

// ─── Entry point ─────────────────────────────────────────────
int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(TARGET_FPS);
    SetExitKey(KEY_NULL);            // ESC shouldn't kill the window;
                                     // we use it to reach the title screen
    WorldInit();
    PlayerInit(&player);
    camera.zoom = 1.0f;

    while (!WindowShouldClose()) {   // false when the X is clicked
        float dt = GetFrameTime();

        if (screen == SCREEN_TITLE) UpdateTitle();
        else                        UpdateGame(dt);

        BeginDrawing();
            ClearBackground(BLACK);
            if (screen == SCREEN_TITLE) DrawTitle();
            else                        DrawGame();
            DrawFPS(10, GetScreenHeight() - 25);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
