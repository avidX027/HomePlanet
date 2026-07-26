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
#include "entities.h"   // Machine — the block panels read machine state

typedef struct {
    Rectangle rect;
    const char *text;
    bool hovered;
    bool clicked;
} UIButton;

// ─── Shared panel styling (the Factorio-gray look) ────────
#define UI_PANEL_BG     (Color){  94,  94,  96, 250 }
#define UI_PANEL_BORDER (Color){ 148, 148, 150, 255 }
#define UI_HEADER_BG    (Color){  60,  60,  62, 255 }
#define UI_CONTENT_BG   (Color){  54,  54,  56, 255 }
#define UI_SLOT_BG      (Color){  43,  43,  45, 255 }
#define UI_SLOT_BORDER  (Color){ 102, 102, 106, 255 }
#define UI_SLOT_HOVER   (Color){  72,  72,  76, 255 }
#define UI_ACCENT       (Color){ 255, 166,   2, 255 }   // selection orange
#define UI_HEADER_H     34

// ─── Panel close button ───────────────────────────────────
// The [X] in a header's top-right. Drawing and hit-testing share
// this rectangle, so what you click is what you see — main.c reads
// the same rect to decide whether the click closed the panel.
static Rectangle UiPanelCloseRect(int x, int y, int w) {
    return (Rectangle){ (float)(x + w - 32), (float)(y + 6), 26, 22 };
}

static void UiDrawCloseButton(int x, int y, int w) {
    Rectangle r = UiPanelCloseRect(x, y, w);
    bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, hovered ? (Color){ 170, 62, 62, 255 } : (Color){ 74, 74, 78, 255 });
    DrawRectangleLinesEx(r, 1, hovered ? RAYWHITE : (Color){ 120, 120, 124, 255 });
    DrawText("X", (int)(r.x + 9), (int)(r.y + 3), 17, RAYWHITE);
}

// A titled window frame: outer panel, header bar, content well.
static void UiDrawPanelFrame(int x, int y, int w, int h, const char *title) {
    DrawRectangle(x, y, w, h, UI_PANEL_BG);
    DrawRectangleLines(x, y, w, h, UI_PANEL_BORDER);
    DrawRectangle(x + 3, y + 3, w - 6, UI_HEADER_H - 3, UI_HEADER_BG);
    DrawText(title, x + 12, y + 10, 20, RAYWHITE);
    // decorative header grooves, because Factorio
    int gx = x + 24 + MeasureText(title, 20);
    for (int gy = y + 12; gy <= y + 24; gy += 6) {
        DrawRectangle(gx, gy, w - (gx - x) - 12, 2, (Color){ 80, 80, 82, 255 });
    }
    DrawRectangle(x + 6, y + UI_HEADER_H + 3, w - 12, h - UI_HEADER_H - 9, UI_CONTENT_BG);
}

typedef struct {
    int panelX;
    int panelY;
    int panelW;
    int panelH;
    int slotSize;
    int gap;
    int startX;
    int startY;    // top-left of the 6-row STORAGE grid
    int hotbarY;   // y of the detached bottom row — the hotbar
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
    int cellSize;       // cell width (shrinks on small windows)
    int cellH;          // cell height = width + a strip for the name
    int gap;
    int gridX, gridY;   // top-left of the cell grid
    int visibleRows;    // grid rows on screen at once
    int detailY;        // top of the recipe-detail box
    int detailH;
} CraftLayout;

// ─── Panel docking ────────────────────────────────────────
// Character, Crafting and the Machine panel are DOCKED: whichever
// are open form one centered row, left to right, so having all
// three up reads as a single three-column workspace instead of a
// pile of floating windows.
//
// The trick is ordering: every panel's WIDTH is computed first
// (from a shared budget), and only then are the x positions handed
// out. A panel can't ask "where am I?" before the row knows how
// wide everyone is.
#define UI_DOCK_GAP    8
#define UI_PANEL_INV   0
#define UI_PANEL_CRAFT 1
#define UI_PANEL_MACH  2

static int UiOpenPanelCount(const Player *p) {
    int n = 0;
    if (p->inventoryOpen) n++;
    if (p->craftMenuOpen) n++;
    if (machineUiX >= 0)  n++;
    return n;
}

