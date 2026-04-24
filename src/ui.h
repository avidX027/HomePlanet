#ifndef UI_H
#define UI_H

#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "player.h"

// ============================================================
//  UI.H — Home Planet Interface Engine
// ============================================================

// ─── INVENTORY DEBUG / TUNING GLOBALS ─────────────────────
static float uiHotbarSlotScale  = 1.0f;   // 0.5 - 2.0
static float uiHotbarOpacity    = 0.85f;  // 0.0 - 1.0
static float uiHotbarCenterBias = 0.0f;   // -1 left, 0 center, +1 right (offset)
static float uiHotbarCornerRadius = 0.35f;
static bool  uiHotbarShowNumbers = true;
static bool  uiHotbarShowNames   = false;
static bool  uiDebugMenuOpen     = false;
static bool  uiSettingsMenuOpen  = false;

// Texture slot for settings icon. If file missing we draw a gear procedurally.
static Texture2D uiSettingsIcon = {0};
static bool      uiSettingsIconLoaded = false;

static void UiLoadAssets(void) {
    // Expects settings.png in working directory. Safe to skip if not present.
    if (FileExists("settings.png")) {
        uiSettingsIcon = LoadTexture("settings.png");
        uiSettingsIconLoaded = true;
    }
}

static void UiUnloadAssets(void) {
    if (uiSettingsIconLoaded) UnloadTexture(uiSettingsIcon);
}

// ─── DRAW: HAND ICON ──────────────────────────────────────
// Two offset yellow circles inside a rectangle
static void DrawHandIcon(float cx, float cy, float size) {
    float w = size * 0.9f;
    float h = size * 0.7f;
    Rectangle r = { cx - w/2, cy - h/2, w, h };
    DrawRectangleRounded(r, 0.3f, 8, (Color){40,40,60,255});
    DrawRectangleRoundedLines(r, 0.3f, 8, (Color){200,200,220,255});
    float off = size * 0.12f;
    DrawCircle(cx - off, cy - off*0.3f, size * 0.18f, (Color){255,220,100,255});
    DrawCircle(cx + off, cy + off*0.3f, size * 0.18f, (Color){255,200,80,255});
}

// ─── DRAW: FRACTAL SQUARE ICON ────────────────────────────
// Recursive 4-quadrant squares, small & big nested
static void DrawFractalSquares(float cx, float cy, float size, int depth, Color col) {
    if (depth <= 0 || size < 2) return;
    Rectangle r = { cx - size/2, cy - size/2, size, size };
    DrawRectangleLinesEx(r, 1.5f, col);
    float half = size * 0.5f;
    DrawFractalSquares(cx - half/2, cy - half/2, half * 0.8f, depth-1, col);
    DrawFractalSquares(cx + half/2, cy + half/2, half * 0.8f, depth-1, col);
}

// ─── DRAW: PICKAXE ICON ───────────────────────────────────
static void DrawPickaxeIcon(float cx, float cy, float size) {
    // diagonal handle
    DrawLineEx((Vector2){cx - size*0.3f, cy + size*0.3f},
               (Vector2){cx + size*0.3f, cy - size*0.3f},
               size * 0.1f, (Color){140,100,60,255});
    // head
    DrawRectangleRounded((Rectangle){cx + size*0.1f, cy - size*0.4f, size*0.35f, size*0.18f},
                         0.5f, 6, (Color){200,200,210,255});
}

// ─── DRAW: ITEM ICON (dispatches) ─────────────────────────
static void DrawItemIcon(ItemID id, float cx, float cy, float size) {
    switch (id) {
        case ITEM_HANDS:   DrawHandIcon(cx, cy, size); return;
        case ITEM_PICKAXE: DrawPickaxeIcon(cx, cy, size); return;
        case ITEM_WOOD: {
            DrawCircle(cx, cy, size*0.35f, BROWN);
            DrawCircle(cx, cy, size*0.18f, DARKBROWN);
        } return;
        case ITEM_STONE:
            DrawPoly((Vector2){cx,cy}, 6, size*0.35f, 30.0f, DARKGRAY);
            return;
        default: {
            Color c = ItemColors[id];
            DrawRectangle(cx - size*0.3f, cy - size*0.3f, size*0.6f, size*0.6f, c);
            DrawRectangleLines(cx - size*0.3f, cy - size*0.3f, size*0.6f, size*0.6f, WHITE);
        } return;
    }
}

