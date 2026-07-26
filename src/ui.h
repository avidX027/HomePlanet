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

// Craft-menu geometry, shared by drawing (here) and click handling
// (main.c). ONE function computes it so the clickable rectangles
// always match the drawn pixels — same idea as InventoryLayout.
// The menu is a GRID of item cells (scrollable by row) with a
// detail box underneath for the selected recipe — so long recipe
// text lives in a fixed box instead of running off the screen.
typedef struct {
    int x, y;           // top-left of the panel
    int w, h;           // panel size
    int cols;           // grid columns
    int cellSize;       // px per cell (shrinks on small windows)
    int gap;
    int gridX, gridY;   // top-left of the cell grid
    int visibleRows;    // grid rows on screen at once
    int detailY;        // top of the recipe-detail box
    int detailH;
} CraftLayout;

static void UiGetCraftLayout(CraftLayout *layout) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    layout->cols = 6;
    layout->gap = 8;
    layout->cellSize = 62;
    layout->visibleRows = 4;

    // Shrink cells rather than let the panel spill off small windows.
    int maxW = sw - 24;
    int need = layout->cols * layout->cellSize + (layout->cols - 1) * layout->gap + 48;
    if (need > maxW) {
        layout->cellSize = (maxW - 48 - (layout->cols - 1) * layout->gap) / layout->cols;
        if (layout->cellSize < 34) layout->cellSize = 34;
    }
    int gridW = layout->cols * layout->cellSize + (layout->cols - 1) * layout->gap;
    layout->w = gridW + 48;
    layout->detailH = 104;
    layout->h = 52 + layout->visibleRows * (layout->cellSize + layout->gap) + layout->detailH + 44;
    if (layout->h > sh - 16) layout->h = sh - 16;
    layout->x = (sw - layout->w) / 2;
    layout->y = (sh - layout->h) / 2;
    layout->gridX = layout->x + 24;
    layout->gridY = layout->y + 52;
    layout->detailY = layout->y + layout->h - layout->detailH - 34;
}

// Screen rectangle of the cell at visible row r, column c.
static Rectangle UiCraftCellRect(const CraftLayout *layout, int r, int c) {
    return (Rectangle){ (float)(layout->gridX + c * (layout->cellSize + layout->gap)),
                        (float)(layout->gridY + r * (layout->cellSize + layout->gap)),
                        (float)layout->cellSize, (float)layout->cellSize };
}

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

// Screen rectangle of hotbar slot `s`. Shared by drawing (below)
// and main.c's click handling — one source of truth for the layout
// (64px slots, 6px gaps, centered, 20px above the bottom).
static Rectangle UiHotbarSlotRect(const Player *p, int s) {
    const float slot = 64, gap = 6;
    float barW = p->slotCount * slot + (p->slotCount > 0 ? (p->slotCount - 1) * gap : 0);
    float x = (GetScreenWidth() - barW) / 2.0f + s * (slot + gap);
    float y = GetScreenHeight() - slot - 20.0f;
    return (Rectangle){ x, y, slot, slot };
}

// Which hotbar slot is `point` over? -1 = none. main.c uses this to
// select slots on click AND to stop those clicks falling through
// into the world (mining the tile hidden under the hotbar).
static int UiHotbarSlotAt(const Player *p, Vector2 point) {
    for (int s = 0; s < p->slotCount; s++) {
        if (CheckCollisionPointRec(point, UiHotbarSlotRect(p, s))) return s;
    }
    return -1;
}

