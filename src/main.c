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
#include <string.h>
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
static UIButton titleContinueButton = { 0 };
static UIButton pauseSaveButton = { 0 };
static UIButton pauseLoadButton = { 0 };
static UIButton pauseResumeButton = { 0 };
static UIButton pauseQuitButton = { 0 };

typedef struct {
    bool     active;
    Vector2  pos;
    Vector2  dir;
    float    speed;
    float    traveled;
    float    maxRange;
    ItemID   type;
} Projectile;

#define MAX_PROJECTILES 16
static Projectile projectiles[MAX_PROJECTILES] = { 0 };

typedef struct {
    char magic[4];
    int  version;
} SaveHeader;

#define SAVE_MAGIC "HPSV"
#define SAVE_VERSION 1

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
static void GiveTileBreakDrops(TileType before) {
    if (before == TILE_TREE) {
        PlayerGiveItem(&player, TILES[before].drops, TILES[before].dropCount);
        if (GetRandomValue(1, 100) <= 25) {
            PlayerGiveItem(&player, ITEM_RUBBER, 1);
        }
        return;
    }
    if (before == TILE_ROCK) {
        PlayerGiveItem(&player, ITEM_STONE, TILES[before].dropCount);
        if (GetRandomValue(1, 100) <= 30) {
            PlayerGiveItem(&player, ITEM_COAL, 1);
        }
        if (GetRandomValue(1, 100) <= 20) {
            PlayerGiveItem(&player, ITEM_SULFUR_ORE, 1);
        }
        if (GetRandomValue(1, 100) <= 12) {
            PlayerGiveItem(&player, ITEM_METAL_ORE, 1);
        }
        return;
    }
    PlayerGiveItem(&player, TILES[before].drops, TILES[before].dropCount);
}

static void SpawnProjectile(const Vector2 origin, const Vector2 target, ItemID type) {
    Vector2 direction = Vector2Normalize(Vector2Subtract(target, origin));
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].active    = true;
            projectiles[i].pos       = origin;
            projectiles[i].dir       = direction;
            projectiles[i].traveled  = 0.0f;
            projectiles[i].type      = type;
            if (type == ITEM_BULLET) {
                projectiles[i].speed    = PISTOL_BULLET_SPEED;
                projectiles[i].maxRange = PISTOL_REACH;
            } else {
                projectiles[i].speed    = SLINGSHOT_SPEED;
                projectiles[i].maxRange = SLINGSHOT_REACH;
            }
            return;
        }
    }
}

static void UpdateProjectiles(float dt) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *proj = &projectiles[i];
        if (!proj->active) continue;

        Vector2 previousPos = proj->pos;
        proj->pos = Vector2Add(proj->pos, Vector2Scale(proj->dir, proj->speed * dt));
        proj->traveled += Vector2Distance(previousPos, proj->pos);

        if (proj->traveled >= proj->maxRange) {
            proj->active = false;
            continue;
        }

        int tx = (int)(proj->pos.x / TILE_SIZE);
        int ty = (int)(proj->pos.y / TILE_SIZE);
        if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) {
            proj->active = false;
            continue;
        }

        TileType hitType = world[tx][ty].type;
        if (!TILES[hitType].breakable) continue;

        float distanceRatio = proj->traveled / proj->maxRange;
        if (distanceRatio > 1.0f) distanceRatio = 1.0f;
        float damage;
        if (proj->type == ITEM_BULLET) {
            damage = PISTOL_BULLET_DAMAGE * (1.0f - distanceRatio * 0.5f);
            if (damage < 1.0f) damage = 1.0f;
        } else {
            damage = SLINGSHOT_BASE_DAMAGE * (1.0f - distanceRatio);
            if (damage < 0.5f) damage = 0.5f;
        }

        if (WorldDamageTile(tx, ty, damage)) {
            GiveTileBreakDrops(hitType);
        }
        proj->active = false;
    }
}

static void DrawProjectiles(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *proj = &projectiles[i];
        if (!proj->active) continue;
        DrawCircleV(proj->pos, 5.0f, ITEMS[proj->type].color);
    }
}