// The most any one panel may take, so N side by side always fit.
static int UiPanelBudget(const Player *p) {
    int n = UiOpenPanelCount(p);
    if (n < 1) n = 1;
    return (GetScreenWidth() - 24 - (n - 1) * UI_DOCK_GAP) / n;
}

static int UiClampW(int preferred, int minimum, int budget) {
    int w = preferred < budget ? preferred : budget;
    if (w < minimum) w = minimum;
    return w;
}

// Widths — position-independent, so the dock can add them up.
static int UiInventoryPanelW(const Player *p) {
    return UiClampW(470, 260, UiPanelBudget(p));
}
static int UiCraftPanelW(const Player *p) {
    return UiClampW(470, 250, UiPanelBudget(p));
}
static int UiMachinePanelW(const Player *p, const Machine *m) {
    int cols = (m != NULL && m->type == TILE_CHEST) ? 7
             : (m != NULL && m->type == TILE_DRILL) ? 4 : 1;
    int preferred = cols * 44 + (cols - 1) * 6 + 40;
    if (preferred < 250) preferred = 250;
    return UiClampW(preferred, 200, UiPanelBudget(p));
}

// Where does panel `which` start? Sums the widths of the open
// panels to its left and centers the whole row on screen.
static int UiDockX(const Player *p, int which, const Machine *m) {
    bool oInv = p->inventoryOpen, oCraft = p->craftMenuOpen, oMach = (machineUiX >= 0);
    int wInv = oInv ? UiInventoryPanelW(p) : 0;
    int wCraft = oCraft ? UiCraftPanelW(p) : 0;
    int wMach = oMach ? UiMachinePanelW(p, m) : 0;

    int total = 0, panels = 0;
    if (oInv)   { total += wInv;   panels++; }
    if (oCraft) { total += wCraft; panels++; }
    if (oMach)  { total += wMach;  panels++; }
    if (panels > 1) total += (panels - 1) * UI_DOCK_GAP;

    int x = (GetScreenWidth() - total) / 2;
    if (x < 8) x = 8;
    if (which == UI_PANEL_INV) return x;
    if (oInv) x += wInv + UI_DOCK_GAP;
    if (which == UI_PANEL_CRAFT) return x;
    if (oCraft) x += wCraft + UI_DOCK_GAP;
    return x;
}

static void UiGetCraftLayout(const Player *p, CraftLayout *layout) {
    int sh = GetScreenHeight();
    layout->cols = 6;
    layout->gap = 8;
    layout->w = UiCraftPanelW(p);
    layout->detailH = 88;

    // Cell size follows from the panel width, so cells shrink
    // gracefully as panels crowd each other.
    layout->cellSize = (layout->w - 48 - (layout->cols - 1) * layout->gap) / layout->cols;
    if (layout->cellSize < 26) layout->cellSize = 26;
    layout->cellH = layout->cellSize + 13;   // name strip under the sprite

    layout->h = UI_HEADER_H + 16 + 4 * (layout->cellH + layout->gap) + layout->detailH + 28;
    if (layout->h > sh - 16) layout->h = sh - 16;
    layout->x = UiDockX(p, UI_PANEL_CRAFT, MachineAt(machineUiX, machineUiY));
    layout->y = (sh - layout->h) / 2;

    layout->gridX = layout->x + 24;
    layout->gridY = layout->y + UI_HEADER_H + 14;
    layout->detailY = layout->y + layout->h - layout->detailH - 14;
    // However many rows fit between the header and the detail strip.
    layout->visibleRows = (layout->detailY - 8 - layout->gridY) / (layout->cellH + layout->gap);
    if (layout->visibleRows < 2) layout->visibleRows = 2;
    if (layout->visibleRows > 8) layout->visibleRows = 8;
}

