#ifndef UI_H
#define UI_H
// ============================================================
//  UI.H — everything drawn in SCREEN space (not world space):
//  hotbar, crafting menu, help text.
//
//  Owns: nothing. UI reads the Player and the ITEMS table and
//  draws; it never changes game state. (main.c changes state in
//  response to input — keeping "decide" and "display" separate
//  is what made your color change so safely isolated.)
// ============================================================

#include "raylib.h"
#include "config.h"
#include "gamedata.h"
#include "player.h"

typedef struct {
    Rectangle rect;
    const char *text;
    bool hovered;
    bool clicked;
} UIButton;

typedef struct {
    int panelX;
    int panelY;
    int panelW;
    int panelH;
    int slotSize;
    int gap;
    int startX;
    int startY;
} InventoryLayout;

static void UiDrawButton(const UIButton *button) {
    Color bg = button->hovered ? (Color){70, 110, 180, 255} : (Color){35, 55, 90, 220};
    Color border = button->hovered ? WHITE : SKYBLUE;

    DrawRectangleRounded(button->rect, 0.25f, 8, bg);
    DrawRectangleRoundedLines(button->rect, 0.25f, 8, border);

    int fontSize = 24;
    int textWidth = MeasureText(button->text, fontSize);
    int textX = (int)button->rect.x + (int)((button->rect.width - textWidth) / 2.0f);
    int textY = (int)button->rect.y + (int)((button->rect.height - fontSize) / 2.0f);
    DrawText(button->text, textX, textY, fontSize, WHITE);
}

static bool UiButtonUpdate(UIButton *button) {
    Vector2 mouse = GetMousePosition();
    button->hovered = CheckCollisionPointRec(mouse, button->rect);
    button->clicked = button->hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    return button->clicked;
}

// ─── Hotbar ───────────────────────────────────────────────
// The hotbar grows as new item types are discovered, up to 7 slots.
static void UiDrawHotbar(const Player *p) {
    const float slot = 64, gap = 6;
    float barW = p->slotCount * slot + (p->slotCount > 0 ? (p->slotCount - 1) * gap : 0);
    float x = (GetScreenWidth() - barW) / 2.0f;
    float y = GetScreenHeight() - slot - 20.0f;

    for (int s = 0; s < p->slotCount; s++) {
        ItemID id = (s < INVENTORY_SIZE) ? p->inventorySlots[s] : ITEM_NONE;
        int amount = (s < INVENTORY_SIZE) ? p->inventoryAmounts[s] : 0;
        Rectangle r = { x, y, slot, slot };
        bool sel = (s == p->selectedSlot);

        DrawRectangleRounded(r, 0.3f, 8,
            sel ? (Color){45,80,130,230} : (Color){15,15,25,200});
        DrawRectangleRoundedLines(r, 0.3f, 8,
            sel ? (Color){120,200,255,255} : (Color){70,70,90,200});

        if (id != ITEM_NONE) {
            Color c = ITEMS[id].color;
            if (p->inventory[id] == 0) c = (Color){60,60,60,255};
            DrawRectangle((int)(x+slot/2-14), (int)(y+slot/2-14), 28, 28, c);
            DrawText(TextFormat("%d", s + 1), (int)x+5, (int)y+4, 14, GOLD);
            DrawText(TextFormat("%d", amount),
                     (int)(x+slot-24), (int)(y+slot-20), 15, WHITE);
        } else {
            DrawText(TextFormat("%d", s + 1), (int)x+5, (int)y+4, 14, GOLD);
        }

        x += slot + gap;
    }
}