static void ValidateLoadedPlayer(Player *p) {
    float minPos = PLAYER_RADIUS;
    float maxPos = (WORLD_SIZE * TILE_SIZE) - PLAYER_RADIUS;
    if (p->pos.x < minPos) p->pos.x = minPos;
    if (p->pos.y < minPos) p->pos.y = minPos;
    if (p->pos.x > maxPos) p->pos.x = maxPos;
    if (p->pos.y > maxPos) p->pos.y = maxPos;

    p->slotCount = HOTBAR_MAX_SLOTS;
    if (p->selectedSlot < 0 || p->selectedSlot >= HOTBAR_MAX_SLOTS) p->selectedSlot = 0;

    for (int i = 0; i < ITEM_COUNT; i++) {
        if (p->inventory[i] < 0) p->inventory[i] = 0;
    }

    for (int i = 0; i < HOTBAR_MAX_SLOTS; i++) {
        if (p->hotbar[i] < ITEM_NONE || p->hotbar[i] >= ITEM_COUNT) {
            p->hotbar[i] = ITEM_NONE;
        }
    }

    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (p->inventorySlots[i] < ITEM_NONE || p->inventorySlots[i] >= ITEM_COUNT) {
            p->inventorySlots[i] = ITEM_NONE;
            p->inventoryAmounts[i] = 0;
            continue;
        }
        if (p->inventoryAmounts[i] < 0) p->inventoryAmounts[i] = 0;
        if (p->inventorySlots[i] == ITEM_NONE) p->inventoryAmounts[i] = 0;
    }

    if (p->inventoryCursor < 0 || p->inventoryCursor >= INVENTORY_SIZE) p->inventoryCursor = 0;
    if (p->inventoryDragIndex < -1 || p->inventoryDragIndex >= INVENTORY_SIZE) p->inventoryDragIndex = -1;
    if (p->craftSel < 0) p->craftSel = 0;
    if (p->craftScroll < 0) p->craftScroll = 0;

    int craftCount = CraftableCount();
    if (craftCount <= 0) {
        p->craftSel = 0;
        p->craftScroll = 0;
    } else {
        if (p->craftSel >= craftCount) p->craftSel = craftCount - 1;
        if (p->craftScroll >= craftCount) p->craftScroll = craftCount - 1;
    }

    ItemID selectedId = ITEM_NONE;
    if (p->selectedSlot >= 0 && p->selectedSlot < HOTBAR_MAX_SLOTS) {
        ItemID slotId = p->inventorySlots[p->selectedSlot];
        if (slotId != ITEM_NONE && p->inventoryAmounts[p->selectedSlot] > 0) {
            selectedId = slotId;
        }
    }
    p->selected = selectedId;
    p->inventoryDragging = false;
    p->inventoryDragIndex = -1;
}

static void ResetTransientState(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = false;
    }
}

static void GameSave(void) {
    FILE *f = fopen(SAVE_FILE, "wb");
    if (f == NULL) return;
    SaveHeader header = { {'H','P','S','V'}, SAVE_VERSION };
    fwrite(&header, sizeof(header), 1, f);
    fwrite(&player, sizeof(player), 1, f);
    fwrite(world, sizeof(world), 1, f);
    fclose(f);
}

static bool GameLoad(void) {
    FILE *f = fopen(SAVE_FILE, "rb");
    if (f == NULL) return false;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long fileSize = ftell(f);
    if (fileSize < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    long worldOnlySize = (long)sizeof(world);
    if (fileSize == worldOnlySize) {
        if (fread(world, sizeof(world), 1, f) != 1) {
            fclose(f);
            return false;
        }
        fclose(f);
        PlayerInit(&player);
        ResetTransientState();
        return true;
    }

    SaveHeader header = { 0 };
    Player loadedPlayer = { 0 };
    Tile loadedWorld[WORLD_SIZE][WORLD_SIZE] = { 0 };
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return false;
    }
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
    fclose(f);

    player = loadedPlayer;
    memcpy(world, loadedWorld, sizeof(world));
    ValidateLoadedPlayer(&player);
    ResetTransientState();
    return true;
}