// Screen rectangle of the cell at visible row r, column c.
static Rectangle UiCraftCellRect(const CraftLayout *layout, int r, int c) {
    return (Rectangle){ (float)(layout->gridX + c * (layout->cellSize + layout->gap)),
                        (float)(layout->gridY + r * (layout->cellH + layout->gap)),
                        (float)layout->cellSize, (float)layout->cellH };
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
    // A framed bar behind the slots so the HUD hotbar reads as the
    // same object as the bottom row of the character screen.
    if (p->slotCount > 0) {
        Rectangle first = UiHotbarSlotRect(p, 0);
        Rectangle last  = UiHotbarSlotRect(p, p->slotCount - 1);
        DrawRectangle((int)first.x - 6, (int)first.y - 6,
                      (int)(last.x + last.width - first.x) + 12,
                      (int)first.height + 12, (Color){ 84, 84, 86, 225 });
        DrawRectangleLines((int)first.x - 6, (int)first.y - 6,
                           (int)(last.x + last.width - first.x) + 12,
                           (int)first.height + 12, UI_PANEL_BORDER);
    }

    for (int s = 0; s < p->slotCount; s++) {
        ItemID id = (s < INVENTORY_SIZE) ? p->inventorySlots[s] : ITEM_NONE;
        int amount = (s < INVENTORY_SIZE) ? p->inventoryAmounts[s] : 0;
        Rectangle r = UiHotbarSlotRect(p, s);
        float x = r.x, y = r.y, slot = r.width;
        bool sel = (s == p->selectedSlot);

        DrawRectangleRec(r, UI_SLOT_BG);
        DrawRectangleLinesEx(r, sel ? 3 : 1, sel ? UI_ACCENT : UI_SLOT_BORDER);

        if (id != ITEM_NONE) {
            DrawItemSprite(id, x + slot/2 - 17, y + slot/2 - 17, 34);
            // Owning zero of it? Dim the sprite with a dark veil.
            if (p->inventory[id] == 0)
                DrawRectangle((int)(x+slot/2-17), (int)(y+slot/2-17), 34, 34, (Color){10,10,14,170});
            // Quiet stack count in the corner.
            const char *amt = TextFormat("%d", amount);
            DrawText(amt, (int)(x + slot - MeasureText(amt, 11) - 5),
                     (int)(y + slot - 15), 11, (Color){255, 255, 255, 170});
        }
    }
}

static void UiGetInventoryLayout(const Player *p, InventoryLayout *layout) {
    int sh = GetScreenHeight();

    int panelW = UiInventoryPanelW(p);
    int padding = 16;
    int detailH = 44;      // slim name strip at the very bottom
    int hotbarSep = 12;    // the Minecraft gap above the hotbar row
    int gap = 6;

    // Slot size follows from the panel width; the height then
    // follows from the slots, so the panel is always exactly as
    // tall as its contents need.
    int slotSize = (panelW - padding * 2 - gap * (INVENTORY_COLS - 1)) / INVENTORY_COLS;
    if (slotSize < 18) slotSize = 18;
    int storageRows = INVENTORY_ROWS - 1;    // last row is the hotbar
    int panelH = UI_HEADER_H + padding + storageRows * (slotSize + gap)
               + hotbarSep + slotSize + padding + detailH;
    if (panelH > sh - 16) panelH = sh - 16;

    int gridW = slotSize * INVENTORY_COLS + gap * (INVENTORY_COLS - 1);
    layout->panelX = UiDockX(p, UI_PANEL_INV, MachineAt(machineUiX, machineUiY));
    layout->panelY = (sh - panelH) / 2;
    layout->panelW = panelW;
    layout->panelH = panelH;
    layout->slotSize = slotSize;
    layout->gap = gap;
    layout->startX = layout->panelX + (panelW - gridW) / 2;
    layout->startY = layout->panelY + UI_HEADER_H + padding;
    // Storage is rows 1..6 of the 7x7; the 7th visual row sits apart
    // at the bottom — that's the hotbar, Minecraft-style.
    layout->hotbarY = layout->startY + storageRows * (slotSize + gap) + hotbarSep;
}

// Where does slot `idx` draw? Indices 0..6 ARE the hotbar and live
// in the detached bottom row; storage indices 7..48 fill the grid
// above. ONE function does the remap, so drawing and click hit-
// testing can never disagree about it.
static Rectangle UiInventorySlotRect(const InventoryLayout *l, int idx) {
    int col, y;
    if (idx < HOTBAR_MAX_SLOTS) {
        col = idx;
        y = l->hotbarY;
    } else {
        int s = idx - HOTBAR_MAX_SLOTS;
        col = s % INVENTORY_COLS;
        y = l->startY + (s / INVENTORY_COLS) * (l->slotSize + l->gap);
    }
    return (Rectangle){ (float)(l->startX + col * (l->slotSize + l->gap)),
                        (float)y, (float)l->slotSize, (float)l->slotSize };
}