static void UiGetInventoryLayout(const Player *p, InventoryLayout *layout) {
    (void)p;
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    int panelW = sw * 90 / 100;
    int panelH = sh * 86 / 100;
    if (panelW > 840) panelW = 840;
    if (panelH > 720) panelH = 720;
    if (panelW < 360) panelW = 360;
    if (panelH < 440) panelH = 440;

    int px = (sw - panelW) / 2;
    int py = (sh - panelH) / 2;
    int padding = 24;
    int headerH = 72;
    int detailH = 96;
    int gap = 8;
    int usableW = panelW - padding * 2;
    int usableH = panelH - headerH - detailH - padding * 2 - 12;

    int slotW = (usableW - gap * (INVENTORY_COLS - 1)) / INVENTORY_COLS;
    int slotH = (usableH - gap * (INVENTORY_ROWS - 1)) / INVENTORY_ROWS;
    int slotSize = slotW < slotH ? slotW : slotH;
    if (slotSize < 18) slotSize = 18;

    int gridW = slotSize * INVENTORY_COLS + gap * (INVENTORY_COLS - 1);
    int startX = px + (panelW - gridW) / 2;
    int startY = py + 78;

    layout->panelX = px;
    layout->panelY = py;
    layout->panelW = panelW;
    layout->panelH = panelH;
    layout->slotSize = slotSize;
    layout->gap = gap;
    layout->startX = startX;
    layout->startY = startY;
}

static void UiDrawInventory(const Player *p) {
    if (!p->inventoryOpen) return;

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    InventoryLayout layout = { 0 };
    UiGetInventoryLayout(p, &layout);

    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 180});
    DrawRectangleRounded((Rectangle){ (float)layout.panelX, (float)layout.panelY, (float)layout.panelW, (float)layout.panelH }, 0.08f, 12, (Color){18, 22, 35, 245});
    DrawRectangleRoundedLines((Rectangle){ (float)layout.panelX, (float)layout.panelY, (float)layout.panelW, (float)layout.panelH }, 0.08f, 12, (Color){120, 200, 255, 220});

    DrawText("BACKPACK", layout.panelX + 28, layout.panelY + 22, 32, SKYBLUE);
    DrawText("[E] close", layout.panelX + layout.panelW - 140, layout.panelY + 24, 20, GRAY);

    for (int r = 0; r < INVENTORY_ROWS; r++) {
        for (int c = 0; c < INVENTORY_COLS; c++) {
            int idx = r * INVENTORY_COLS + c;
            int x = layout.startX + c * (layout.slotSize + layout.gap);
            int y = layout.startY + r * (layout.slotSize + layout.gap);
            Rectangle rect = { (float)x, (float)y, (float)layout.slotSize, (float)layout.slotSize };
            bool selected = (idx == p->inventoryCursor);
            bool dragging = p->inventoryDragging && idx == p->inventoryDragIndex;
            bool usable = idx < HOTBAR_MAX_SLOTS;

            DrawRectangleRounded(rect, 0.2f, 8, dragging ? (Color){140, 90, 35, 255} : (selected ? (Color){80, 110, 180, 255} : (Color){32, 38, 54, 240}));
            DrawRectangleRoundedLines(rect, 0.2f, 8, dragging ? GOLD : (selected ? GOLD : (usable ? SKYBLUE : (Color){100, 110, 140, 220})));

            if (p->inventorySlots[idx] != ITEM_NONE && p->inventoryAmounts[idx] > 0) {
                ItemID id = p->inventorySlots[idx];
                Color c = ITEMS[id].color;
                DrawRectangle(x + layout.slotSize / 6, y + layout.slotSize / 6, layout.slotSize - layout.slotSize / 3, layout.slotSize - layout.slotSize / 3, c);
                DrawText(TextFormat("%d", p->inventoryAmounts[idx]), x + 8, y + layout.slotSize - 22, 16, WHITE);
            }
            if (idx < HOTBAR_MAX_SLOTS) {
                DrawText(TextFormat("%d", idx + 1), x + 8, y + 6, 14, GOLD);
            }
        }
    }

    if (p->inventoryDragging && p->inventoryDragIndex >= 0) {
        Vector2 mouse = GetMousePosition();
        ItemID id = p->inventorySlots[p->inventoryDragIndex];
        if (id != ITEM_NONE && p->inventoryAmounts[p->inventoryDragIndex] > 0) {
            int ghostSize = layout.slotSize - 6;
            DrawRectangleRounded((Rectangle){ mouse.x - ghostSize / 2.0f, mouse.y - ghostSize / 2.0f, (float)ghostSize, (float)ghostSize }, 0.2f, 8, (Color){255, 255, 255, 80});
            DrawRectangle((int)(mouse.x - ghostSize / 2.0f) + 8, (int)(mouse.y - ghostSize / 2.0f) + 8, ghostSize - 16, ghostSize - 16, ITEMS[id].color);
        }
    }

    int detailX = layout.panelX + 28;
    int detailY = layout.panelY + layout.panelH - 118;
    DrawRectangleRounded((Rectangle){ detailX, detailY, layout.panelW - 56, 88 }, 0.08f, 10, (Color){26, 30, 44, 240});

    ItemID selectedItem = ITEM_NONE;
    int selectedAmount = 0;
    if (p->inventoryCursor >= 0 && p->inventoryCursor < INVENTORY_SIZE) {
        selectedItem = p->inventorySlots[p->inventoryCursor];
        selectedAmount = p->inventoryAmounts[p->inventoryCursor];
    }

    if (selectedItem != ITEM_NONE) {
        DrawText(TextFormat("Selected: %s", ITEMS[selectedItem].name), detailX + 16, detailY + 16, 24, WHITE);
        DrawText(TextFormat("Stack: %d", selectedAmount), detailX + 16, detailY + 48, 20, LIGHTGRAY);
        DrawText("[Drag and drop]", detailX + 16, detailY + 68, 16, SKYBLUE);
    } else {
        DrawText("Selected: empty", detailX + 16, detailY + 16, 24, LIGHTGRAY);
        DrawText("Fill the backpack by mining and crafting.", detailX + 16, detailY + 48, 18, GRAY);
    }
}

