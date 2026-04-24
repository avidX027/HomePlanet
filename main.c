// ============================================================
//  HOME PLANET: VOID RUNNER — MAIN
// ============================================================

#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "world.h"
#include "player.h"
#include "ui.h"

typedef enum {
    STATE_MENU,
    STATE_HOME_PLANET,
    STATE_BASE_3D,
    STATE_LOADING
} GameState;

#define MAX_BASE_OBJECTS 128
typedef enum { OBJ_SERVER, OBJ_COOLING_FAN, OBJ_ANTENNA_DISH, OBJ_CABLE_RACK, OBJ_COUNT_3D } BaseObjType;
typedef struct { BaseObjType type; Vector3 pos; float rotation; bool active; } BaseObject;

static BaseObject baseObjects[MAX_BASE_OBJECTS];
static int        baseObjCount = 0;
static const char *BaseObjNames[] = { "Server", "Cooling Fan", "Antenna", "Cables" };
static Color BaseObjColors[] = {
    (Color){60,60,180,255},
    (Color){80,220,220,255},
    (Color){180,80,220,255},
    (Color){200,160,40,255}
};

static GameState   curState       = STATE_MENU;
static Player      player;
static Camera2D    cam2d          = {0};
static Camera3D    cam3d          = {0};
static float       autoMineTimer  = 0.0f;
static int         selectedBase3DType = 0;
static float       baseHeatLevel  = 0.0f;
static int         baseServerCount = 0;
static int         baseFanCount    = 0;
static char        lastBlueprint[40] = "";
static bool        bpMenuOpen = false;
static float       flashAlpha = 0.0f;
static Color       flashColor = BLACK;

// Menu button animation
static Rectangle   menuBtnRect = {0};

// Prototypes
void GameInit(void);
void UpdateMenu(float dt);
void UpdateHomePlanet(float dt);
void UpdateBase3D(float dt);
void DrawMenu(void);
void DrawHomePlanet(void);
void DrawBase3D(void);
void DrawPauseMenu(void);
void FlashScreen(Color c, float dur);
void UpdateFlash(float dt);
void DrawFlash(void);
void RecalcBaseStats(void);
void PlaceBase3DObject(BaseObjType type, Vector3 pos);

// ─── INIT ─────────────────────────────────────────────────
void GameInit(void) {
    WorldInit();
    PlayerInit(&player);
    UiLoadAssets();

    cam2d.target   = player.pos;
    cam2d.offset   = (Vector2){ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
    cam2d.zoom     = 1.0f;

    cam3d.position   = (Vector3){ 12, 12, 12 };
    cam3d.target     = (Vector3){ 0, 0, 0 };
    cam3d.up         = (Vector3){ 0, 1, 0 };
    cam3d.fovy       = 45;
    cam3d.projection = CAMERA_PERSPECTIVE;

    PlaceBase3DObject(OBJ_SERVER,       (Vector3){2, 0, 2});
    PlaceBase3DObject(OBJ_COOLING_FAN,  (Vector3){4, 0, 2});
    PlaceBase3DObject(OBJ_ANTENNA_DISH, (Vector3){-3, 0, 0});
    PlaceBase3DObject(OBJ_CABLE_RACK,   (Vector3){0, 0, -3});
    RecalcBaseStats();
}

void PlaceBase3DObject(BaseObjType type, Vector3 pos) {
    if (baseObjCount >= MAX_BASE_OBJECTS) return;
    baseObjects[baseObjCount].type     = type;
    baseObjects[baseObjCount].pos      = pos;
    baseObjects[baseObjCount].active   = true;
    baseObjCount++;
}

void RecalcBaseStats(void) {
    baseServerCount = 0; baseFanCount = 0;
    for (int i = 0; i < baseObjCount; i++) {
        if (!baseObjects[i].active) continue;
        if (baseObjects[i].type == OBJ_SERVER)      baseServerCount++;
        if (baseObjects[i].type == OBJ_COOLING_FAN) baseFanCount++;
    }
    float h = baseServerCount * 10.0f - baseFanCount * 8.0f;
    baseHeatLevel = h < 0 ? 0 : h;
}

void FlashScreen(Color c, float dur) { flashColor = c; flashAlpha = dur; }
void UpdateFlash(float dt) { if (flashAlpha > 0) flashAlpha -= dt; if (flashAlpha < 0) flashAlpha = 0; }
void DrawFlash(void) {
    if (flashAlpha <= 0) return;
    float a = flashAlpha > 1 ? 1 : flashAlpha;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){flashColor.r, flashColor.g, flashColor.b,
                          (unsigned char)(a * 200)});
}