static void UiDrawInventory(const Player *p) {
    if (!p->inventoryOpen) return;

    InventoryLayout layout = { 0 };
    UiGetInventoryLayout(p, &layout);

    // (The dim over the game is drawn ONCE by main.c, so two open
    // menus don't double-darken the world.)
    UiDrawPanelFrame(layout.panelX, layout.panelY, layout.panelW, layout.panelH, "CHARACTER");

    for (int idx = 0; idx < INVENTORY_SIZE; idx++) {
        Rectangle rect = UiInventorySlotRect(&layout, idx);
        bool hovered  = (idx == p->inventoryCursor);
        bool dragging = p->inventoryDragging && idx == p->inventoryDragIndex;
        bool isHotbar = idx < HOTBAR_MAX_SLOTS;

        DrawRectangleRec(rect, dragging ? (Color){ 96, 70, 24, 255 }
                             : (hovered ? UI_SLOT_HOVER : UI_SLOT_BG));
        // The equipped hotbar slot glows Factorio-orange, even here.
        Color border = UI_SLOT_BORDER;
        if (isHotbar && idx == p->selectedSlot) border = UI_ACCENT;
        if (dragging || (hovered && p->inventoryDragging)) border = GOLD;
        DrawRectangleLinesEx(rect, (border.r == UI_ACCENT.r && isHotbar) ? 2 : 1, border);

        if (p->inventorySlots[idx] != ITEM_NONE && p->inventoryAmounts[idx] > 0) {
            ItemID id = p->inventorySlots[idx];
            int icon = layout.slotSize * 3 / 4;
            DrawItemSprite(id, rect.x + (layout.slotSize - icon) / 2.0f,
                               rect.y + (layout.slotSize - icon) / 2.0f, (float)icon);
            // Quiet stack count, tucked in the corner.
            const char *amt = TextFormat("%d", p->inventoryAmounts[idx]);
            DrawText(amt, (int)(rect.x + layout.slotSize - MeasureText(amt, 11) - 4),
                     (int)(rect.y + layout.slotSize - 14), 11, (Color){255, 255, 255, 180});
        }
    }

    // (The dragged stack is drawn by UiDrawDragGhost, on top of
    // every panel — it can travel between them now.)

    // Slim name strip along the bottom — a stand-in for tooltips.
    ItemID selectedItem = ITEM_NONE;
    int selectedAmount = 0;
    if (p->inventoryCursor >= 0 && p->inventoryCursor < INVENTORY_SIZE) {
        selectedItem = p->inventorySlots[p->inventoryCursor];
        selectedAmount = p->inventoryAmounts[p->inventoryCursor];
    }
    int detailX = layout.panelX + 12;
    int detailY = layout.panelY + layout.panelH - 38;
    if (selectedItem != ITEM_NONE) {
        DrawItemSprite(selectedItem, (float)detailX, (float)(detailY + 2), 22);
        DrawText(TextFormat("%s  x%d", ITEMS[selectedItem].name, selectedAmount),
                 detailX + 30, detailY + 6, 17, RAYWHITE);
    }
}

