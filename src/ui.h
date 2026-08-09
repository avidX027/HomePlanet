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
#include "saves.h"      // SaveSlot — the saves screen draws the slot table

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
//
// The menu is a GRID of item cells, split into labelled GROUPS
// (smelting & ammo, logistics, military, ...) with a detail box
// underneath and an AUTO-CRAFT rail down the right-hand side. The
// grouping is not decoration: the things you make forty times an
// hour sit together at the top, and the pickaxe sits in its own
// block at the bottom where your cursor never goes by accident.
#define CRAFT_MAX_ROWS    48
#define CRAFT_MAX_COLS     8
#define UI_CRAFT_GROUP_H  24    // height of a group heading row
#define UI_AUTO_ROW_H     46    // height of one auto-craft rail row

// One visual row of the grid: either a GROUP HEADING or a run of
// recipe cells. Both drawing and hit-testing walk the same rows.
typedef struct {
    bool   header;
    int    cat;                    // heading: which group
    ItemID ids[CRAFT_MAX_COLS];    // cells: the recipes on this row...
    int    idx[CRAFT_MAX_COLS];    // ...and their craft-menu indices
    int    count;
} CraftRow;

typedef struct {
    int x, y;           // top-left of the panel
    int w, h;           // panel size
    int cols;           // grid columns
    int cellSize;       // cell width (shrinks on small windows)
    int cellH;          // cell height = width + a strip for the name
    int gap;
    int gridX, gridY;   // top-left of the cell grid
    int gridW;          // width of the recipe column (rail sits right of it)
    int rowH;           // pitch of one row of cells
    int railX, railW;   // the auto-craft rail
    int autoRows;       // how many rail entries fit
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
    // Deliberately the widest panel of the three: it carries a grid,
    // a detail box AND the auto-craft rail.
    return UiClampW(740, 400, UiPanelBudget(p));
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
    layout->gap = 8;
    layout->w = UiCraftPanelW(p);
    layout->detailH = 96;

    // The rail takes a fixed slice off the right; the recipe grid
    // gets what's left, and the cells size themselves to fill it.
    layout->railW = (layout->w >= 560) ? 182 : 152;
    layout->gridW = layout->w - layout->railW - 46;
    if (layout->gridW < 160) {          // absurdly narrow window: drop the rail
        layout->railW = 0;
        layout->gridW = layout->w - 40;
    }
    layout->cols = layout->gridW / 66;   // ~64px cells: big targets, few misclicks
    if (layout->cols < 3) layout->cols = 3;
    if (layout->cols > CRAFT_MAX_COLS) layout->cols = CRAFT_MAX_COLS;
    layout->cellSize = (layout->gridW - (layout->cols - 1) * layout->gap) / layout->cols;
    if (layout->cellSize < 30) layout->cellSize = 30;
    layout->cellH = layout->cellSize + 14;   // name strip under the sprite
    layout->rowH  = layout->cellH + layout->gap;

    // Tall on purpose — the whole point is to see more at once.
    layout->h = sh - 40;
    if (layout->h > 780)   layout->h = 780;
    if (layout->h > sh - 16) layout->h = sh - 16;
    layout->x = UiDockX(p, UI_PANEL_CRAFT, MachineAt(machineUiX, machineUiY));
    layout->y = (sh - layout->h) / 2;

    layout->gridX = layout->x + 20;
    layout->gridY = layout->y + UI_HEADER_H + 30;   // room for the column labels
    layout->railX = layout->x + layout->w - layout->railW - 16;
    layout->detailY = layout->y + layout->h - layout->detailH - 14;

    // (How many rows actually fit is worked out row by row in
    // UiCraftVisibleRows — headings and cell rows are different
    // heights, so there is no single "rows per screen" number.)

    // Rail: a big toggle button, then as many list rows as fit.
    layout->autoRows = (layout->detailY - 12 - (layout->gridY + 42)) / UI_AUTO_ROW_H;
    if (layout->autoRows < 0) layout->autoRows = 0;
    if (layout->autoRows > 8) layout->autoRows = 8;
    if (layout->railW == 0) layout->autoRows = 0;
}

// ─── The grouped row list ─────────────────────────────────
// Walks the recipes in craft-menu order (which is grouped, see
// CraftableAtRow) and lays them out as heading rows and cell rows.
// Called by BOTH the drawing here and the input in main.c, so a
// click can never land on a cell that isn't where it looks.
static int UiBuildCraftRows(int cols, CraftRow *rows, int maxRows) {
    if (cols > CRAFT_MAX_COLS) cols = CRAFT_MAX_COLS;
    if (cols < 1) cols = 1;
    int n = 0, order = 0;
    for (int c = 0; c < CAT_COUNT; c++) {
        int inCat = CraftableCountInCat(c);
        if (inCat <= 0) continue;               // empty group: no heading
        if (n >= maxRows) break;
        rows[n].header = true; rows[n].cat = c; rows[n].count = 0;
        n++;
        int placed = 0;
        while (placed < inCat && n < maxRows) {
            CraftRow *r = &rows[n];
            r->header = false; r->cat = c; r->count = 0;
            for (int k = 0; k < cols && placed < inCat; k++) {
                r->ids[r->count] = CraftableAtRow(order);
                r->idx[r->count] = order;
                r->count++;
                order++; placed++;
            }
            n++;
        }
    }
    return n;
}

static int UiCraftRowHeight(const CraftLayout *l, const CraftRow *row) {
    return row->header ? UI_CRAFT_GROUP_H : l->rowH;
}

// Fill outY[] with the screen y of each row starting at `scroll`,
// and return how many of them actually fit above the detail box.
static int UiCraftVisibleRows(const CraftLayout *l, const CraftRow *rows, int rowCount,
                              int scroll, int *outY, int maxOut) {
    int limit = l->detailY - 10;
    int y = l->gridY, shown = 0;
    for (int i = scroll; i < rowCount && shown < maxOut; i++) {
        int h = UiCraftRowHeight(l, &rows[i]);
        if (y + h > limit) break;
        outY[shown++] = y;
        y += h;
    }
    return shown;
}