static void UiDrawHotbar(const Player *p) {
    for (int s = 0; s < p->slotCount; s++) {
        ItemID id = (s < INVENTORY_SIZE) ? p->inventorySlots[s] : ITEM_NONE;
        int amount = (s < INVENTORY_SIZE) ? p->inventoryAmounts[s] : 0;
        Rectangle r = UiHotbarSlotRect(p, s);
        float x = r.x, y = r.y, slot = r.width;
        bool sel = (s == p->selectedSlot);

        DrawRectangleRounded(r, 0.3f, 8,
            sel ? (Color){45,80,130,230} : (Color){15,15,25,200});
        DrawRectangleRoundedLines(r, 0.3f, 8,
            sel ? (Color){120,200,255,255} : (Color){70,70,90,200});

        DrawText(TextFormat("%d", s + 1), (int)x+5, (int)y+4, 14, GOLD);
        if (id != ITEM_NONE) {
            DrawItemSprite(id, x + slot/2 - 16, y + slot/2 - 16, 32);
            // Owning zero of it? Dim the sprite with a dark veil.
            if (p->inventory[id] == 0)
                DrawRectangle((int)(x+slot/2-16), (int)(y+slot/2-16), 32, 32, (Color){10,10,14,170});
            DrawText(TextFormat("%d", amount),
                     (int)(x+slot-24), (int)(y+slot-20), 15, WHITE);
        }
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
    DrawText("[E/ESC] close", layout.panelX + layout.panelW - 170, layout.panelY + 24, 20, GRAY);

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
                int icon = layout.slotSize * 2 / 3;
                DrawItemSprite(id, x + (layout.slotSize - icon) / 2.0f,
                                   y + (layout.slotSize - icon) / 2.0f, (float)icon);
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
            DrawItemSprite(id, mouse.x - ghostSize / 2.0f + 6, mouse.y - ghostSize / 2.0f + 6, (float)(ghostSize - 12));
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
        DrawItemSprite(selectedItem, (float)(detailX + layout.panelW - 56 - 80), (float)(detailY + 12), 64);
    } else {
        DrawText("Selected: empty", detailX + 16, detailY + 16, 24, LIGHTGRAY);
        DrawText("Fill the backpack by mining and crafting.", detailX + 16, detailY + 48, 18, GRAY);
    }
}