// ─── TOP BAR: gear button + menu ─────────────────────────
// Returns a rectangle for the gear button, so main can detect clicks
static Rectangle DrawSettingsGear(void) {
    int sw = GetScreenWidth();
    Rectangle btn = { sw - 60.0f, 15.0f, 45.0f, 45.0f };
    Color hover = CheckCollisionPointRec(GetMousePosition(), btn)
                  ? (Color){80,140,220,255} : (Color){40,40,60,220};
    DrawRectangleRounded(btn, 0.25f, 8, hover);
    DrawRectangleRoundedLines(btn, 0.25f, 8, WHITE);

    if (uiSettingsIconLoaded) {
        // Draw texture scaled to fit
        Rectangle src = {0,0,(float)uiSettingsIcon.width,(float)uiSettingsIcon.height};
        Rectangle dst = { btn.x + 6, btn.y + 6, btn.width - 12, btn.height - 12 };
        DrawTexturePro(uiSettingsIcon, src, dst, (Vector2){0,0}, 0, WHITE);
    } else {
        // Procedural gear
        float cx = btn.x + btn.width/2, cy = btn.y + btn.height/2;
        float t = (float)GetTime();
        for (int i = 0; i < 8; i++) {
            float a = t*0.4f + i * (PI/4);
            Vector2 p1 = {cx + cosf(a)*10, cy + sinf(a)*10};
            Vector2 p2 = {cx + cosf(a)*16, cy + sinf(a)*16};
            DrawLineEx(p1, p2, 3, WHITE);
        }
        DrawCircle(cx, cy, 7, WHITE);
        DrawCircle(cx, cy, 3, (Color){40,40,60,255});
    }
    return btn;
}

// ─── HOTBAR ──────────────────────────────────────────────
// Slot 0 is Hands, rounded black slot. Other slots grow centered.
// Right side has a permanent fractal-square "debug" button.
// Returns bounds of the debug button for click detection.
static Rectangle DrawHotbar(Player *p) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float slot = 68.0f * uiHotbarSlotScale;
    float gap  = 6.0f;
    int   count = p->hotbarCount;

    // Width = N slots + gaps
    float barW = count * slot + (count - 1) * gap;
    // Debug fractal button sits right of the hotbar always
    float dbgW = slot * 0.65f;
    float totalW = barW + dbgW + gap * 2;

    float cxOffset = uiHotbarCenterBias * 200.0f;
    float startX = (sw - barW) * 0.5f + cxOffset - (dbgW + gap) * 0.5f;
    float y = sh - slot - 25.0f;

    unsigned char alpha = (unsigned char)(uiHotbarOpacity * 255);

    for (int i = 0; i < count; i++) {
        float x = startX + i * (slot + gap);
        Rectangle sr = { x, y, slot, slot };
        bool sel = (i == p->hotbarSel);

        Color bg = sel ? (Color){45,80,130,alpha} : (Color){15,15,25,alpha};
        DrawRectangleRounded(sr, uiHotbarCornerRadius, 10, bg);
        DrawRectangleRoundedLines(sr, uiHotbarCornerRadius, 10,
                                  sel ? (Color){120,200,255,255} : (Color){60,60,80,200});

        ItemID id = p->hotbarItem[i];
        if (id != ITEM_NONE) {
            DrawItemIcon(id, x + slot/2, y + slot/2, slot);
            int cnt = (id == ITEM_HANDS) ? -1 : PlayerCountItem(p, id);
            if (cnt > 0 && uiHotbarShowNumbers) {
                DrawText(TextFormat("%d", cnt),
                         x + slot - 22, y + slot - 20, 16, WHITE);
            }
            if (uiHotbarShowNames) {
                const char *nm = ItemNames[id];
                int tw = MeasureText(nm, 12);
                DrawText(nm, x + slot/2 - tw/2, y + slot + 2, 12, LIGHTGRAY);
            }
        }

        if (sel) {
            DrawText(TextFormat("%d", i+1), x + 4, y + 2, 14, GOLD);
        }
    }

    // Debug fractal button on the right
    float dbgX = startX + barW + gap*2;
    Rectangle dbg = { dbgX, y, dbgW, slot };
    bool dbgHover = CheckCollisionPointRec(GetMousePosition(), dbg);
    DrawRectangleRounded(dbg, uiHotbarCornerRadius, 10,
                         dbgHover ? (Color){60,60,100,alpha} : (Color){20,20,35,alpha});
    DrawRectangleRoundedLines(dbg, uiHotbarCornerRadius, 10,
                              dbgHover ? (Color){180,220,255,255} : (Color){80,80,120,200});
    DrawFractalSquares(dbgX + dbgW/2, y + slot/2, slot * 0.7f, 3,
                       (Color){180,220,255,255});

    return dbg;
}