// ─── MENU STATE ───────────────────────────────────────────
void UpdateMenu(float dt) {
    (void)dt;
    // Button click OR Enter starts the game
    bool click = (CheckCollisionPointRec(GetMousePosition(), menuBtnRect) &&
                  IsMouseButtonPressed(MOUSE_LEFT_BUTTON));
    if (IsKeyPressed(KEY_ENTER) || click) {
        curState = STATE_HOME_PLANET;
        FlashScreen((Color){80,40,200,255}, 0.6f);
    }
}

void DrawMenu(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float t = (float)GetTime();

    DrawRectangleGradientV(0, 0, sw, sh, (Color){10,0,30,255}, BLACK);

    SetRandomSeed(42);
    for (int i = 0; i < 200; i++) {
        int sx = GetRandomValue(0, sw);
        int sy = GetRandomValue(0, sh);
        float b = sinf(t * 0.8f + i * 0.3f) * 0.5f + 0.5f;
        int sz = GetRandomValue(1, 3);
        DrawCircle(sx, sy, sz, (Color){255,255,255,(unsigned char)(b*180)});
    }
    SetRandomSeed((unsigned int)GetTime());

    int cx = sw/2;
    DrawText("HOME PLANET", cx - MeasureText("HOME PLANET", 120)/2, 160, 120,
             (Color){150,100,255,255});
    DrawText("V O I D   R U N N E R", cx - MeasureText("V O I D   R U N N E R", 38)/2, 290, 38,
             (Color){200,160,255,200});

    // ── GO TO WORLD BUTTON ── purple fade, 3s cycle
    float phase  = fmodf(t, 3.0f) / 3.0f;            // 0..1
    float wave   = 0.5f - 0.5f * cosf(phase * 2 * PI); // 0..1..0 smooth
    unsigned char a = (unsigned char)(80 + wave * 175);

    const char *btnText = "GO TO WORLD";
    int fontSize = 42;
    int tw = MeasureText(btnText, fontSize);
    int btnW = tw + 80;
    int btnH = 70;
    menuBtnRect = (Rectangle){ cx - btnW/2, 470, btnW, btnH };

    bool hover = CheckCollisionPointRec(GetMousePosition(), menuBtnRect);
    Color bg = hover ? (Color){70,30,140,a} : (Color){35,10,80,(unsigned char)(a*0.7f)};
    DrawRectangleRounded(menuBtnRect, 0.35f, 10, bg);
    DrawRectangleRoundedLines(menuBtnRect, 0.35f, 10,
                              (Color){180,130,255,a});

    Color textCol = { 200, 140, 255, a };
    DrawText(btnText, cx - tw/2, (int)menuBtnRect.y + 18, fontSize, textCol);

    DrawText("F11 Fullscreen", 30, sh - 40, 18, (Color){80,80,120,255});
    DrawText("v2.1", sw - 80, sh - 40, 18, (Color){80,80,120,255});
}

// ─── PAUSE MENU ──────────────────────────────────────────
void DrawPauseMenu(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, (Color){0,0,0,180});
    DrawRectangle(sw/2-200, sh/2-150, 400, 300, (Color){10,15,40,240});
    DrawRectangleLines(sw/2-200, sh/2-150, 400, 300, WHITE);
    DrawText("PAUSED", sw/2 - MeasureText("PAUSED",48)/2, sh/2-120, 48, WHITE);
    DrawText("[ESC] Resume",   sw/2-150, sh/2-40, 28, LIGHTGRAY);
    DrawText("[F5] Save",      sw/2-150, sh/2+0,  28, LIGHTGRAY);
    DrawText("[Q] Quit",       sw/2-150, sh/2+40, 28, RED);
}