// The furthest you can scroll before the list just shows blank space.
static int UiCraftMaxScroll(const CraftLayout *l, const CraftRow *rows, int rowCount) {
    int avail = l->detailY - 10 - l->gridY;
    int used = 0, first = rowCount > 0 ? rowCount - 1 : 0;
    for (int i = rowCount - 1; i >= 0; i--) {
        int h = UiCraftRowHeight(l, &rows[i]);
        if (used + h > avail) break;
        used += h;
        first = i;
    }
    return first;
}

// Which row holds craft-menu index `idx`? (Keyboard navigation uses
// it to drag the window along with the selection.)
static int UiCraftRowOfIndex(const CraftRow *rows, int rowCount, int idx) {
    for (int i = 0; i < rowCount; i++) {
        if (rows[i].header) continue;
        for (int k = 0; k < rows[i].count; k++) if (rows[i].idx[k] == idx) return i;
    }
    return 0;
}

// Screen rectangle of the cell in column c of a row drawn at `rowY`.
static Rectangle UiCraftCellRect(const CraftLayout *layout, int rowY, int c) {
    return (Rectangle){ (float)(layout->gridX + c * (layout->cellSize + layout->gap)),
                        (float)rowY,
                        (float)layout->cellSize, (float)layout->cellH };
}