// ─── CRAFTING MENU ────────────────────────────────────────
static void DrawCraftMenu(Player *p) {
    if (!p->craftMenuOpen) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int mw = 680, mh = 560;
    int mx = sw/2 - mw/2, my = sh/2 - mh/2;

    DrawRectangle(0, 0, sw, sh, (Color){0,0,0,140});
    DrawRectangle(mx, my, mw, mh, (Color){10,10,30,240});
    DrawRectangleLines(mx, my, mw, mh, (Color){0,180,255,255});
    DrawText("CRAFTING", mx+20, my+15, 36, SKYBLUE);
    DrawText("Combine materials into new items", mx+20, my+58, 18, GRAY);
    DrawLine(mx, my+90, mx+mw, my+90, (Color){0,120,180,180});

    for (int i = 0; i < RECIPE_COUNT; i++) {
        Recipe *r = &Recipes[i];
        int ry = my + 110 + i * 70;
        bool sel = (i == p->craftSel);
        DrawRectangle(mx+10, ry, mw-20, 63,
                      sel ? (Color){20,60,100,255} : (Color){15,15,40,200});
        DrawRectangleLines(mx+10, ry, mw-20, 63,
                           sel ? SKYBLUE : (Color){50,50,80,255});

        DrawItemIcon(r->output, mx+45, ry+32, 50);
        DrawText(ItemNames[r->output], mx+85, ry+8, 24, WHITE);
        DrawText(TextFormat("x%d", r->outputCount), mx+85, ry+34, 18, GOLD);

        char inputStr[128] = "";
        if (r->inputA != ITEM_NONE)
            sprintf(inputStr, "%s x%d", ItemNames[r->inputA], r->countA);
        if (r->inputB != ITEM_NONE) {
            char b[64];
            sprintf(b, "  +  %s x%d", ItemNames[r->inputB], r->countB);
            strcat(inputStr, b);
        }
        bool canCraft = 1;
        if (r->inputA != ITEM_NONE && PlayerCountItem(p, r->inputA) < r->countA) canCraft = 0;
        if (r->inputB != ITEM_NONE && PlayerCountItem(p, r->inputB) < r->countB) canCraft = 0;
        DrawText(inputStr, mx+260, ry+22, 18, canCraft ? GREEN : RED);
        if (sel) DrawText("[ENTER] Craft", mx+mw-170, ry+22, 18, GOLD);
    }

    DrawText("[UP/DOWN] Navigate   [ENTER] Craft   [TAB] Close",
             mx+20, my+mh-35, 20, GRAY);
}