// ─── HOME PLANET ─────────────────────────────────────────
static bool showPauseMenu = false;

void UpdateHomePlanet(float dt) {
    // ESC: if any overlay is open, close it; otherwise toggle pause
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (uiSettingsMenuOpen)     uiSettingsMenuOpen = false;
        else if (uiControlsOverlayOpen) uiControlsOverlayOpen = false;
        else if (uiDebugMenuOpen)   uiDebugMenuOpen = false;
        else if (player.craftMenuOpen) player.craftMenuOpen = false;
        else if (bpMenuOpen)        { bpMenuOpen = false; bpUI.active = false; }
        else                        showPauseMenu = !showPauseMenu;
        return;
    }
    if (showPauseMenu) {
        if (IsKeyPressed(KEY_Q)) CloseWindow();
        if (IsKeyPressed(KEY_F5)) { WorldSave(); FlashScreen(GREEN, 0.3f); }
        return;
    }

    if (IsKeyPressed(KEY_F5)) { WorldSave(); FlashScreen(GREEN, 0.3f); }
    if (IsKeyPressed(KEY_F9)) { WorldLoad(); FlashScreen(SKYBLUE, 0.3f); }

    // Hotbar keys 1-7
    for (int k = KEY_ONE; k <= KEY_SEVEN; k++) {
        if (IsKeyPressed(k)) {
            int slot = k - KEY_ONE;
            if (slot < player.hotbarCount) player.hotbarSel = slot;
        }
    }
    float wheel = GetMouseWheelMove();
    if (wheel > 0) { player.hotbarSel--; if (player.hotbarSel < 0) player.hotbarSel = player.hotbarCount-1; }
    if (wheel < 0) { player.hotbarSel++; if (player.hotbarSel >= player.hotbarCount) player.hotbarSel = 0; }

    // Craft menu
    if (IsKeyPressed(KEY_TAB)) player.craftMenuOpen = !player.craftMenuOpen;
    if (player.craftMenuOpen) {
        if (IsKeyPressed(KEY_UP))   { player.craftSel--; if (player.craftSel < 0) player.craftSel = RECIPE_COUNT-1; }
        if (IsKeyPressed(KEY_DOWN)) { player.craftSel++; if (player.craftSel >= RECIPE_COUNT) player.craftSel = 0; }
        if (IsKeyPressed(KEY_ENTER)) {
            if (PlayerCraft(&player, player.craftSel)) FlashScreen(GREEN, 0.25f);
            else FlashScreen(RED, 0.25f);
        }
    }

    // Blueprint
    if (IsKeyPressed(KEY_B)) { bpMenuOpen = !bpMenuOpen; bpUI.active = bpMenuOpen; }
    if (bpMenuOpen) {
        if (IsKeyPressed(KEY_C)) {
            int ox = (int)(player.pos.x / TILE_SIZE) - BLUEPRINT_AREA/2;
            int oy = (int)(player.pos.y / TILE_SIZE) - BLUEPRINT_AREA/2;
            WorldCaptureBlueprint(ox, oy, lastBlueprint, sizeof(lastBlueprint));
            strncpy(bpUI.code, lastBlueprint, sizeof(bpUI.code)-1);
            bpUI.pasteMode = true;
        }
        if (IsKeyPressed(KEY_P) && lastBlueprint[0]) {
            int ox = (int)(player.pos.x / TILE_SIZE) - BLUEPRINT_AREA/2;
            int oy = (int)(player.pos.y / TILE_SIZE) - BLUEPRINT_AREA/2;
            WorldPasteBlueprint(ox, oy, lastBlueprint, &bpUI.woodCost, &bpUI.stoneCost);
            if (PlayerCountItem(&player, ITEM_WOOD) >= bpUI.woodCost &&
                PlayerCountItem(&player, ITEM_STONE) >= bpUI.stoneCost) {
                PlayerRemoveItem(&player, ITEM_WOOD,  bpUI.woodCost);
                PlayerRemoveItem(&player, ITEM_STONE, bpUI.stoneCost);
                WorldApplyBlueprint(ox, oy, lastBlueprint);
                FlashScreen((Color){0,255,100,255}, 0.3f);
            } else FlashScreen(RED, 0.25f);
        }
    }

    // Block other inputs when overlays are open
    bool blocked = player.craftMenuOpen || bpMenuOpen ||
                   uiSettingsMenuOpen || uiControlsOverlayOpen || uiDebugMenuOpen;
    if (!blocked) PlayerMove(&player, dt);

    cam2d.target = player.pos;
    cam2d.offset = (Vector2){ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };

    // Auto-miner tick
    autoMineTimer += dt;
    if (autoMineTimer >= 0.25f) {
        autoMineTimer = 0.0f;
        for (int x = 0; x < WORLD_SIZE; x++) {
            for (int y = 0; y < WORLD_SIZE; y++) {
                if (world[x][y].type != TILE_MINING_DRILL) continue;
                int item = WorldTickMiner(x, y, 0.25f);
                if (item > 0) {
                    ItemID id = (item == 1) ? ITEM_WOOD : ITEM_STONE;
                    PlayerAddItem(&player, id, 5);
                }
            }
        }
    }
    WorldTickConveyors(dt);

    // Enter 3D base
    int ptx = (int)(player.pos.x / TILE_SIZE);
    int pty = (int)(player.pos.y / TILE_SIZE);
    if (ptx >= 0 && ptx < WORLD_SIZE && pty >= 0 && pty < WORLD_SIZE) {
        if (world[ptx][pty].type == TILE_ROCKET_PAD && IsKeyPressed(KEY_E)) {
            curState = STATE_BASE_3D;
            FlashScreen(WHITE, 0.4f);
        }
    }
}