// ─── Auto-craft rail rectangles ───────────────────────────
// The big button adds/removes the SELECTED recipe; each list row
// below it owns one automated recipe, with -/+ target buttons and an
// [x] to switch it off.
static Rectangle UiCraftAutoButtonRect(const CraftLayout *l) {
    return (Rectangle){ (float)l->railX, (float)l->gridY, (float)l->railW, 34.0f };
}
static Rectangle UiCraftAutoRowRect(const CraftLayout *l, int slot) {
    return (Rectangle){ (float)l->railX, (float)(l->gridY + 42 + slot * UI_AUTO_ROW_H),
                        (float)l->railW, (float)(UI_AUTO_ROW_H - 6) };
}
static Rectangle UiCraftAutoMinusRect(const CraftLayout *l, int slot) {
    Rectangle r = UiCraftAutoRowRect(l, slot);
    return (Rectangle){ r.x + 6, r.y + r.height - 21, 22, 17 };
}
static Rectangle UiCraftAutoPlusRect(const CraftLayout *l, int slot) {
    Rectangle r = UiCraftAutoRowRect(l, slot);
    return (Rectangle){ r.x + r.width - 28, r.y + r.height - 21, 22, 17 };
}
static Rectangle UiCraftAutoOffRect(const CraftLayout *l, int slot) {
    Rectangle r = UiCraftAutoRowRect(l, slot);
    return (Rectangle){ r.x + r.width - 22, r.y + 4, 18, 16 };
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
    // Every button in the game runs through here, so one line gives
    // the whole UI a click.
    if (button->clicked) SfxPlay(SFX_UI_CLICK, 0.35f, 1.0f);
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
        // Hover is read from the MOUSE, the same way the machine
        // panel does it — so the orange highlight behaves identically
        // whichever panel the cursor is over.
        bool hovered  = CheckCollisionPointRec(GetMousePosition(), rect);
        bool dragging = (uiDragKind == DRAG_PLAYER && idx == uiDragIndex);
        bool isHotbar = idx < HOTBAR_MAX_SLOTS;

        DrawRectangleRec(rect, dragging ? (Color){ 96, 70, 24, 255 }
                             : (hovered ? UI_SLOT_HOVER : UI_SLOT_BG));
        Color border = UI_SLOT_BORDER;
        if (isHotbar && idx == p->selectedSlot) border = UI_ACCENT;  // equipped
        if (hovered || dragging) border = UI_ACCENT;                 // drop target
        DrawRectangleLinesEx(rect, (hovered || dragging) ? 2 : 1, border);

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

// ─── Crafting menu (TAB) — grouped GRID + auto-craft rail ─
static void UiDrawCraftMenu(const Player *p) {
    if (!p->craftMenuOpen) return;
    CraftLayout layout = { 0 };
    UiGetCraftLayout(p, &layout);
    int mx = layout.x, my = layout.y, mw = layout.w, mh = layout.h;

    // (main.c draws the single shared dim.)
    UiDrawPanelFrame(mx, my, mw, mh, "CRAFTING");

    int n = CraftableCount();
    CraftRow rows[CRAFT_MAX_ROWS];
    int rowCount = UiBuildCraftRows(layout.cols, rows, CRAFT_MAX_ROWS);
    int ys[CRAFT_MAX_ROWS];
    int shown = UiCraftVisibleRows(&layout, rows, rowCount, p->craftScroll, ys, CRAFT_MAX_ROWS);

    DrawText("RECIPES", layout.gridX, layout.gridY - 20, 13, (Color){ 170, 170, 176, 255 });
    if (layout.railW > 0) {
        DrawText("AUTO-CRAFT", layout.railX, layout.gridY - 20, 13, UI_ACCENT);
    }

    // The cells. Slots are Factorio-gray; the accent border carries
    // the meaning: orange = selected, green = craftable (counting
    // what it can make from parts), red = missing materials, dim +
    // veil = tech-locked.
    for (int r = 0; r < shown; r++) {
        const CraftRow *row = &rows[p->craftScroll + r];
        if (row->header) {
            // Group heading: a label with a rule running off it.
            const char *title = CRAFT_CAT_NAMES[row->cat];
            int tw = MeasureText(title, 13);
            DrawText(title, layout.gridX, ys[r] + 7, 13, (Color){ 232, 196, 120, 255 });
            DrawRectangle(layout.gridX + tw + 10, ys[r] + 13,
                          layout.gridW - tw - 10, 1, (Color){ 96, 88, 70, 255 });
            continue;
        }
        for (int c = 0; c < row->count; c++) {
            ItemID id = row->ids[c];
            int idx = row->idx[c];
            Rectangle cell = UiCraftCellRect(&layout, ys[r], c);
            bool sel    = (idx == p->craftSel);
            bool ok     = PlayerCanCraft(p, id);          // straight from stock
            bool chain  = !ok && PlayerCanCraftChain(p, id);  // ...or via parts
            bool locked = !PlayerHasTech(p, ITEMS[id].tech);

            DrawRectangleRec(cell, sel ? UI_SLOT_HOVER : UI_SLOT_BG);
            float icon = cell.width - 24;
            DrawItemSprite(id, cell.x + (cell.width - icon) / 2, cell.y + 6, icon);
            if (locked) {
                DrawRectangleRec(cell, (Color){ 20, 20, 22, 170 });
                DrawText("?", (int)(cell.x + cell.width - 13), (int)cell.y + 3, 16, UI_ACCENT);
            } else if (ITEMS[id].yield > 1) {
                DrawText(TextFormat("x%d", ITEMS[id].yield),
                         (int)(cell.x + cell.width - 22), (int)cell.y + 4, 12,
                         (Color){ 255, 255, 255, 190 });
            }
            // An automated recipe wears a little orange A.
            if (autoCraftOn[id]) {
                DrawRectangle((int)cell.x + 3, (int)cell.y + 3, 13, 13, UI_ACCENT);
                DrawText("A", (int)cell.x + 6, (int)cell.y + 4, 11, (Color){ 30, 24, 10, 255 });
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

            // The border says what a click would do: green = build it
            // right now, blue = buildable, but the parts get made
            // first, red = you're missing raw material, dim = locked.
            Color border = locked ? (Color){ 70, 70, 74, 255 }
                         : (ok    ? (Color){ 96, 178, 96, 255 }
                         : (chain ? (Color){ 92, 132, 196, 255 }
                                  : (Color){ 168, 78, 78, 255 }));
            if (sel) border = UI_ACCENT;
            DrawRectangleLinesEx(cell, sel ? 2 : 1, border);
        }
    }

    // Scrollbar, when the list is taller than the window.
    if (shown < rowCount) {
        int barX = layout.gridX + layout.gridW + 4;
        int trackTop = layout.gridY, trackH = layout.detailY - 10 - layout.gridY;
        DrawRectangle(barX, trackTop, 6, trackH, (Color){ 38, 38, 40, 255 });
        float thumbH = trackH * ((float)shown / rowCount);
        if (thumbH < 18) thumbH = 18;
        float t = (rowCount - shown > 0) ? (float)p->craftScroll / (rowCount - shown) : 0;
        if (t > 1) t = 1;
        DrawRectangle(barX, (int)(trackTop + (trackH - thumbH) * t), 6, (int)thumbH,
                      (Color){ 130, 130, 134, 255 });
    }

    // ── The auto-craft rail ──────────────────────────────
    if (layout.railW > 0) {
        ItemID selId = (p->craftSel >= 0 && p->craftSel < n) ? CraftableAtRow(p->craftSel)
                                                             : ITEM_NONE;
        Rectangle btn = UiCraftAutoButtonRect(&layout);
        bool on = (selId != ITEM_NONE && autoCraftOn[selId]);
        bool hov = CheckCollisionPointRec(GetMousePosition(), btn);
        DrawRectangleRec(btn, on ? (Color){ 122, 74, 26, 255 }
                                 : (hov ? (Color){ 66, 66, 70, 255 } : UI_SLOT_BG));
        DrawRectangleLinesEx(btn, 1, on ? UI_ACCENT : UI_SLOT_BORDER);
        if (selId != ITEM_NONE) {
            DrawItemSprite(selId, btn.x + 6, btn.y + 7, 20);
            const char *label = on ? "STOP AUTO" : "AUTO-CRAFT";
            DrawText(label, (int)btn.x + 32, (int)btn.y + 10, 14,
                     on ? (Color){ 255, 214, 150, 255 } : RAYWHITE);
        }

        // An empty rail explains itself rather than sitting there
        // being mysterious.
        if (AutoCraftCount() == 0 && layout.autoRows > 0) {
            Rectangle r = UiCraftAutoRowRect(&layout, 0);
            DrawText("Nothing automated.", (int)r.x + 6, (int)r.y + 4, 11,
                     (Color){ 156, 156, 164, 255 });
            DrawText("Pick a recipe, then", (int)r.x + 6, (int)r.y + 18, 11,
                     (Color){ 156, 156, 164, 255 });
            DrawText("hit AUTO-CRAFT.", (int)r.x + 6, (int)r.y + 32, 11,
                     (Color){ 156, 156, 164, 255 });
        }

        for (int slot = 0; slot < layout.autoRows; slot++) {
            ItemID id = AutoCraftAt(slot);
            Rectangle r = UiCraftAutoRowRect(&layout, slot);
            if (id == ITEM_NONE) {
                // Empty berth — drawn faintly so the rail reads as a
                // list with room in it, not as a broken panel.
                DrawRectangleLinesEx(r, 1, (Color){ 66, 66, 70, 255 });
                continue;
            }
            int stock = p->inventory[id];
            int target = autoCraftTarget[id];
            bool full = stock >= target;
            DrawRectangleRec(r, UI_SLOT_BG);
            DrawRectangleLinesEx(r, 1, full ? (Color){ 96, 178, 96, 255 } : UI_ACCENT);
            DrawItemSprite(id, r.x + 5, r.y + 4, 18);
            BeginScissorMode((int)r.x + 26, (int)r.y + 3, (int)r.width - 50, 14);
            DrawText(ITEMS[id].name, (int)r.x + 26, (int)r.y + 4, 12, RAYWHITE);
            EndScissorMode();

            // Progress toward the target, as a thin bar.
            float frac = (target > 0) ? (float)stock / target : 0;
            if (frac > 1) frac = 1;
            int barX = (int)r.x + 6, barY = (int)r.y + 21, barW = (int)r.width - 12;
            DrawRectangle(barX, barY, barW, 4, (Color){ 32, 32, 34, 255 });
            DrawRectangle(barX, barY, (int)(barW * frac), 4,
                          full ? (Color){ 96, 178, 96, 255 } : UI_ACCENT);

            Rectangle minus = UiCraftAutoMinusRect(&layout, slot);
            Rectangle plus  = UiCraftAutoPlusRect(&layout, slot);
            Rectangle off   = UiCraftAutoOffRect(&layout, slot);
            Vector2 mp = GetMousePosition();
            DrawRectangleRec(minus, CheckCollisionPointRec(mp, minus)
                             ? (Color){ 80, 80, 86, 255 } : (Color){ 58, 58, 62, 255 });
            DrawRectangleRec(plus, CheckCollisionPointRec(mp, plus)
                             ? (Color){ 80, 80, 86, 255 } : (Color){ 58, 58, 62, 255 });
            DrawText("-", (int)minus.x + 9, (int)minus.y + 1, 15, RAYWHITE);
            DrawText("+", (int)plus.x + 7, (int)plus.y + 1, 15, RAYWHITE);
            DrawText(TextFormat("%d / %d", stock, target),
                     (int)r.x + 34, (int)minus.y + 2, 12, (Color){ 210, 214, 224, 255 });
            DrawRectangleRec(off, CheckCollisionPointRec(mp, off)
                             ? (Color){ 170, 62, 62, 255 } : (Color){ 58, 58, 62, 255 });
            DrawText("x", (int)off.x + 6, (int)off.y + 1, 13, RAYWHITE);
        }
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
        // Plain ASCII: raylib's built-in font has no dashes or bullets.
        DrawText("click: craft (missing parts get queued first)    right-click: craft 5",
                 dx, (int)(detail.y + detail.height) - 20, 12, (Color){ 160, 166, 178, 255 });
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
    Rectangle fuelRect;     // the coal slot, when the machine burns it
    bool hasFilter;         // inserters and splitters sort by item
    Rectangle filterRect;   // ...and this is the button that says which
} MachinePanelLayout;

static void UiGetMachinePanelLayout(const Player *p, const Machine *m,
                                    MachinePanelLayout *l) {
    l->slotCount = MachineSlotCount(m);
    l->hasFuel = TileNeedsFuel(m->type);
    l->hasFilter = TileHasFilter(m->type);
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
    int fuelH   = l->hasFuel   ? 62 : 0;
    int filterH = l->hasFilter ? 66 : 0;

    l->h = UI_HEADER_H + 16 + gridH + fuelH + filterH + 18;
    // Third column of the dock, beside Character and Crafting.
    l->x = UiDockX(p, UI_PANEL_MACH, m);
    l->y = (GetScreenHeight() - l->h) / 2;
    if (l->y < 8) l->y = 8;
    l->gridX = l->x + (l->w - gridW) / 2;
    l->gridY = l->y + UI_HEADER_H + 12;
    l->fuelRect = (Rectangle){ (float)(l->x + 16),
                               (float)(l->gridY + gridH + 22), 44, 44 };
    // The filter sits under the fuel slot when there is one, so an
    // inserter reads top to bottom as cargo, fuel, sorting rule.
    l->filterRect = (Rectangle){ (float)(l->x + 16),
                                 (float)(l->gridY + gridH + 26 + fuelH), 44, 44 };
}

// ─── The filter picker ────────────────────────────────────
// Click a machine's filter slot and this drops over the screen: a
// grid of EVERY item in the game — which, since anything can ride a
// belt, is exactly the list of things worth sorting for. Click one
// and it becomes the rule. The grid is built straight from the ITEMS
// table, so an item added to gamedata.h appears here for free.
#define UI_FILTER_COLS 8

typedef struct {
    int x, y, w, h;
    int cols, rows;
    int cell, gap;
    int gridX, gridY;
    int count;             // how many items are offered
    Rectangle clearRect;   // the "ANY ITEM" button
} FilterPickerLayout;

// The `i`-th item on offer. Index 0 is ITEM_WOOD — slot 0 of the
// enum is the "nothing" sentinel and isn't a thing you can filter for.
static ItemID UiFilterItemAt(int i) {
    ItemID id = (ItemID)(i + 1);
    return (id > ITEM_NONE && id < ITEM_COUNT) ? id : ITEM_NONE;
}

static void UiGetFilterPickerLayout(FilterPickerLayout *l) {
    l->count = ITEM_COUNT - 1;
    l->cols  = UI_FILTER_COLS;
    l->rows  = (l->count + l->cols - 1) / l->cols;
    l->cell  = 46;
    l->gap   = 6;

    int gridW = l->cols * l->cell + (l->cols - 1) * l->gap;
    int gridH = l->rows * l->cell + (l->rows - 1) * l->gap;
    l->w = gridW + 32;
    l->h = UI_HEADER_H + 14 + gridH + 46 + 14;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (l->w > sw - 16) l->w = sw - 16;
    if (l->h > sh - 16) l->h = sh - 16;
    l->x = (sw - l->w) / 2;
    l->y = (sh - l->h) / 2;
    l->gridX = l->x + (l->w - gridW) / 2;
    l->gridY = l->y + UI_HEADER_H + 12;
    l->clearRect = (Rectangle){ (float)(l->x + 16), (float)(l->y + l->h - 50),
                                (float)(l->w - 32), 36 };
}

static Rectangle UiFilterCellRect(const FilterPickerLayout *l, int i) {
    int c = i % l->cols, r = i / l->cols;
    return (Rectangle){ (float)(l->gridX + c * (l->cell + l->gap)),
                        (float)(l->gridY + r * (l->cell + l->gap)),
                        (float)l->cell, (float)l->cell };
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
        bool dragging = (uiDragKind == DRAG_MACHINE && i == uiDragIndex);
        DrawRectangleRec(r, dragging ? (Color){ 96, 70, 24, 255 }
                            : (hovered ? UI_SLOT_HOVER : UI_SLOT_BG));
        DrawRectangleLinesEx(r, (hovered || dragging) ? 2 : 1,
                             (hovered || dragging) ? UI_ACCENT : UI_SLOT_BORDER);
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

    if (l.hasFilter) {
        // The filter slot: empty means "handle anything". Clicking it
        // opens the item grid; right-clicking it clears the rule.
        bool hovered = CheckCollisionPointRec(GetMousePosition(), l.filterRect);
        bool set = (m->filter != ITEM_NONE);
        DrawRectangleRec(l.filterRect, hovered ? UI_SLOT_HOVER : UI_SLOT_BG);
        DrawRectangleLinesEx(l.filterRect, hovered ? 2 : 1,
                             set ? UI_ACCENT : UI_SLOT_BORDER);
        if (set) {
            DrawItemSprite(m->filter, l.filterRect.x + 7, l.filterRect.y + 7, 30);
        } else {
            DrawText("ANY", (int)l.filterRect.x + 8, (int)l.filterRect.y + 15, 15,
                     (Color){ 150, 150, 158, 255 });
        }

        int tx = (int)(l.filterRect.x + l.filterRect.width + 12);
        int ty = (int)(l.filterRect.y + 4);
        DrawText("FILTER", tx, ty, 14, UI_ACCENT);
        // Two machines, two meanings — spell out which one this is.
        const char *line = (m->type == TILE_SPLITTER)
            ? (set ? "matches go LEFT" : "alternates L / R")
            : (set ? "only picks this up" : "picks up anything");
        DrawText(line, tx, ty + 18, 12, (Color){ 205, 210, 222, 255 });
        DrawText(set ? "click: change   RMB: clear" : "click to choose an item",
                 tx, ty + 33, 11, (Color){ 150, 156, 168, 255 });
    }
}

// The item grid itself, drawn over everything else the panel row has.
static void UiDrawFilterPicker(void) {
    if (!machineFilterPickerOpen) return;
    Machine *m = MachineAt(machineUiX, machineUiY);
    if (m == NULL || !TileHasFilter(m->type)) return;

    FilterPickerLayout l = { 0 };
    UiGetFilterPickerLayout(&l);

    // A dim behind it, because unlike the docked panels this one is
    // MODAL: it wants the next click, whatever else is on screen.
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 4, 4, 8, 150 });
    UiDrawPanelFrame(l.x, l.y, l.w, l.h, "SET FILTER");
    UiDrawCloseButton(l.x, l.y, l.w);

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < l.count; i++) {
        ItemID id = UiFilterItemAt(i);
        if (id == ITEM_NONE) continue;
        Rectangle r = UiFilterCellRect(&l, i);
        if (r.y + r.height > l.clearRect.y - 6) break;   // ran out of panel
        bool hovered = CheckCollisionPointRec(mouse, r);
        bool chosen  = (m->filter == id);
        DrawRectangleRec(r, hovered ? UI_SLOT_HOVER : UI_SLOT_BG);
        DrawRectangleLinesEx(r, (hovered || chosen) ? 2 : 1,
                             chosen ? UI_ACCENT : UI_SLOT_BORDER);
        DrawItemSprite(id, r.x + 7, r.y + 5, 32);
        // The name only appears under the cursor: 36 labels at once is
        // an unreadable wall, one label is a tooltip.
        if (hovered) {
            const char *nm = ITEMS[id].name;
            int nw = MeasureText(nm, 12);
            int bx = (int)(r.x + r.width / 2) - nw / 2 - 4;
            if (bx < l.x + 4) bx = l.x + 4;
            if (bx + nw + 8 > l.x + l.w - 4) bx = l.x + l.w - 12 - nw;
            DrawRectangle(bx, (int)r.y - 18, nw + 8, 16, (Color){ 16, 16, 22, 235 });
            DrawText(nm, bx + 4, (int)r.y - 16, 12, RAYWHITE);
        }
    }

    bool clearHov = CheckCollisionPointRec(mouse, l.clearRect);
    bool none = (m->filter == ITEM_NONE);
    DrawRectangleRec(l.clearRect, clearHov ? (Color){ 66, 66, 70, 255 } : UI_SLOT_BG);
    DrawRectangleLinesEx(l.clearRect, 1, none ? UI_ACCENT : UI_SLOT_BORDER);
    const char *label = "NO FILTER  (handle any item)";
    DrawText(label, (int)(l.clearRect.x + l.clearRect.width / 2) - MeasureText(label, 15) / 2,
             (int)l.clearRect.y + 10, 15, none ? UI_ACCENT : RAYWHITE);
}