// ─── Crafting menu (TAB) — a scrollable GRID of recipes ───
static void UiDrawCraftMenu(const Player *p) {
    if (!p->craftMenuOpen) return;
    CraftLayout layout = { 0 };
    UiGetCraftLayout(p, &layout);
    int mx = layout.x, my = layout.y, mw = layout.w, mh = layout.h;

    // (main.c draws the single shared dim.)
    UiDrawPanelFrame(mx, my, mw, mh, "CRAFTING");

    int n = CraftableCount();
    int rows = (n + layout.cols - 1) / layout.cols;   // grid rows total
    int startRow = p->craftScroll;

    // The cells. Slots are Factorio-gray; the accent border carries
    // the meaning: orange = selected, green = craftable, red =
    // missing materials, dim + veil = tech-locked.
    for (int r = 0; r < layout.visibleRows; r++) {
        for (int c = 0; c < layout.cols; c++) {
            int idx = (startRow + r) * layout.cols + c;
            if (idx >= n) break;
            ItemID id = CraftableAtRow(idx);
            Rectangle cell = UiCraftCellRect(&layout, r, c);
            bool sel    = (idx == p->craftSel);
            bool ok     = PlayerCanCraft(p, id);
            bool locked = !PlayerHasTech(p, ITEMS[id].tech);

            DrawRectangleRec(cell, sel ? UI_SLOT_HOVER : UI_SLOT_BG);
            float icon = cell.width - 22;
            DrawItemSprite(id, cell.x + (cell.width - icon) / 2, cell.y + 5, icon);
            if (locked) {
                DrawRectangleRec(cell, (Color){ 20, 20, 22, 170 });
                DrawText("?", (int)(cell.x + cell.width - 13), (int)cell.y + 3, 16, UI_ACCENT);
            } else if (ITEMS[id].yield > 1) {
                DrawText(TextFormat("x%d", ITEMS[id].yield),
                         (int)(cell.x + cell.width - 22), (int)cell.y + 4, 12,
                         (Color){ 255, 255, 255, 190 });
            }
            // The item's name in small print under the sprite —
            // scissored to the cell so long names can't escape.
            const char *nm = ITEMS[id].name;
            int nw = MeasureText(nm, 10);
            int tx = (int)(cell.x + (cell.width - nw) / 2);
            if (nw > (int)cell.width - 4) tx = (int)cell.x + 2;
            BeginScissorMode((int)cell.x + 1, (int)(cell.y + cell.height - 14),
                             (int)cell.width - 2, 13);
            DrawText(nm, tx, (int)(cell.y + cell.height - 12), 10,
                     locked ? GRAY : LIGHTGRAY);
            EndScissorMode();

            Color border = locked ? (Color){ 70, 70, 74, 255 }
                                  : (ok ? (Color){ 96, 178, 96, 255 }
                                        : (Color){ 168, 78, 78, 255 });
            if (sel) border = UI_ACCENT;
            DrawRectangleLinesEx(cell, sel ? 2 : 1, border);
        }
    }

    // Scrollbar, when there's more than a screenful.
    if (rows > layout.visibleRows) {
        int barX = mx + mw - 16;
        int trackH = layout.visibleRows * (layout.cellH + layout.gap) - layout.gap;
        DrawRectangle(barX, layout.gridY, 6, trackH, (Color){ 38, 38, 40, 255 });
        float thumbH = trackH * ((float)layout.visibleRows / rows);
        float thumbY = layout.gridY + (trackH - thumbH) * ((float)startRow / (rows - layout.visibleRows));
        DrawRectangle(barX, (int)thumbY, 6, (int)thumbH, (Color){ 130, 130, 134, 255 });
    }

    // Detail box: the selected recipe's full story, in a box that
    // owns its space — long names and costs can't leak off screen.
    Rectangle detail = { (float)(mx + 14), (float)layout.detailY,
                         (float)(mw - 28), (float)layout.detailH };
    DrawRectangleRec(detail, UI_SLOT_BG);
    DrawRectangleLinesEx(detail, 1, UI_SLOT_BORDER);
    if (p->craftSel >= 0 && p->craftSel < n) {
        ItemID id = CraftableAtRow(p->craftSel);
        const ItemInfo *it = &ITEMS[id];
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
        }
        DrawText(TextFormat("owned: %d", p->inventory[id]),
                 (int)(detail.x + detail.width) - 90, dy + 2, 14, LIGHTGRAY);
    }
    (void)mw; (void)mh;
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

    DrawRectangle(mx, my, mw, mh, (Color){8,16,26,245});
    DrawRectangleLines(mx, my, mw, mh, (Color){80,200,230,255});
    DrawText("RESEARCH TERMINAL", mx+20, my+15, 30, (Color){80,200,230,255});
    UiDrawCloseButton(mx, my, mw);

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
            (void)sel; (void)affordable;
        }
    }
    (void)mh;
}