// ─── Crafting menu (TAB) — a scrollable GRID of recipes ───
static void UiDrawCraftMenu(const Player *p) {
    if (!p->craftMenuOpen) return;
    CraftLayout layout = { 0 };
    UiGetCraftLayout(&layout);
    int mx = layout.x, my = layout.y, mw = layout.w, mh = layout.h;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0,0,0,140});  // dim the game
    DrawRectangle(mx, my, mw, mh, (Color){10,10,30,240});
    DrawRectangleLines(mx, my, mw, mh, SKYBLUE);
    DrawText("CRAFTING", mx+20, my+14, 28, SKYBLUE);

    int n = CraftableCount();
    int rows = (n + layout.cols - 1) / layout.cols;   // grid rows total
    int startRow = p->craftScroll;

    // The cells. Border color tells the story at a glance:
    // gold = selected, green = craftable, red = missing materials,
    // dim gray + veil = tech-locked.
    for (int r = 0; r < layout.visibleRows; r++) {
        for (int c = 0; c < layout.cols; c++) {
            int idx = (startRow + r) * layout.cols + c;
            if (idx >= n) break;
            ItemID id = CraftableAtRow(idx);
            Rectangle cell = UiCraftCellRect(&layout, r, c);
            bool sel    = (idx == p->craftSel);
            bool ok     = PlayerCanCraft(p, id);
            bool locked = !PlayerHasTech(p, ITEMS[id].tech);

            DrawRectangleRounded(cell, 0.15f, 4,
                sel ? (Color){26,62,96,255} : (Color){16,18,42,230});
            float icon = cell.width - 18;
            DrawItemSprite(id, cell.x + (cell.width - icon) / 2,
                               cell.y + (cell.height - icon) / 2 - 3, icon);
            if (locked) {
                DrawRectangleRounded(cell, 0.15f, 4, (Color){8,8,16,170});
                DrawText("?", (int)(cell.x + cell.width - 13), (int)cell.y + 3, 16, GOLD);
            } else if (ITEMS[id].yield > 1) {
                DrawText(TextFormat("x%d", ITEMS[id].yield),
                         (int)(cell.x + cell.width - 24), (int)(cell.y + cell.height - 17), 13, RAYWHITE);
            }
            Color border = locked ? (Color){70,70,90,255} : (ok ? GREEN : RED);
            if (sel) border = GOLD;
            DrawRectangleRoundedLines(cell, 0.15f, 4, border);
        }
    }

    // Scrollbar, when there's more than a screenful.
    if (rows > layout.visibleRows) {
        int barX = mx + mw - 14;
        int trackH = layout.visibleRows * (layout.cellSize + layout.gap) - layout.gap;
        DrawRectangle(barX, layout.gridY, 5, trackH, (Color){30,34,60,255});
        float thumbH = trackH * ((float)layout.visibleRows / rows);
        float thumbY = layout.gridY + (trackH - thumbH) * ((float)startRow / (rows - layout.visibleRows));
        DrawRectangle(barX, (int)thumbY, 5, (int)thumbH, SKYBLUE);
    }

    // Detail box: the selected recipe's full story, in a box that
    // owns its space — long names and costs can't leak off screen.
    Rectangle detail = { (float)(mx + 16), (float)layout.detailY,
                         (float)(mw - 32), (float)layout.detailH };
    DrawRectangleRounded(detail, 0.1f, 6, (Color){18,22,48,240});
    if (p->craftSel >= 0 && p->craftSel < n) {
        ItemID id = CraftableAtRow(p->craftSel);
        const ItemInfo *it = &ITEMS[id];
        bool ok     = PlayerCanCraft(p, id);
        bool locked = !PlayerHasTech(p, it->tech);
        int dx = (int)detail.x + 12, dy = (int)detail.y + 10;

        DrawItemSprite(id, (float)dx, (float)dy, 40);
        DrawText(TextFormat("%s  x%d", it->name, it->yield), dx + 52, dy + 2, 20,
                 locked ? GRAY : WHITE);
        if (locked) {
            DrawText(TextFormat("LOCKED — research %s", TECHS[it->tech].name),
                     dx + 52, dy + 26, 15, (Color){220,150,70,255});
        } else {
            // Costs as sprite + count pairs — compact, can't overflow.
            int cx = dx + 52;
            DrawItemSprite(it->inA, (float)cx, (float)(dy + 24), 18);
            DrawText(TextFormat("x%d", it->nA), cx + 22, dy + 27, 15,
                     p->inventory[it->inA] >= it->nA ? GREEN : RED);
            if (it->inB != ITEM_NONE) {
                DrawItemSprite(it->inB, (float)(cx + 66), (float)(dy + 24), 18);
                DrawText(TextFormat("x%d", it->nB), cx + 88, dy + 27, 15,
                         p->inventory[it->inB] >= it->nB ? GREEN : RED);
            }
            if (ok) DrawText("[ENTER] craft", dx + 52, dy + 48, 14, GOLD);
        }
        DrawText(TextFormat("owned: %d", p->inventory[id]),
                 (int)(detail.x + detail.width) - 90, dy + 2, 14, LIGHTGRAY);
    }

    DrawText("[WHEEL] scroll   [CLICK/ENTER] craft   [ESC] close",
             mx + 20, my + mh - 24, 14, GRAY);
}

// ─── Tech tree menu (RMB a Research Computer) ─────────────
// Same shared-layout pattern as the craft menu: one function owns
// the geometry, both input (main.c) and drawing (here) use it.
typedef struct {
    int x, y, w, h;
    int visibleRows;
} TechLayout;

static void UiGetTechLayout(TechLayout *layout) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    layout->w = 660;
    layout->h = 470;
    if (layout->w > sw - 16) layout->w = sw - 16;   // never spill off screen
    if (layout->h > sh - 16) layout->h = sh - 16;
    layout->x = (sw - layout->w) / 2;
    layout->y = (sh - layout->h) / 2;
    layout->visibleRows = 5;
}

static Rectangle UiTechRowRect(const TechLayout *layout, int row) {
    return (Rectangle){ (float)(layout->x + 12), (float)(layout->y + 62 + row * 74),
                        (float)(layout->w - 24), 68.0f };
}