// ─── Creative item picker (C, creative mode only) ─────────
// Deliberately the SAME grid as the inserter's filter picker — same
// layout function, same cell size, same hover tooltip — because
// "pick an item from every item in the game" is the same question
// twice, and a second look for it would just be a second thing to
// learn. The only difference is what a click means: there it sets a
// filter, here it drops a full stack in your pack.
static void UiDrawCreativePicker(const Player *p) {
    if (!devCreativePickerOpen) return;

    FilterPickerLayout l = { 0 };
    UiGetFilterPickerLayout(&l);

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 4, 4, 8, 150 });
    UiDrawPanelFrame(l.x, l.y, l.w, l.h, "CREATIVE — TAKE A STACK");
    UiDrawCloseButton(l.x, l.y, l.w);

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < l.count; i++) {
        ItemID id = UiFilterItemAt(i);
        if (id == ITEM_NONE) continue;
        Rectangle r = UiFilterCellRect(&l, i);
        if (r.y + r.height > l.clearRect.y - 6) break;
        bool hovered = CheckCollisionPointRec(mouse, r);
        bool owned   = (p->inventory[id] > 0);
        DrawRectangleRec(r, hovered ? UI_SLOT_HOVER : UI_SLOT_BG);
        DrawRectangleLinesEx(r, hovered ? 2 : 1, hovered ? UI_ACCENT : UI_SLOT_BORDER);
        DrawItemSprite(id, r.x + 7, r.y + 5, 32);
        // A faint count in the corner of things you already carry —
        // the picker doubles as "what have I got?".
        if (owned) {
            DrawText(TextFormat("%d", p->inventory[id]),
                     (int)r.x + 4, (int)(r.y + r.height) - 12, 10, (Color){ 200, 200, 205, 190 });
        }
        if (hovered) {
            const char *nm = ITEMS[id].name;
            int nw = MeasureText(nm, 12);
            int bx = (int)(r.x + r.width / 2) - nw / 2 - 4;
            if (bx < l.x + 4) bx = l.x + 4;
            if (bx + nw + 8 > l.x + l.w - 4) bx = l.x + l.w - 12 - nw;
            DrawRectangle(bx, (int)r.y - 18, nw + 8, 16, (Color){ 16, 16, 22, 235 });
            DrawText(nm, bx + 4, (int)r.y - 16, 12, RAYWHITE);
        }
    }

    const char *hint = TextFormat("[CLICK] take %d   [SHIFT+CLICK] take 1   [RMB/ESC/C] close",
                                  STACK_MAX);
    DrawText(hint, (int)(l.clearRect.x + l.clearRect.width / 2) - MeasureText(hint, 14) / 2,
             (int)l.clearRect.y + 12, 14, (Color){ 190, 190, 196, 255 });
}

