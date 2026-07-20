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

// ─── Hotbar ───────────────────────────────────────────────
// One slot per real item type (we skip slot 0, ITEM_NONE).
// Because it loops to ITEM_COUNT, the hotbar resizes itself
// whenever you add or remove items in gamedata.h.
static void UiDrawHotbar(const Player *p) {
    const float slot = 64, gap = 6;
    int   n      = ITEM_COUNT - 1;                  // skip ITEM_NONE
    float barW   = n * slot + (n - 1) * gap;
    float x      = (GetScreenWidth() - barW) / 2.0f;
    float y      = GetScreenHeight() - slot - 20.0f;

    for (ItemID id = 1; id < ITEM_COUNT; id++) {
        Rectangle r   = { x, y, slot, slot };
        bool      sel = (id == p->selected);

        DrawRectangleRounded(r, 0.3f, 8,
            sel ? (Color){45,80,130,230} : (Color){15,15,25,200});
        DrawRectangleRoundedLines(r, 0.3f, 8,
            sel ? (Color){120,200,255,255} : (Color){70,70,90,200});

        // Icon = a colored square from the table; gray if you own none.
        Color c = ITEMS[id].color;
        if (p->inventory[id] == 0) c = (Color){60,60,60,255};
        DrawRectangle((int)(x+slot/2-14), (int)(y+slot/2-14), 28, 28, c);

        DrawText(TextFormat("%d", id), (int)x+5, (int)y+4, 14, GOLD);
        DrawText(TextFormat("%d", p->inventory[id]),
                 (int)(x+slot-24), (int)(y+slot-20), 15, WHITE);
        x += slot + gap;
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
    DrawText("WASD move | LMB mine | RMB place | 1-9 select | TAB craft | F5 save | F9 load",
             15, 12, 18, (Color){255,255,255,180});
}

#endif // UI_H