// ─── MINING & PLACING (world-space mouse) ────────────────
static void HandleMiningAndPlacing(float dt) {
    // Skip if any overlay blocks input
    if (player.craftMenuOpen || bpMenuOpen || showPauseMenu ||
        uiSettingsMenuOpen || uiControlsOverlayOpen || uiDebugMenuOpen) return;

    Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), cam2d);
    int tx = (int)(mouseWorld.x / TILE_SIZE);
    int ty = (int)(mouseWorld.y / TILE_SIZE);
    if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) return;

    float dist = Vector2Distance(player.pos, mouseWorld);
    if (dist > PLAYER_REACH) return;

    Rectangle hr = { (float)(tx*TILE_SIZE), (float)(ty*TILE_SIZE),
                     TILE_SIZE - 1.0f, TILE_SIZE - 1.0f };
    DrawRectangleLinesEx(hr, 2, (Color){255,255,255,120});

    // HOLD LMB to mine — damage based on selected tool
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        TileType t = world[tx][ty].type;
        if (t == TILE_GRASS || t == TILE_ROCKET_PAD) return;

        ItemID sel = PlayerSelectedItem(&player);
        float dps = ItemMiningDPS[sel]; // hands = 1, pickaxe = 4
        if (dps <= 0) dps = 1.0f;        // default to hand speed if tool doesn't mine

        world[tx][ty].health -= dps * dt;
        if (world[tx][ty].health <= 0) {
            if (t == TILE_WOOD)          PlayerAddItem(&player, ITEM_WOOD, 10);
            else if (t == TILE_STONE)    PlayerAddItem(&player, ITEM_STONE, 10);
            else if (t == TILE_TOWER)    PlayerAddItem(&player, ITEM_WOOD, 10);
            else if (t == TILE_MINING_DRILL) PlayerAddItem(&player, ITEM_STONE, 15);
            WorldSetTile(tx, ty, TILE_GRASS);
            FlashScreen((Color){200,200,200,255}, 0.08f);
        }
    }

    // RMB place
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        if (world[tx][ty].type != TILE_GRASS) return;
        ItemID sel = PlayerSelectedItem(&player);
        // For now, only wood/stone placeable as "dirt-ish" demonstration.
        // You can expand this mapping as you add tile placement items.
        if (sel == ITEM_WOOD && PlayerRemoveItem(&player, ITEM_WOOD, 1)) {
            WorldSetTile(tx, ty, TILE_WOOD);
        } else if (sel == ITEM_STONE && PlayerRemoveItem(&player, ITEM_STONE, 1)) {
            WorldSetTile(tx, ty, TILE_STONE);
        }
    }
}