// ─── Map painter palette ──────────────────────────────────
// A strip along the bottom holding every tile in the game. Same
// shared-rectangle rule as the rest of this file: the layout math
// lives here once, and main.c hit-tests the exact rectangles it
// draws, so a click can never land somewhere other than where it
// looks.
#define UI_MAPEDIT_BAR_H 84

static Rectangle UiMapEditBarRect(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    return (Rectangle){ 0, (float)(sh - UI_MAPEDIT_BAR_H), (float)sw, (float)UI_MAPEDIT_BAR_H };
}

// Swatch `i` is tile `i`. Cells shrink to fit rather than wrapping —
// one unbroken row stays readable as "the palette", and every tile
// keeps the same place in it whatever the window size.
static Rectangle UiMapEditSwatchRect(int i) {
    Rectangle bar = UiMapEditBarRect();
    int n = TILE_COUNT;
    float gap = 4;
    float avail = bar.width - 28;
    float cell = (avail - gap * (n - 1)) / n;
    if (cell > 42) cell = 42;
    if (cell < 10) cell = 10;
    float rowW = cell * n + gap * (n - 1);
    float x0 = bar.x + (bar.width - rowW) / 2;
    return (Rectangle){ x0 + i * (cell + gap), bar.y + 30, cell, cell };
}