// ─── Machine panels (chest / drill / inserter / belt) ─────
// One panel serves every machine; only the grid shape differs.
typedef struct {
    int x, y, w, h;
    int cols, rows;
    int slotSize, gap;
    int gridX, gridY;
    int slotCount;
    bool hasFuel;
    Rectangle fuelRect;   // the coal slot, when the machine burns it
} MachinePanelLayout;

static void UiGetMachinePanelLayout(const Player *p, const Machine *m,
                                    MachinePanelLayout *l) {
    l->slotCount = MachineSlotCount(m);
    l->hasFuel = TileNeedsFuel(m->type);
    if (m->type == TILE_CHEST)      { l->cols = 7; l->rows = 7; }
    else if (m->type == TILE_DRILL) { l->cols = 4; l->rows = 2; }
    else                            { l->cols = 1; l->rows = 1; }

    l->gap = 6;
    l->w = UiMachinePanelW(p, m);
    l->slotSize = (l->w - 32 - (l->cols - 1) * l->gap) / l->cols;
    if (l->slotSize < 20) l->slotSize = 20;
    if (l->slotSize > 44) l->slotSize = 44;

    int gridW = l->cols * l->slotSize + (l->cols - 1) * l->gap;
    int gridH = l->rows * l->slotSize + (l->rows - 1) * l->gap;
    int fuelH = l->hasFuel ? 62 : 0;

    l->h = UI_HEADER_H + 16 + gridH + fuelH + 18;
    // Third column of the dock, beside Character and Crafting.
    l->x = UiDockX(p, UI_PANEL_MACH, m);
    l->y = (GetScreenHeight() - l->h) / 2;
    if (l->y < 8) l->y = 8;
    l->gridX = l->x + (l->w - gridW) / 2;
    l->gridY = l->y + UI_HEADER_H + 12;
    l->fuelRect = (Rectangle){ (float)(l->x + 16),
                               (float)(l->gridY + gridH + 22), 44, 44 };
}

static Rectangle UiMachineSlotRect(const MachinePanelLayout *l, int idx) {
    int c = idx % l->cols, r = idx / l->cols;
    return (Rectangle){ (float)(l->gridX + c * (l->slotSize + l->gap)),
                        (float)(l->gridY + r * (l->slotSize + l->gap)),
                        (float)l->slotSize, (float)l->slotSize };
}

static void UiDrawMachinePanel(const Player *p) {
    if (machineUiX < 0 || machineUiY < 0) return;
    Machine *m = MachineAt(machineUiX, machineUiY);
    if (m == NULL) return;

    MachinePanelLayout l = { 0 };
    UiGetMachinePanelLayout(p, m, &l);
    UiDrawPanelFrame(l.x, l.y, l.w, l.h, TILES[m->type].name);
    UiDrawCloseButton(l.x, l.y, l.w);

    for (int i = 0; i < l.slotCount && i < l.cols * l.rows; i++) {
        Rectangle r = UiMachineSlotRect(&l, i);
        bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
        DrawRectangleRec(r, hovered ? UI_SLOT_HOVER : UI_SLOT_BG);
        DrawRectangleLinesEx(r, 1, UI_SLOT_BORDER);
        if (m->slots[i] != ITEM_NONE && m->counts[i] > 0) {
            int icon = l.slotSize * 3 / 4;
            DrawItemSprite(m->slots[i], r.x + (l.slotSize - icon) / 2.0f,
                                        r.y + (l.slotSize - icon) / 2.0f, (float)icon);
            const char *amt = TextFormat("%d", m->counts[i]);
            DrawText(amt, (int)(r.x + l.slotSize - MeasureText(amt, 11) - 4),
                     (int)(r.y + l.slotSize - 14), 11, (Color){ 255, 255, 255, 185 });
        }
    }

    if (l.hasFuel) {
        // Coal slot + a burn bar for the lump currently alight.
        bool hovered = CheckCollisionPointRec(GetMousePosition(), l.fuelRect);
        DrawRectangleRec(l.fuelRect, hovered ? UI_SLOT_HOVER : UI_SLOT_BG);
        DrawRectangleLinesEx(l.fuelRect, 1, (m->coal > 0 || m->fuel > 0) ? UI_ACCENT : (Color){ 168, 78, 78, 255 });
        DrawItemSprite(ITEM_COAL, l.fuelRect.x + 8, l.fuelRect.y + 8, 28);
        DrawText(TextFormat("%d/%d", m->coal, MACHINE_FUEL_MAX),
                 (int)l.fuelRect.x + 2, (int)(l.fuelRect.y + l.fuelRect.height - 13), 11,
                 (Color){ 255, 255, 255, 195 });

        int barX = (int)(l.fuelRect.x + l.fuelRect.width + 12);
        int barY = (int)(l.fuelRect.y + 12);
        int barW = l.x + l.w - 16 - barX;
        DrawRectangle(barX, barY, barW, 14, (Color){ 34, 34, 36, 255 });
        float burn = m->fuel / COAL_FUEL_SECONDS;
        if (burn > 0) DrawRectangle(barX, barY, (int)(barW * burn), 14, (Color){ 255, 150, 40, 235 });
        DrawRectangleLines(barX, barY, barW, 14, UI_SLOT_BORDER);
        DrawText(m->fuel > 0 ? "BURNING" : "NO FUEL", barX, barY + 20, 13,
                 m->fuel > 0 ? (Color){ 255, 190, 120, 255 } : (Color){ 230, 110, 110, 255 });
    }
}