static void UpdateMiningAndPlacing(float dt) {
    // Convert the mouse from SCREEN pixels to WORLD pixels.
    // (The camera follows the player, so these differ.)
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    int tx = (int)(mouse.x / TILE_SIZE);       // which tile is that?
    int ty = (int)(mouse.y / TILE_SIZE);
    // Check placement bounds
    if (tx < 0 || tx >= WORLD_SIZE || ty < 0 || ty >= WORLD_SIZE) return;
    float reach = (player.selected == ITEM_SLINGSHOT) ? SLINGSHOT_REACH : PLAYER_REACH;
    if (Vector2Distance(player.pos, mouse) > reach) return;

    if (player.selected == ITEM_SLINGSHOT) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && player.inventory[ITEM_SMALL_STONE] > 0) {
            PlayerRemoveItem(&player, ITEM_SMALL_STONE, 1);
            SpawnProjectile(player.pos, mouse, ITEM_SMALL_STONE);
        }
        return;
    }

    if (player.selected == ITEM_PISTOL) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && player.inventory[ITEM_BULLET] > 0) {
            PlayerRemoveItem(&player, ITEM_BULLET, 1);
            SpawnProjectile(player.pos, mouse, ITEM_BULLET);
        }
        return;
    }

    // LMB (held): damage the tile; collect drops if it broke.
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        TileType before = world[tx][ty].type;
        if (WorldDamageTile(tx, ty, PlayerMiningDPS(&player) * dt)) {
            GiveTileBreakDrops(before);
        }
    }

    // RMB (held): place the selected item continuously while dragging.
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
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
        if (n > 0) {
            if (IsKeyPressed(KEY_DOWN)) {
                player.craftSel = (player.craftSel + 1) % n;
            }
            if (IsKeyPressed(KEY_UP)) {
                player.craftSel = (player.craftSel + n - 1) % n;
            }
            int visibleRows = 4;
            if (player.craftSel < player.craftScroll) {
                player.craftScroll = player.craftSel;
            } else if (player.craftSel >= player.craftScroll + visibleRows) {
                player.craftScroll = player.craftSel - visibleRows + 1;
            }
            if (player.craftScroll < 0) player.craftScroll = 0;
            if (player.craftScroll > n - visibleRows) player.craftScroll = n - visibleRows;
            if (player.craftScroll < 0) player.craftScroll = 0;
        }
        if (IsKeyPressed(KEY_ENTER)) PlayerCraft(&player, CraftableAtRow(player.craftSel));
        // Mouse support: hovering over a row updates the selected recipe,
        // and clicking the row crafts it immediately.
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        int mw = 560, mh = 400;
        int mx = (sw - mw) / 2, my = (sh - mh) / 2;
        int visibleRows = 4;
        int startRow = player.craftScroll;
        int endRow = startRow + visibleRows;
        if (endRow > n) endRow = n;
        int current = 0;
        int row = 0;
        for (ItemID id = 1; id < ITEM_COUNT; id++) {
            if (ITEMS[id].inA == ITEM_NONE) continue;
            if (current < startRow) {
                current++;
                continue;
            }
            if (current >= endRow) break;
            Rectangle r = { mx + 12, my + 70 + row * 64, mw - 24, 56 };
            UIButton recipeButton = { r, "", false, false };
            UiButtonUpdate(&recipeButton);

            if (recipeButton.hovered) player.craftSel = current;
            if (recipeButton.clicked) PlayerCraft(&player, id);
            row++;
            current++;
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
    if (wheel != 0) {
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            camera.zoom += wheel * 0.1f;
            if (camera.zoom < 0.6f) camera.zoom = 0.6f;
            if (camera.zoom > 2.0f) camera.zoom = 2.0f;
        } else {
            if (wheel > 0) PlayerSelectRelative(&player, -1);
            if (wheel < 0) PlayerSelectRelative(&player, 1);
        }
    }

    if (IsKeyPressed(KEY_F5)) GameSave();
    if (IsKeyPressed(KEY_F9)) GameLoad();
    if (IsKeyPressed(KEY_ESCAPE)) screen = SCREEN_PAUSE;

    PlayerMove(&player, dt);
    UpdateMiningAndPlacing(dt);
    UpdateProjectiles(dt);

    // Camera keeps the player centered even when the window resizes.
    camera.target = player.pos;
    camera.offset = (Vector2){ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
}