// ─── Crafting menu (TAB) ──────────────────────────────────
static void UiDrawCraftMenu(const Player *p) {
    if (!p->craftMenuOpen) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int mw = 560, mh = 400;
    int mx = (sw-mw)/2, my = (sh-mh)/2;

    DrawRectangle(0, 0, sw, sh, (Color){0,0,0,140});     // dim the game
    DrawRectangle(mx, my, mw, mh, (Color){10,10,30,240});
    DrawRectangleLines(mx, my, mw, mh, SKYBLUE);
    DrawText("CRAFTING", mx+20, my+15, 32, SKYBLUE);

    int row = 0;
    for (ItemID id = 1; id < ITEM_COUNT; id++) {
        if (ITEMS[id].inA == ITEM_NONE) continue;        // not craftable
        const ItemInfo *it = &ITEMS[id];
        int  ry  = my + 70 + row * 64;
        bool sel = (row == p->craftSel);
        bool ok  = PlayerCanCraft(p, id);

        DrawRectangle(mx+12, ry, mw-24, 56,
                      sel ? (Color){20,60,100,255} : (Color){15,15,40,200});
        DrawRectangle(mx+24, ry+14, 28, 28, it->color);
        DrawText(TextFormat("%s x%d", it->name, it->yield),
                 mx+64, ry+8, 22, WHITE);

        // Build the "costs" line. Ingredient names come from the
        // table too — no item is ever named in UI code.
        const char *cost = TextFormat("%d %s", it->nA, ITEMS[it->inA].name);
        if (it->inB != ITEM_NONE)
            cost = TextFormat("%s + %d %s", cost, it->nB, ITEMS[it->inB].name);
        DrawText(cost, mx+64, ry+32, 18, ok ? GREEN : RED);

        if (sel) DrawText("[ENTER]", mx+mw-110, ry+18, 20, GOLD);
        row++;
    }
    DrawText("[UP/DOWN] select   [ENTER] craft   [TAB] close",
             mx+20, my+mh-32, 18, GRAY);
}

// ─── Help line at top of screen ───────────────────────────
static void UiDrawHelp(void) {
    DrawText("WASD move | LMB mine | RMB place | 1-7 select | E backpack | TAB craft | F5 save | F9 load",
             15, 12, 18, (Color){255,255,255,180});
}

#endif // UI_H