// Which swatch is under the cursor? -1 for none.
static int UiMapEditSwatchAt(Vector2 mouse) {
    for (int i = 0; i < TILE_COUNT; i++) {
        if (CheckCollisionPointRec(mouse, UiMapEditSwatchRect(i))) return i;
    }
    return -1;
}

static void UiDrawMapEditBar(void) {
    if (!devMapEdit) return;
    Rectangle bar = UiMapEditBarRect();
    DrawRectangleRec(bar, (Color){ 10, 14, 12, 238 });
    DrawLine((int)bar.x, (int)bar.y, (int)(bar.x + bar.width), (int)bar.y,
             (Color){ 90, 255, 140, 255 });

    DrawText(TextFormat("MAP PAINTER   brush: %s   size: %dx%d   facing: %s",
                        TILES[devBrushTile].name,
                        devBrushRadius * 2 + 1, devBrushRadius * 2 + 1,
                        (devBrushDir == 0) ? "E" : (devBrushDir == 1) ? "S" :
                        (devBrushDir == 2) ? "W" : "N"),
             (int)bar.x + 14, (int)bar.y + 8, 14, (Color){ 90, 255, 140, 255 });

    const char *keys = "[LMB] paint  [RMB] erase  [ / ] brush size  [R] facing  "
                       "[WHEEL] zoom (in past the line = back to playing)";
    DrawText(keys, (int)(bar.x + bar.width) - MeasureText(keys, 12) - 14,
             (int)bar.y + 10, 12, (Color){ 70, 150, 100, 255 });

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < TILE_COUNT; i++) {
        Rectangle r = UiMapEditSwatchRect(i);
        bool hovered  = CheckCollisionPointRec(mouse, r);
        bool selected = (i == (int)devBrushTile);
        DrawRectangleRec(r, TILES[i].color);
        DrawRectangleLinesEx(r, selected ? 3 : (hovered ? 2 : 1),
                             selected ? (Color){ 255, 200, 60, 255 }
                                      : (hovered ? RAYWHITE : (Color){ 45, 90, 62, 255 }));
        if (hovered) {
            const char *nm = TILES[i].name;
            int nw = MeasureText(nm, 12);
            int bx = (int)(r.x + r.width / 2) - nw / 2 - 4;
            if (bx < 4) bx = 4;
            if (bx + nw + 8 > GetScreenWidth() - 4) bx = GetScreenWidth() - 12 - nw;
            DrawRectangle(bx, (int)r.y - 18, nw + 8, 16, (Color){ 16, 16, 22, 235 });
            DrawText(nm, bx + 4, (int)r.y - 16, 12, RAYWHITE);
        }
    }
}