// ─── DEBUG INVENTORY MENU ─────────────────────────────────
static void DrawDebugMenu(Player *p) {
    (void)p;
    if (!uiDebugMenuOpen) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int mw = 480, mh = 440;
    int mx = sw - mw - 30, my = sh/2 - mh/2;

    DrawRectangle(mx, my, mw, mh, (Color){10,10,30,240});
    DrawRectangleLines(mx, my, mw, mh, (Color){180,220,255,255});
    DrawText("INVENTORY DEBUG", mx+20, my+15, 28, (Color){180,220,255,255});
    DrawText("Tune hotbar appearance in real time", mx+20, my+48, 16, GRAY);

    int row = my + 85;

    // Slot scale
    DrawText(TextFormat("Slot Scale: %.2f", uiHotbarSlotScale), mx+20, row, 18, WHITE);
    Rectangle minus1 = {mx+240, row-4, 30, 26};
    Rectangle plus1  = {mx+280, row-4, 30, 26};
    DrawRectangleRec(minus1, (Color){60,60,80,255});
    DrawRectangleRec(plus1,  (Color){60,60,80,255});
    DrawText("-", minus1.x+11, minus1.y+4, 18, WHITE);
    DrawText("+", plus1.x+11,  plus1.y+4,  18, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), minus1) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiHotbarSlotScale = fmaxf(0.5f, uiHotbarSlotScale - 0.1f);
    if (CheckCollisionPointRec(GetMousePosition(), plus1) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiHotbarSlotScale = fminf(2.0f, uiHotbarSlotScale + 0.1f);
    row += 40;

    // Opacity
    DrawText(TextFormat("Opacity: %.2f", uiHotbarOpacity), mx+20, row, 18, WHITE);
    Rectangle minus2 = {mx+240, row-4, 30, 26};
    Rectangle plus2  = {mx+280, row-4, 30, 26};
    DrawRectangleRec(minus2, (Color){60,60,80,255});
    DrawRectangleRec(plus2,  (Color){60,60,80,255});
    DrawText("-", minus2.x+11, minus2.y+4, 18, WHITE);
    DrawText("+", plus2.x+11,  plus2.y+4,  18, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), minus2) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiHotbarOpacity = fmaxf(0.1f, uiHotbarOpacity - 0.1f);
    if (CheckCollisionPointRec(GetMousePosition(), plus2) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiHotbarOpacity = fminf(1.0f, uiHotbarOpacity + 0.1f);
    row += 40;

    // Center bias
    DrawText(TextFormat("Center Bias: %.2f", uiHotbarCenterBias), mx+20, row, 18, WHITE);
    Rectangle minus3 = {mx+240, row-4, 30, 26};
    Rectangle plus3  = {mx+280, row-4, 30, 26};
    DrawRectangleRec(minus3, (Color){60,60,80,255});
    DrawRectangleRec(plus3,  (Color){60,60,80,255});
    DrawText("-", minus3.x+11, minus3.y+4, 18, WHITE);
    DrawText("+", plus3.x+11,  plus3.y+4,  18, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), minus3) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiHotbarCenterBias = fmaxf(-1.0f, uiHotbarCenterBias - 0.1f);
    if (CheckCollisionPointRec(GetMousePosition(), plus3) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiHotbarCenterBias = fminf(1.0f, uiHotbarCenterBias + 0.1f);
    row += 40;

    // Corner radius
    DrawText(TextFormat("Corner Radius: %.2f", uiHotbarCornerRadius), mx+20, row, 18, WHITE);
    Rectangle minus4 = {mx+240, row-4, 30, 26};
    Rectangle plus4  = {mx+280, row-4, 30, 26};
    DrawRectangleRec(minus4, (Color){60,60,80,255});
    DrawRectangleRec(plus4,  (Color){60,60,80,255});
    DrawText("-", minus4.x+11, minus4.y+4, 18, WHITE);
    DrawText("+", plus4.x+11,  plus4.y+4,  18, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), minus4) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiHotbarCornerRadius = fmaxf(0.0f, uiHotbarCornerRadius - 0.05f);
    if (CheckCollisionPointRec(GetMousePosition(), plus4) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiHotbarCornerRadius = fminf(1.0f, uiHotbarCornerRadius + 0.05f);
    row += 50;

    // Toggles
    Rectangle t1 = {mx+20, row, 200, 28};
    DrawRectangleRec(t1, uiHotbarShowNumbers ? (Color){40,120,60,255} : (Color){80,40,40,255});
    DrawText(uiHotbarShowNumbers ? "Show Numbers: ON" : "Show Numbers: OFF",
             t1.x+8, t1.y+6, 16, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), t1) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiHotbarShowNumbers = !uiHotbarShowNumbers;
    row += 38;

    Rectangle t2 = {mx+20, row, 200, 28};
    DrawRectangleRec(t2, uiHotbarShowNames ? (Color){40,120,60,255} : (Color){80,40,40,255});
    DrawText(uiHotbarShowNames ? "Show Names: ON" : "Show Names: OFF",
             t2.x+8, t2.y+6, 16, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), t2) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        uiHotbarShowNames = !uiHotbarShowNames;
    row += 45;

    Rectangle reset = {mx+20, row, 120, 30};
    DrawRectangleRec(reset, (Color){100,60,60,255});
    DrawText("RESET", reset.x+28, reset.y+7, 18, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), reset) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        uiHotbarSlotScale = 1.0f;
        uiHotbarOpacity = 0.85f;
        uiHotbarCenterBias = 0.0f;
        uiHotbarCornerRadius = 0.35f;
        uiHotbarShowNumbers = true;
        uiHotbarShowNames = false;
    }

    DrawText("(click the fractal icon to close)", mx+20, my+mh-30, 14, GRAY);
}