// ─── Health bar ───────────────────────────────────────────
static void UiDrawHealth(const Player *p) {
    int w = 300, h = 28;
    int x = 18, y = GetScreenHeight() - 122;
    float frac = p->hp / PLAYER_MAX_HP;
    if (frac < 0) frac = 0;
    Color c = frac > 0.5f ? (Color){90,200,90,255} : (frac > 0.25f ? GOLD : RED);
    DrawRectangle(x - 3, y - 3, w + 6, h + 6, (Color){10,10,18,225});
    DrawRectangle(x, y, (int)(w * frac), h, c);
    DrawRectangleLines(x - 3, y - 3, w + 6, h + 6, UI_PANEL_BORDER);
    DrawText(TextFormat("HP %d/%d", (int)p->hp, (int)PLAYER_MAX_HP),
             x + 8, y + 6, 18, RAYWHITE);
    if (p->invulnTimer > 0) {
        DrawText("SHIELDED", x + w - 90, y + 7, 16, SKYBLUE);
    }
}

// ─── The dragged stack, following the cursor ──────────────
// Drawn above every panel, since a drag can cross from the
// backpack to a chest to the hotbar.
static void UiDrawDragGhost(void) {
    if (uiDragKind == DRAG_NONE || uiDragItem == ITEM_NONE) return;
    Vector2 m = GetMousePosition();
    float size = 34;
    DrawRectangleRounded((Rectangle){ m.x - size / 2, m.y - size / 2, size, size },
                         0.2f, 6, (Color){ 255, 255, 255, 70 });
    DrawItemSprite(uiDragItem, m.x - size / 2 + 4, m.y - size / 2 + 4, size - 8);
    if (uiDragCount > 1) {
        DrawText(TextFormat("%d", uiDragCount), (int)(m.x + 6), (int)(m.y + 4), 12, RAYWHITE);
    }
}

// ─── Crafting queue readout ───────────────────────────────
// Bottom-left above the health bar: what's building, how far along,
// and how many more are lined up behind it.
// The queue STACKS upward from just above the health bar: one bar
// per pending build, the live one at the bottom. Each carries its
// own [x] to cancel and refund that entry.
#define UI_QUEUE_ROW_H 34
#define UI_QUEUE_MAX_SHOWN 6

static Rectangle UiCraftQueueRect(int index) {
    float bottom = (float)(GetScreenHeight() - 160);
    return (Rectangle){ 18, bottom - index * (UI_QUEUE_ROW_H + 4), 290, UI_QUEUE_ROW_H };
}

static Rectangle UiCraftQueueCancelRect(int index) {
    Rectangle r = UiCraftQueueRect(index);
    return (Rectangle){ r.x + r.width - 26, r.y + 5, 22, UI_QUEUE_ROW_H - 10 };
}