// ─── The saves screen ─────────────────────────────────────
// Eight worlds as a grid of CARDS. A card is a thumbnail with a name
// you can type into and two lines of "which one is this?" — the date
// it was written and how long it's been played. Empty slots are
// drawn as empty slots rather than hidden, because "there is room
// for another world here" is information too.
//
// Same shared-layout rule as every other panel: this file computes
// the rectangles, main.c hit-tests the SAME rectangles, so a click
// can never land somewhere other than where it looks.
#define UI_SAVE_COLS 4
#define UI_SAVE_ROWS ((SAVE_SLOTS + UI_SAVE_COLS - 1) / UI_SAVE_COLS)

typedef struct {
    int x, y, w, h;         // the framed panel
    int cardW, cardH, gap;
    int gridX, gridY;
    int thumbH;             // height of the preview strip on a card
} SavesLayout;

static void UiGetSavesLayout(SavesLayout *l) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    l->gap = 14;
    l->cardW = 250;
    l->cardH = 196;
    // Shrink the cards rather than the grid when the window is small,
    // so all eight worlds stay on one screen at any size.
    int availW = sw - 60 - (UI_SAVE_COLS - 1) * l->gap;
    if (l->cardW * UI_SAVE_COLS > availW) l->cardW = availW / UI_SAVE_COLS;
    if (l->cardW < 130) l->cardW = 130;
    int availH = sh - 190 - (UI_SAVE_ROWS - 1) * l->gap;
    if (l->cardH * UI_SAVE_ROWS > availH) l->cardH = availH / UI_SAVE_ROWS;
    if (l->cardH < 120) l->cardH = 120;
    l->thumbH = l->cardH - 62;          // the rest is name + two info lines
    if (l->thumbH < 40) l->thumbH = 40;

    int gridW = UI_SAVE_COLS * l->cardW + (UI_SAVE_COLS - 1) * l->gap;
    int gridH = UI_SAVE_ROWS * l->cardH + (UI_SAVE_ROWS - 1) * l->gap;
    l->w = gridW + 32;
    l->h = gridH + UI_HEADER_H + 34;
    l->x = (sw - l->w) / 2;
    l->y = (sh - l->h) / 2 - 20;
    if (l->y < 8) l->y = 8;
    l->gridX = l->x + 16;
    l->gridY = l->y + UI_HEADER_H + 14;
}

static Rectangle UiSaveCardRect(const SavesLayout *l, int slot) {
    int c = slot % UI_SAVE_COLS, r = slot / UI_SAVE_COLS;
    return (Rectangle){ (float)(l->gridX + c * (l->cardW + l->gap)),
                        (float)(l->gridY + r * (l->cardH + l->gap)),
                        (float)l->cardW, (float)l->cardH };
}

// The name strip: click it to rename, rather than the card body,
// which loads. Two targets on one card, and they never overlap.
static Rectangle UiSaveNameRect(const SavesLayout *l, int slot) {
    Rectangle c = UiSaveCardRect(l, slot);
    return (Rectangle){ c.x + 4, c.y + l->thumbH + 4, c.width - 34, 21 };
}

// The little [x]. It takes TWO clicks to delete — see the confirm
// state in main.c — because a world is hours of someone's evening.
static Rectangle UiSaveDeleteRect(const SavesLayout *l, int slot) {
    Rectangle c = UiSaveCardRect(l, slot);
    return (Rectangle){ c.x + c.width - 27, c.y + l->thumbH + 4, 23, 21 };
}