// ─── SETTINGS MENU ────────────────────────────────────────
// Returns a code: 0 = nothing, 1 = show-controls, 2 = save, 3 = exit-to-menu, 4 = close
static int DrawSettingsMenu(void) {
    if (!uiSettingsMenuOpen) return 0;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int mw = 420, mh = 340;
    int mx = sw/2 - mw/2, my = sh/2 - mh/2;

    DrawRectangle(0, 0, sw, sh, (Color){0,0,0,160});
    DrawRectangle(mx, my, mw, mh, (Color){10,10,30,240});
    DrawRectangleLines(mx, my, mw, mh, WHITE);
    DrawText("SETTINGS", mx+20, my+20, 36, WHITE);

    int retCode = 0;
    int row = my + 90;
    const char *labels[] = { "Show Controls", "Save World", "Exit to Menu", "Close" };
    for (int i = 0; i < 4; i++) {
        Rectangle b = {mx+30, row + i*55.0f, mw-60, 44};
        bool hover = CheckCollisionPointRec(GetMousePosition(), b);
        DrawRectangleRec(b, hover ? (Color){60,100,160,255} : (Color){30,40,70,255});
        DrawRectangleLinesEx(b, 2, hover ? WHITE : (Color){100,140,200,255});
        DrawText(labels[i], b.x + 20, b.y + 12, 22, WHITE);
        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) retCode = i + 1;
    }
    return retCode;
}

// ─── CONTROLS OVERLAY ─────────────────────────────────────
static bool uiControlsOverlayOpen = false;
static void DrawControlsOverlay(void) {
    if (!uiControlsOverlayOpen) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int mw = 520, mh = 440;
    int mx = sw/2 - mw/2, my = sh/2 - mh/2;
    DrawRectangle(0,0,sw,sh,(Color){0,0,0,180});
    DrawRectangle(mx, my, mw, mh, (Color){10,15,40,245});
    DrawRectangleLines(mx, my, mw, mh, WHITE);
    DrawText("CONTROLS", mx+20, my+20, 32, GOLD);

    const char *lines[] = {
        "WASD          Move",
        "LMB (hold)    Mine block",
        "RMB           Place selected item",
        "1-7           Select hotbar slot",
        "Mouse Wheel   Scroll hotbar",
        "TAB           Crafting menu",
        "E             Enter 3D Base (on pad)",
        "F11           Toggle fullscreen",
        "ESC           Close / pause",
        "Click Gear    Settings menu",
        "Click Fractal Inventory debug",
    };
    int n = sizeof(lines)/sizeof(lines[0]);
    for (int i = 0; i < n; i++)
        DrawText(lines[i], mx+30, my+75 + i*28, 20, WHITE);

    DrawText("[click anywhere to close]", mx+20, my+mh-30, 16, GRAY);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) uiControlsOverlayOpen = false;
}

// ─── BLUEPRINT UI ─────────────────────────────────────────
typedef struct {
    bool   active;
    char   code[40];
    int    capX, capY;
    bool   pasteMode;
    int    woodCost, stoneCost;
} BlueprintUI;

static BlueprintUI bpUI = {0};

static void BlueprintDraw(Player *p) {
    if (!bpUI.active) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int mw = 640, mh = 260;
    int mx = sw/2 - mw/2, my = sh/2 - mh/2;
    DrawRectangle(mx, my, mw, mh, (Color){10,20,10,240});
    DrawRectangleLines(mx, my, mw, mh, (Color){0,255,100,255});
    DrawText("BLUEPRINT", mx+20, my+15, 30, (Color){0,255,100,255});
    DrawText("Share structures as codes", mx+20, my+50, 16, GRAY);

    if (bpUI.code[0]) {
        DrawText("CODE:", mx+20, my+85, 20, WHITE);
        DrawRectangle(mx+90, my+82, mw-110, 30, (Color){20,20,20,255});
        DrawText(bpUI.code, mx+100, my+88, 20, GOLD);
    }
    if (bpUI.pasteMode) {
        DrawText(TextFormat("Paste cost: Wood=%d Stone=%d",
                             bpUI.woodCost, bpUI.stoneCost),
                 mx+20, my+130, 20,
                 (PlayerCountItem(p, ITEM_WOOD) >= bpUI.woodCost &&
                  PlayerCountItem(p, ITEM_STONE) >= bpUI.stoneCost) ? GREEN : RED);
    }
    DrawText("[C] Capture  [P] Paste  [B] Close", mx+20, my+mh-32, 18, SKYBLUE);
}

#endif // UI_H