static void UiDrawTechMenu(const Player *p) {
    if (!p->techMenuOpen) return;
    TechLayout layout = { 0 };
    UiGetTechLayout(&layout);
    int mx = layout.x, my = layout.y, mw = layout.w, mh = layout.h;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0,0,0,150});
    DrawRectangle(mx, my, mw, mh, (Color){8,16,26,245});
    DrawRectangleLines(mx, my, mw, mh, (Color){80,200,230,255});
    DrawText("RESEARCH TERMINAL", mx+20, my+15, 30, (Color){80,200,230,255});

    int total = TECH_COUNT - 1;   // rows map to TechID row+1 (skip TECH_NONE)
    int startRow = p->techScroll;
    int endRow = startRow + layout.visibleRows;
    if (endRow > total) endRow = total;

    for (int rIdx = startRow; rIdx < endRow; rIdx++) {
        TechID t = (TechID)(rIdx + 1);
        const TechInfo *ti = &TECHS[t];
        Rectangle rowRect = UiTechRowRect(&layout, rIdx - startRow);
        int ry = (int)rowRect.y;
        bool sel        = (rIdx == p->techSel);
        bool done       = p->techUnlocked[t];
        bool prereqMet  = PlayerHasTech(p, ti->requires);
        bool affordable = PlayerCanResearch(p, t);

        DrawRectangleRec(rowRect, sel ? (Color){18,55,75,255} : (Color){12,24,36,220});
        DrawText(ti->name, (int)rowRect.x + 14, ry + 8, 22,
                 done ? GREEN : (prereqMet ? WHITE : GRAY));
        DrawText(ti->desc, (int)rowRect.x + 14, ry + 34, 15, (Color){140,170,190,255});

        if (done) {
            DrawText("RESEARCHED", (int)(rowRect.x + rowRect.width) - 140, ry + 10, 18, GREEN);
        } else if (!prereqMet) {
            DrawText(TextFormat("REQUIRES %s", TECHS[ti->requires].name),
                     (int)(rowRect.x + rowRect.width) - 230, ry + 10, 16, GRAY);
        } else {
            // Cost, with the ingredient sprites so it reads at a glance.
            int cx = (int)(rowRect.x + rowRect.width) - 250;
            DrawItemSprite(ti->costA, (float)cx, (float)(ry + 8), 20);
            DrawText(TextFormat("x%d", ti->nA), cx + 24, ry + 12, 16,
                     p->inventory[ti->costA] >= ti->nA ? GREEN : RED);
            if (ti->costB != ITEM_NONE) {
                DrawItemSprite(ti->costB, (float)(cx + 70), (float)(ry + 8), 20);
                DrawText(TextFormat("x%d", ti->nB), cx + 94, ry + 12, 16,
                         p->inventory[ti->costB] >= ti->nB ? GREEN : RED);
            }
            if (sel && affordable) {
                DrawText("[ENTER] RESEARCH", (int)(rowRect.x + rowRect.width) - 190, ry + 42, 16, GOLD);
            }
        }
    }
    DrawText("[W/S] select   [ENTER/CLICK] research   [ESC] close",
             mx+20, my+mh-30, 16, GRAY);
}

// ─── Health bar ───────────────────────────────────────────
static void UiDrawHealth(const Player *p) {
    int x = 15, y = GetScreenHeight() - 110;
    int w = 220, h = 20;
    float frac = p->hp / PLAYER_MAX_HP;
    if (frac < 0) frac = 0;
    Color c = frac > 0.5f ? (Color){90,200,90,255} : (frac > 0.25f ? GOLD : RED);
    DrawRectangle(x - 2, y - 2, w + 4, h + 4, (Color){10,10,18,220});
    DrawRectangle(x, y, (int)(w * frac), h, c);
    DrawRectangleLines(x - 2, y - 2, w + 4, h + 4, SKYBLUE);
    DrawText(TextFormat("HP %d/%d", (int)p->hp, (int)PLAYER_MAX_HP), x + 6, y + 3, 14, RAYWHITE);
    if (p->invulnTimer > 0) {
        DrawText("SHIELDED", x + w - 70, y + 3, 14, SKYBLUE);
    }
}

// ─── Help line at top of screen ───────────────────────────
// Just the openers — the full list lives in Settings → Controls.
static void UiDrawHelp(void) {
    DrawText("[E] backpack   [TAB] craft   [G] map   [ESC] menu",
             15, 12, 17, (Color){255,255,255,170});
}

#endif // UI_H