// `renaming` is the slot currently being typed into (-1 = none) and
// `nameBuf` is the live text; `confirmDelete` is the slot showing its
// second-click warning. All three live in main.c — the UI draws the
// state, it doesn't own it.
static void UiDrawSavesScreen(int renaming, const char *nameBuf, int confirmDelete) {
    SavesLayout l = { 0 };
    UiGetSavesLayout(&l);
    int sw = GetScreenWidth();

    DrawRectangle(0, 0, sw, GetScreenHeight(), (Color){ 4, 6, 10, 225 });
    UiDrawPanelFrame(l.x, l.y, l.w, l.h, "SAVED WORLDS");

    Vector2 mouse = GetMousePosition();
    for (int slot = 0; slot < SAVE_SLOTS; slot++) {
        Rectangle card = UiSaveCardRect(&l, slot);
        Rectangle name = UiSaveNameRect(&l, slot);
        Rectangle del  = UiSaveDeleteRect(&l, slot);
        const SaveSlot *s = &saveSlots[slot];
        bool hovered  = CheckCollisionPointRec(mouse, card);
        bool onName   = CheckCollisionPointRec(mouse, name);
        bool onDelete = CheckCollisionPointRec(mouse, del);
        bool editing  = (renaming == slot);
        bool warning  = (confirmDelete == slot);

        if (s->blocked) {
            // A file we couldn't read. Deliberately does NOT look
            // like an empty slot and doesn't respond to hover: the
            // whole point is that clicking it must not feel available.
            DrawRectangleRec(card, (Color){ 46, 38, 30, 255 });
            DrawRectangleLinesEx(card, 1, (Color){ 128, 96, 52, 255 });
            const char *a = "UNREADABLE SAVE";
            const char *b = "written by a different version";
            const char *c = "left alone, not overwritten";
            DrawText(a, (int)(card.x + card.width / 2) - MeasureText(a, 15) / 2,
                     (int)(card.y + card.height / 2) - 24, 15, (Color){ 232, 176, 96, 255 });
            DrawText(b, (int)(card.x + card.width / 2) - MeasureText(b, 11) / 2,
                     (int)(card.y + card.height / 2) - 2, 11, (Color){ 176, 152, 120, 255 });
            DrawText(c, (int)(card.x + card.width / 2) - MeasureText(c, 11) / 2,
                     (int)(card.y + card.height / 2) + 14, 11, (Color){ 148, 128, 102, 255 });
            continue;
        }
        if (!s->used) {
            // An empty berth, drawn faintly — a place a world could
            // go, which is exactly what it is.
            DrawRectangleRec(card, (Color){ 38, 38, 42, 255 });
            DrawRectangleLinesEx(card, hovered ? 2 : 1,
                                 hovered ? UI_ACCENT : (Color){ 74, 74, 80, 255 });
            const char *a = "EMPTY SLOT";
            const char *b = "click to start a world here";
            DrawText(a, (int)(card.x + card.width / 2) - MeasureText(a, 16) / 2,
                     (int)(card.y + card.height / 2) - 16, 16,
                     hovered ? RAYWHITE : (Color){ 132, 132, 140, 255 });
            DrawText(b, (int)(card.x + card.width / 2) - MeasureText(b, 11) / 2,
                     (int)(card.y + card.height / 2) + 6, 11, (Color){ 118, 118, 126, 255 });
            continue;
        }

        DrawRectangleRec(card, warning ? (Color){ 62, 26, 26, 255 } : UI_SLOT_BG);

        // The thumbnail, letterboxed into the strip so a square
        // preview never stretches into a rectangle.
        Rectangle strip = { card.x + 4, card.y + 4, card.width - 8, (float)l.thumbH - 4 };
        DrawRectangleRec(strip, (Color){ 12, 12, 18, 255 });
        if (s->hasThumb) {
            float side = (strip.width < strip.height) ? strip.width : strip.height;
            Rectangle dst = { strip.x + (strip.width - side) / 2,
                              strip.y + (strip.height - side) / 2, side, side };
            DrawTexturePro(s->thumb,
                           (Rectangle){ 0, 0, (float)SAVE_PREVIEW_DIM, (float)SAVE_PREVIEW_DIM },
                           dst, (Vector2){ 0, 0 }, 0, WHITE);
        }
        DrawRectangleLinesEx(strip, 1, (Color){ 70, 70, 78, 255 });

        // The name strip doubles as the text field.
        DrawRectangleRec(name, editing ? (Color){ 24, 40, 60, 255 }
                                       : (onName ? UI_SLOT_HOVER : (Color){ 34, 34, 38, 255 }));
        DrawRectangleLinesEx(name, editing ? 2 : 1,
                             editing ? UI_ACCENT : (onName ? UI_SLOT_BORDER
                                                           : (Color){ 58, 58, 64, 255 }));
        const char *label = editing ? nameBuf : s->head.name;
        BeginScissorMode((int)name.x + 3, (int)name.y + 2, (int)name.width - 6, (int)name.height - 4);
        DrawText(label, (int)name.x + 6, (int)name.y + 4, 15, RAYWHITE);
        if (editing && ((int)(GetTime() * 2)) % 2 == 0) {
            DrawText("_", (int)name.x + 8 + MeasureText(label, 15), (int)name.y + 4, 15, UI_ACCENT);
        }
        EndScissorMode();

        DrawRectangleRec(del, onDelete ? (Color){ 170, 62, 62, 255 } : (Color){ 48, 48, 54, 255 });
        DrawRectangleLinesEx(del, 1, onDelete ? RAYWHITE : (Color){ 76, 76, 84, 255 });
        DrawText("x", (int)del.x + 8, (int)del.y + 3, 15, RAYWHITE);

        if (warning) {
            const char *w = "click x again to DELETE";
            DrawText(w, (int)(card.x + card.width / 2) - MeasureText(w, 12) / 2,
                     (int)(card.y + l.thumbH + 32), 12, (Color){ 255, 150, 150, 255 });
        } else {
            DrawText(SaveStampText(s->head.stamp), (int)card.x + 6,
                     (int)(card.y + l.thumbH + 30), 12, (Color){ 196, 202, 214, 255 });
            DrawText(SavePlayTimeText(s->head.playSeconds), (int)card.x + 6,
                     (int)(card.y + l.thumbH + 46), 12, (Color){ 146, 152, 164, 255 });
        }

        // The card border is the hint: orange when this click loads it.
        bool loadable = hovered && !onName && !onDelete;
        DrawRectangleLinesEx(card, loadable ? 2 : 1,
                             loadable ? UI_ACCENT : (Color){ 88, 88, 96, 255 });
    }

    // Plain ASCII: raylib's built-in font has no dashes or bullets.
    const char *hint = (renaming >= 0)
        ? "typing a name  |  ENTER to keep it, ESC to cancel"
        : "click a world to play it  |  click its name to rename  |  x to delete";
    DrawText(hint, sw / 2 - MeasureText(hint, 14) / 2, l.y + l.h + 12, 14,
             (Color){ 150, 158, 172, 255 });
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
// TEN rows, not six: a chain craft can queue half a dozen
// intermediates on its own, and every one of them needs to be
// visible long enough that you can still hit its [x].
#define UI_QUEUE_ROW_H 32
#define UI_QUEUE_MAX_SHOWN 10

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
        // A build whose ingredients haven't been taken yet is an
        // INTERMEDIATE: the entry in front of it is making its parts.
        bool waiting = !craftPaid[i];

        DrawRectangleRec(r, waiting ? (Color){ 14, 16, 26, 215 } : (Color){ 10, 10, 16, 215 });
        if (frac > 0) {
            DrawRectangle((int)r.x, (int)r.y, (int)(r.width * frac), (int)r.height,
                          (Color){ 60, 110, 70, 235 });
        }
        DrawRectangleLinesEx(r, 1, i == 0 ? UI_ACCENT
                                          : (waiting ? (Color){ 92, 110, 150, 255 }
                                                     : UI_PANEL_BORDER));
        DrawItemSprite(id, r.x + 4, r.y + 3, 25);
        DrawText(ITEMS[id].name, (int)r.x + 36, (int)r.y + 3, 14,
                 waiting ? (Color){ 186, 198, 224, 255 } : RAYWHITE);
        DrawText(i == 0 ? TextFormat("%.1fs", need - p->craftProgress)
                        : (waiting ? "needs parts first" : "queued"),
                 (int)r.x + 36, (int)r.y + 18, 11, (Color){ 200, 210, 225, 220 });

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