void DrawHomePlanet(void) {
    float dt = GetFrameTime();
    BeginMode2D(cam2d);
        WorldDraw();

        int ptx = (int)(player.pos.x / TILE_SIZE);
        int pty = (int)(player.pos.y / TILE_SIZE);
        if (ptx >= 0 && ptx < WORLD_SIZE && pty >= 0 && pty < WORLD_SIZE) {
            if (world[ptx][pty].type == TILE_ROCKET_PAD)
                DrawText("[E] ENTER BASE", (int)player.pos.x - 80, (int)player.pos.y - 50, 24, WHITE);
        }

        DrawCircleV(player.pos, 20, BLUE);
        DrawCircleLines((int)player.pos.x, (int)player.pos.y, 22, (Color){0,255,255,255});

        HandleMiningAndPlacing(dt);
    EndMode2D();

    // Top-right gear icon
    Rectangle gear = DrawSettingsGear();
    if (CheckCollisionPointRec(GetMousePosition(), gear) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiSettingsMenuOpen = !uiSettingsMenuOpen;

    // Hotbar (returns fractal-button rect for click detection)
    Rectangle dbgBtn = DrawHotbar(&player);
    if (CheckCollisionPointRec(GetMousePosition(), dbgBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiDebugMenuOpen = !uiDebugMenuOpen;

    DrawDebugMenu(&player);
    DrawCraftMenu(&player);
    BlueprintDraw(&player);

    // Settings menu — handle clicks here
    int action = DrawSettingsMenu();
    if (action == 1) { uiControlsOverlayOpen = true; uiSettingsMenuOpen = false; }
    if (action == 2) { WorldSave(); FlashScreen(GREEN, 0.3f); uiSettingsMenuOpen = false; }
    if (action == 3) { curState = STATE_MENU; uiSettingsMenuOpen = false; FlashScreen(BLACK, 0.4f); }
    if (action == 4) { uiSettingsMenuOpen = false; }

    DrawControlsOverlay();
    if (showPauseMenu) DrawPauseMenu();
}

// ─── 3D BASE ─────────────────────────────────────────────
void UpdateBase3D(float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) { curState = STATE_HOME_PLANET; FlashScreen(WHITE, 0.3f); }
    UpdateCamera(&cam3d, CAMERA_ORBITAL);
    for (int k = KEY_ONE; k <= KEY_FOUR; k++)
        if (IsKeyPressed(k)) selectedBase3DType = k - KEY_ONE;

    if (IsKeyPressed(KEY_SPACE)) {
        BaseObjType t = (BaseObjType)selectedBase3DType;
        bool ok = false;
        if (t == OBJ_SERVER)       ok = PlayerRemoveItem(&player, ITEM_ELECTRONICS, 3);
        if (t == OBJ_COOLING_FAN)  ok = PlayerRemoveItem(&player, ITEM_STONE, 5);
        if (t == OBJ_ANTENNA_DISH) ok = PlayerRemoveItem(&player, ITEM_ANTENNA, 1);
        if (t == OBJ_CABLE_RACK)   ok = PlayerRemoveItem(&player, ITEM_CABLE, 2);
        if (ok) {
            Vector3 pos = { cam3d.target.x + GetRandomValue(-4,4), 0,
                             cam3d.target.z + GetRandomValue(-4,4) };
            PlaceBase3DObject(t, pos);
            RecalcBaseStats();
            FlashScreen((Color){0,150,255,255}, 0.25f);
        } else FlashScreen(RED, 0.2f);
    }
    UpdateFlash(dt);
}

static void DrawBase3DObjects(void) {
    float t = (float)GetTime();
    for (int i = 0; i < baseObjCount; i++) {
        if (!baseObjects[i].active) continue;
        Vector3 p3 = baseObjects[i].pos;
        p3.y = 0.5f;
        switch (baseObjects[i].type) {
            case OBJ_SERVER:
                DrawCube(p3, 1, 2, 0.6f, (Color){40,40,160,255});
                DrawCubeWires(p3, 1, 2, 0.6f, (Color){80,80,255,255});
                if ((int)(t*4 + i) % 2 == 0)
                    DrawSphere((Vector3){p3.x, p3.y+0.9f, p3.z+0.25f}, 0.06f, GREEN);
                break;
            case OBJ_COOLING_FAN: {
                DrawCylinder(p3, 0.5f, 0.5f, 0.3f, 8, (Color){60,200,200,255});
                Vector3 b = {p3.x + cosf(t*5)*0.6f, p3.y+0.2f, p3.z + sinf(t*5)*0.6f};
                DrawLine3D(p3, b, WHITE);
                break;
            }
            case OBJ_ANTENNA_DISH:
                DrawCylinder(p3, 0.1f, 0.1f, 1.5f, 6, LIGHTGRAY);
                DrawSphereWires((Vector3){p3.x, p3.y+1.4f, p3.z}, 0.4f, 5, 5, (Color){200,80,220,200});
                break;
            case OBJ_CABLE_RACK:
                DrawCube(p3, 2, 0.1f, 0.3f, (Color){160,140,30,255});
                break;
            default: break;
        }
    }
}

void DrawBase3D(void) {
    BeginMode3D(cam3d);
        DrawGrid(20, 1.0f);
        DrawCube((Vector3){0, -0.05f, 0}, 40, 0.1f, 40, (Color){20,20,40,255});
        DrawBase3DObjects();
    EndMode3D();

    int sw = GetScreenWidth();
    DrawRectangle(0, 0, sw, 100, (Color){0,0,0,200});
    DrawText("3D BASE", 30, 15, 36, GOLD);
    DrawText("[1-4] Select  [SPACE] Place  [ESC] Exit", 30, 60, 20, LIGHTGRAY);

    int px = sw - 340, py = 120;
    DrawRectangle(px-10, py-10, 330, 210, (Color){0,0,0,200});
    DrawRectangleLines(px-10, py-10, 330, 210, (Color){0,180,255,120});
    DrawText(TextFormat("Servers: %d  Fans: %d  Heat: %.0f",
                         baseServerCount, baseFanCount, baseHeatLevel),
             px, py, 18, baseHeatLevel > 30 ? RED : GREEN);
    for (int i = 0; i < OBJ_COUNT_3D; i++) {
        bool sel = (i == selectedBase3DType);
        int yy = py + 35 + i*40;
        DrawRectangle(px, yy, 300, 34,
                      sel ? (Color){30,60,100,255} : (Color){15,15,30,255});
        DrawRectangle(px, yy+4, 18, 18, BaseObjColors[i]);
        DrawText(TextFormat("[%d] %s", i+1, BaseObjNames[i]),
                 px+26, yy+6, 20, sel ? WHITE : GRAY);
    }
    DrawFlash();
}

// ─── MAIN ─────────────────────────────────────────────────
int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "HOME PLANET: VOID RUNNER");
    SetTargetFPS(144);

    GameInit();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        switch (curState) {
            case STATE_MENU:         UpdateMenu(dt);        break;
            case STATE_HOME_PLANET:  UpdateHomePlanet(dt);  break;
            case STATE_BASE_3D:      UpdateBase3D(dt);      break;
            default: break;
        }
        UpdateFlash(dt);

        BeginDrawing();
        ClearBackground(BLACK);
        switch (curState) {
            case STATE_MENU:         DrawMenu();        break;
            case STATE_HOME_PLANET:  DrawHomePlanet();  break;
            case STATE_BASE_3D:      DrawBase3D();      break;
            default: break;
        }
        DrawFlash();
        DrawFPS(10, GetScreenHeight() - 25);
        EndDrawing();
    }

    UiUnloadAssets();
    CloseWindow();
    return 0;
}