static void UiDrawCraftQueue(const Player *p) {
    if (p->craftQueueCount <= 0) return;
    int shown = p->craftQueueCount < UI_QUEUE_MAX_SHOWN
              ? p->craftQueueCount : UI_QUEUE_MAX_SHOWN;

    for (int i = 0; i < shown; i++) {
        ItemID id = p->craftQueue[i];
        Rectangle r = UiCraftQueueRect(i);
        float need = ItemCraftTime(id);
        // Only the head is actually building; the rest are waiting.
        float frac = (i == 0 && need > 0) ? (p->craftProgress / need) : 0.0f;
        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;

        DrawRectangleRec(r, (Color){ 10, 10, 16, 215 });
        if (frac > 0) {
            DrawRectangle((int)r.x, (int)r.y, (int)(r.width * frac), (int)r.height,
                          (Color){ 60, 110, 70, 235 });
        }
        DrawRectangleLinesEx(r, 1, i == 0 ? UI_ACCENT : UI_PANEL_BORDER);
        DrawItemSprite(id, r.x + 4, r.y + 4, 26);
        DrawText(ITEMS[id].name, (int)r.x + 36, (int)r.y + 4, 14, RAYWHITE);
        DrawText(i == 0 ? TextFormat("%.1fs", need - p->craftProgress) : "queued",
                 (int)r.x + 36, (int)r.y + 19, 11, (Color){ 200, 210, 225, 220 });

        Rectangle x = UiCraftQueueCancelRect(i);
        bool hov = CheckCollisionPointRec(GetMousePosition(), x);
        DrawRectangleRec(x, hov ? (Color){ 170, 62, 62, 255 } : (Color){ 60, 60, 64, 255 });
        DrawRectangleLinesEx(x, 1, hov ? RAYWHITE : (Color){ 110, 110, 114, 255 });
        DrawText("x", (int)x.x + 7, (int)x.y + 3, 16, RAYWHITE);
    }

    if (p->craftQueueCount > shown) {
        Rectangle top = UiCraftQueueRect(shown - 1);
        DrawText(TextFormat("+%d more", p->craftQueueCount - shown),
                 (int)top.x, (int)top.y - 16, 13, (Color){ 210, 220, 235, 220 });
    }
}

// ─── Ammo readout + circular reload gauge ─────────────────
// Sits just above the hotbar. While reloading it's a ring that
// sweeps clockwise to full — the classic "wait for it" dial.
static void UiDrawAmmo(const Player *p) {
    ItemID gun = p->selected;
    bool reloading = (p->reloadTimer > 0 && p->reloadTotal > 0);
    if (!ItemIsWeapon(gun) && !reloading) return;
    if (reloading) gun = p->reloadingItem;

    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() - 118;   // above the hotbar frame
    int magSize = ItemMagSize(gun);
    ItemID ammo = ItemAmmoFor(gun);

    if (reloading) {
        float done = 1.0f - (p->reloadTimer / p->reloadTotal);   // 0..1
        float radius = 26.0f;
        // Dark backing ring, then the progress sweep on top.
        DrawCircle(cx, cy, radius + 4, (Color){ 10, 10, 16, 205 });
        DrawRing((Vector2){ (float)cx, (float)cy }, radius - 6, radius,
                 0, 360, 48, (Color){ 48, 48, 52, 255 });
        DrawRing((Vector2){ (float)cx, (float)cy }, radius - 6, radius,
                 -90, -90 + 360 * done, 48, UI_ACCENT);
        const char *label = "RELOAD";
        DrawText(label, cx - MeasureText(label, 12) / 2, cy - 5, 12, RAYWHITE);
    } else if (magSize > 0) {
        // Loaded / reserve, centered above the bar.
        const char *txt = TextFormat("%d / %d", p->mag[gun], p->inventory[ammo]);
        int tw = MeasureText(txt, 20);
        DrawRectangle(cx - tw / 2 - 10, cy - 13, tw + 20, 26, (Color){ 10, 10, 16, 190 });
        DrawText(txt, cx - tw / 2, cy - 9, 20,
                 p->mag[gun] > 0 ? RAYWHITE : (Color){ 235, 110, 110, 255 });
    }
}

// (The old always-on help line is gone — controls live in
// Settings → Controls now, where they belong.)

#endif // UI_H