static void DrawGame(void) {
    BeginMode2D(camera);     // everything until EndMode2D is WORLD space
        WorldDraw();
        PlayerDraw(&player);
        DrawProjectiles();
    EndMode2D();
    UiDrawHelp();            // screen space from here on
    UiDrawHotbar(&player);
    UiDrawInventory(&player);
    UiDrawCraftMenu(&player);
}

static void UpdatePause(void) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;

    pauseResumeButton.rect = (Rectangle){ cx - 110, cy - 90, 220, 56 };
    pauseResumeButton.text = "RESUME";
    pauseSaveButton.rect = (Rectangle){ cx - 110, cy - 20, 220, 56 };
    pauseSaveButton.text = "SAVE";
    pauseLoadButton.rect = (Rectangle){ cx - 110, cy + 50, 220, 56 };
    pauseLoadButton.text = "LOAD";
    pauseQuitButton.rect = (Rectangle){ cx - 110, cy + 120, 220, 56 };
    pauseQuitButton.text = "QUIT";

    if (IsKeyPressed(KEY_ESCAPE) || UiButtonUpdate(&pauseResumeButton)) {
        screen = SCREEN_GAME;
        return;
    }

    if (UiButtonUpdate(&pauseSaveButton)) {
        GameSave();
    }

    if (UiButtonUpdate(&pauseLoadButton)) {
        GameLoad();
        screen = SCREEN_GAME;
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

    pauseResumeButton.rect = (Rectangle){ cx - 110, cy - 90, 220, 56 };
    pauseResumeButton.text = "RESUME";
    pauseSaveButton.rect = (Rectangle){ cx - 110, cy - 20, 220, 56 };
    pauseSaveButton.text = "SAVE";
    pauseLoadButton.rect = (Rectangle){ cx - 110, cy + 50, 220, 56 };
    pauseLoadButton.text = "LOAD";
    pauseQuitButton.rect = (Rectangle){ cx - 110, cy + 120, 220, 56 };
    pauseQuitButton.text = "QUIT";

    UiDrawButton(&pauseResumeButton);
    UiDrawButton(&pauseSaveButton);
    UiDrawButton(&pauseLoadButton);
    UiDrawButton(&pauseQuitButton);
}

// ─── Title screen ────────────────────────────────────────────
static void UpdateTitle(void) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;
    titleStartButton.rect = (Rectangle){ cx - 110, cy + 90, 220, 56 };
    titleStartButton.text = "NEW GAME";
    titleContinueButton.rect = (Rectangle){ cx - 110, cy + 20, 220, 56 };
    titleContinueButton.text = "CONTINUE";

    bool hasSave = WorldHasSave();

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        WorldInit();
        PlayerInit(&player);
        screen = SCREEN_GAME;
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        CloseWindow();
        return;
    }

    if (hasSave && UiButtonUpdate(&titleContinueButton)) {
        if (!GameLoad()) {
            WorldInit();
            PlayerInit(&player);
        }
        screen = SCREEN_GAME;
        return;
    }

    if (UiButtonUpdate(&titleStartButton)) {
        WorldInit();
        PlayerInit(&player);
        screen = SCREEN_GAME;
    }
}

static void DrawTitle(void) {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;
    DrawText("HOME PLANET",
             cx - MeasureText("HOME PLANET", 90)/2, 180, 90,
             (Color){6, 53, 148,255});
    DrawText("VOID RUNNER",
             cx - MeasureText("VOID RUNNER", 24)/2, cy - 100, 30, GRAY);

    titleStartButton.rect = (Rectangle){ cx - 110, cy + 90, 220, 56 };
    titleStartButton.text = "NEW GAME";
    UiDrawButton(&titleStartButton);

    if (WorldHasSave()) {
        titleContinueButton.rect = (Rectangle){ cx - 110, cy + 20, 220, 56 };
        titleContinueButton.text = "CONTINUE";
        UiDrawButton(&titleContinueButton);
    }
}

// ─── Entry point ─────────────────────────────────────────────
int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(TARGET_FPS);
    SetExitKey(KEY_NULL); // handle Escape ourselves in game logic
    WorldInit();
    PlayerInit(&player);
    camera.zoom = 1.2f;

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
