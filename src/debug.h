#ifndef DEBUG_H
#define DEBUG_H
// ============================================================
//  DEBUG.H — the F3 DEBUG CONSOLE: every item, every tile, and
//  every global tunable in the game, adjustable LIVE with sliders
//  while you play. Pause-free balancing: crank the pickaxe to 40
//  DPS, walk over to a rock, feel the difference, dial it back.
//
//  Owns: the console's open/scroll/selection state, plus a backup
//  of the "factory default" tables so RESET can restore them.
//
//  NOTE ON STYLE — unlike the rest of the game, this file uses
//  "immediate-mode UI": each widget function READS the mouse and
//  DRAWS ITSELF in the same call, once per frame, with no stored
//  widget objects. That deliberately breaks the game's usual
//  update/draw split, because debug tools optimize for "adding a
//  slider is ONE line". (This is how the famous Dear ImGui library
//  works; sliders identified by their value's memory address is
//  the same trick it uses for widget identity.)
//
//  Changes made here are LIVE MEMORY EDITS: they are not written
//  to the save file and vanish when the game closes. That's a
//  feature — experiment freely, RESET (or restart) to undo.
// ============================================================

#include <string.h>     // memcpy for the defaults backup
#include "raylib.h"
#include "config.h"
#include "gamedata.h"
#include "player.h"
#include "entities.h"   // mob/raid readouts + debug action buttons

// ─── Console state ────────────────────────────────────────
static bool debugMenuOpen   = false;
static int  debugListSel    = 0;      // selected registry entry
static int  debugListScroll = 0;      // first visible registry row
// Which slider is being dragged? Identified by the ADDRESS of the
// value it edits — no two sliders edit the same variable, so the
// pointer doubles as a perfectly unique widget id.
static const void *debugActiveSlider = NULL;

// The registry list = three GLOBAL pages, then every tile, then
// every item (skipping ITEM_NONE).
#define DEBUG_GLOBAL_PAGES       5
#define DEBUG_ENTRY_TILES_START  DEBUG_GLOBAL_PAGES
#define DEBUG_ENTRY_ITEMS_START  (DEBUG_GLOBAL_PAGES + TILE_COUNT)
#define DEBUG_ENTRY_COUNT        (DEBUG_GLOBAL_PAGES + TILE_COUNT + (ITEM_COUNT - 1))

// ─── Factory defaults (for the RESET button) ──────────────
static ItemInfo debugDefaultItems[ITEM_COUNT];
static TileInfo debugDefaultTiles[TILE_COUNT];
static Tuning   debugDefaultTune;
static bool     debugDefaultsSaved = false;

static void DebugSaveDefaultsOnce(void) {
    if (debugDefaultsSaved) return;
    memcpy(debugDefaultItems, ITEMS, sizeof(ITEMS));
    memcpy(debugDefaultTiles, TILES, sizeof(TILES));
    debugDefaultTune = TUNE;
    debugDefaultsSaved = true;
}

static void DebugResetAll(void) {
    memcpy(ITEMS, debugDefaultItems, sizeof(ITEMS));
    memcpy(TILES, debugDefaultTiles, sizeof(TILES));
    TUNE = debugDefaultTune;
}

// ─── Console palette (green-on-black terminal look) ───────
#define DBG_BG      (Color){   8,  12,  10, 246 }
#define DBG_PANEL   (Color){  13,  22,  17, 250 }
#define DBG_LINE    (Color){  45,  90,  62, 255 }
#define DBG_GREEN   (Color){  90, 255, 140, 255 }
#define DBG_DIM     (Color){  70, 150, 100, 255 }
#define DBG_FAINT   (Color){  45,  95,  65, 255 }
#define DBG_ACCENT  (Color){ 255, 200,  60, 255 }
#define DBG_RED     (Color){ 255,  90,  90, 255 }

// ─── The slider widget ────────────────────────────────────
// Split into three small pieces (track rect, input, drawing) so the
// typed wrappers below (float/int/byte/item/tile) stay tiny.

static Rectangle DebugSliderTrack(Rectangle row) {
    return (Rectangle){ row.x + row.width * 0.42f,
                        row.y + row.height * 0.5f - 3,
                        row.width * 0.38f, 6 };
}

// Handle mouse input for one slider. Takes and returns the value as
// a 0..1 position along the track; `id` is the edited variable's
// address (see debugActiveSlider above).
static float DebugSliderGrab(Rectangle row, const void *id, float t01) {
    Rectangle track = DebugSliderTrack(row);
    Rectangle grabZone = { track.x - 6, row.y, track.width + 12, row.height };
    Vector2 m = GetMousePosition();
    if (CheckCollisionPointRec(m, grabZone) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        debugActiveSlider = id;
    }
    if (debugActiveSlider == id) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            t01 = (m.x - track.x) / track.width;   // follow the mouse
        } else {
            debugActiveSlider = NULL;              // released → let go
        }
    }
    if (t01 < 0) t01 = 0;
    if (t01 > 1) t01 = 1;
    return t01;
}

static void DebugSliderDraw(Rectangle row, const char *label, float t01,
                            const char *valueText, bool active) {
    Rectangle track = DebugSliderTrack(row);
    DrawText(label, (int)row.x, (int)(row.y + row.height / 2 - 7), 14,
             active ? RAYWHITE : DBG_DIM);
    DrawRectangleRec(track, (Color){ 28, 50, 38, 255 });
    DrawRectangle((int)track.x, (int)track.y, (int)(t01 * track.width), 6,
                  active ? DBG_ACCENT : DBG_GREEN);
    DrawRectangle((int)(track.x + t01 * track.width - 3), (int)row.y + 3,
                  6, (int)row.height - 6, active ? RAYWHITE : DBG_GREEN);
    DrawText(valueText, (int)(track.x + track.width + 12),
             (int)(row.y + row.height / 2 - 7), 14, DBG_ACCENT);
}

// Typed wrappers — each maps its value to/from the 0..1 track.
static void DebugSliderFloat(Rectangle row, const char *label, float *v,
                             float min, float max, const char *fmt) {
    float t = (max > min) ? (*v - min) / (max - min) : 0;
    t = DebugSliderGrab(row, v, t);
    *v = min + t * (max - min);
    DebugSliderDraw(row, label, t, TextFormat(fmt, *v), debugActiveSlider == v);
}

static void DebugSliderInt(Rectangle row, const char *label, int *v,
                           int min, int max) {
    float t = (max > min) ? (float)(*v - min) / (float)(max - min) : 0;
    t = DebugSliderGrab(row, v, t);
    *v = min + (int)(t * (max - min) + 0.5f);      // snap to whole numbers
    t = (max > min) ? (float)(*v - min) / (float)(max - min) : 0;  // knob snaps too
    DebugSliderDraw(row, label, t, TextFormat("%d", *v), debugActiveSlider == v);
}

// Color channels are unsigned char (0..255), not int — they need
// their own wrapper so the slider writes the right-sized memory.
static void DebugSliderByte(Rectangle row, const char *label, unsigned char *v) {
    float t = *v / 255.0f;
    t = DebugSliderGrab(row, v, t);
    *v = (unsigned char)(t * 255.0f + 0.5f);
    DebugSliderDraw(row, label, *v / 255.0f, TextFormat("%d", *v), debugActiveSlider == v);
}

// Item picker as a slider: drag through the whole ITEMS table by id
// and read the name. 0 = "Nothing" (which for a recipe input means
// "no ingredient / not craftable" — yes, you can delete recipes).
static void DebugSliderItem(Rectangle row, const char *label, ItemID *v) {
    int i = (int)*v;
    float t = (float)i / (float)(ITEM_COUNT - 1);
    t = DebugSliderGrab(row, v, t);
    i = (int)(t * (ITEM_COUNT - 1) + 0.5f);
    *v = (ItemID)i;
    DebugSliderDraw(row, label, (float)i / (float)(ITEM_COUNT - 1),
                    ITEMS[i].name, debugActiveSlider == v);
}

static void DebugSliderTile(Rectangle row, const char *label, TileType *v) {
    int i = (int)*v;
    float t = (float)i / (float)(TILE_COUNT - 1);
    t = DebugSliderGrab(row, v, t);
    i = (int)(t * (TILE_COUNT - 1) + 0.5f);
    *v = (TileType)i;
    DebugSliderDraw(row, label, (float)i / (float)(TILE_COUNT - 1),
                    TILES[i].name, debugActiveSlider == v);
}

// ─── Small widgets ────────────────────────────────────────
static void DebugToggle(Rectangle row, const char *label, bool *v) {
    Rectangle box = { row.x + row.width * 0.42f, row.y + 3, 56, row.height - 6 };
    Vector2 m = GetMousePosition();
    if (CheckCollisionPointRec(m, box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        *v = !*v;
    }
    DrawText(label, (int)row.x, (int)(row.y + row.height / 2 - 7), 14, DBG_DIM);
    DrawRectangleRec(box, *v ? (Color){ 20, 70, 40, 255 } : (Color){ 60, 26, 26, 255 });
    DrawRectangleLinesEx(box, 1, *v ? DBG_GREEN : DBG_RED);
    DrawText(*v ? "ON" : "OFF", (int)box.x + 16, (int)box.y + 3, 14, *v ? DBG_GREEN : DBG_RED);
}

static bool DebugButton(Rectangle r, const char *text, Color col) {
    Vector2 m = GetMousePosition();
    bool hovered = CheckCollisionPointRec(m, r);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    DrawRectangleRec(r, hovered ? (Color){ 40, 60, 45, 255 } : (Color){ 20, 32, 25, 255 });
    DrawRectangleLinesEx(r, 1, col);
    DrawText(text, (int)(r.x + (r.width - MeasureText(text, 14)) / 2),
             (int)(r.y + r.height / 2 - 7), 14, col);
    return clicked;
}

// A labeled section divider; returns the y to continue laying out at.
static float DebugSection(float x, float y, float w, const char *title) {
    DrawText(title, (int)x, (int)y, 13, DBG_ACCENT);
    int tw = MeasureText(title, 13);
    DrawLine((int)(x + tw + 10), (int)(y + 7), (int)(x + w), (int)(y + 7), DBG_LINE);
    return y + 22;
}

// ─── The three attribute panels ───────────────────────────
// Each lays out rows top to bottom with a running y cursor.
#define DBG_ROW_H 27.0f

static void DebugDrawGlobalWeaponsPanel(Rectangle panel) {
    float x = panel.x + 16, w = panel.width - 32, y = panel.y + 14;
    Rectangle row;

    DrawText("PLAYER & WEAPONS", (int)x, (int)y, 26, DBG_GREEN);
    DrawText("engine-wide values (the config.h numbers, live)", (int)x, (int)y + 30, 13, DBG_FAINT);
    y += 56;

    y = DebugSection(x, y, w, "PLAYER");
    row = (Rectangle){ x, y, w, DBG_ROW_H };
    DebugSliderFloat(row, "Move speed", &TUNE.playerSpeed, 50, 1200, "%.0f px/s"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Reach", &TUNE.playerReach, 20, 400, "%.0f px"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Hand mining", &TUNE.handDPS, 0, 40, "%.1f dps"); y += DBG_ROW_H;

    y = DebugSection(x, y + 8, w, "SLINGSHOT");
    row.y = y; DebugSliderFloat(row, "Range", &TUNE.slingshotReach, 20, 600, "%.0f px"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Stone speed", &TUNE.slingshotSpeed, 100, 3000, "%.0f px/s"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Stone damage", &TUNE.slingshotDamage, 0, 40, "%.1f"); y += DBG_ROW_H;

    y = DebugSection(x, y + 8, w, "GUNS (pistol / SMG / shotgun share bullets)");
    row.y = y; DebugSliderFloat(row, "Range", &TUNE.pistolReach, 20, 1000, "%.0f px"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Bullet speed", &TUNE.pistolBulletSpeed, 100, 4000, "%.0f px/s"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Bullet damage", &TUNE.pistolBulletDamage, 0, 60, "%.1f"); y += DBG_ROW_H;
}

static void DebugDrawGlobalMobsPanel(Rectangle panel) {
    float x = panel.x + 16, w = panel.width - 32, y = panel.y + 14;
    Rectangle row;

    DrawText("MOBS & RAIDS", (int)x, (int)y, 26, DBG_GREEN);
    DrawText(TextFormat("evolution %.0f%%   population %d   played %.0fs",
                        EvolutionFactor() * 100, MobCount(), entGameTime),
             (int)x, (int)y + 30, 13, DBG_ACCENT);
    y += 56;

    y = DebugSection(x, y, w, "MOB STATS (scale further with evolution)");
    row = (Rectangle){ x, y, w, DBG_ROW_H };
    DebugSliderFloat(row, "Base health", &TUNE.mobHp, 1, 200, "%.0f hp"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Speed", &TUNE.mobSpeed, 20, 400, "%.0f px/s"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Contact damage", &TUNE.mobContactDPS, 0, 60, "%.1f dps"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Wall chewing", &TUNE.mobChewDPS, 0, 40, "%.1f dps"); y += DBG_ROW_H;

    y = DebugSection(x, y + 8, w, "ESCALATION");
    row.y = y; DebugSliderFloat(row, "Evolution time", &TUNE.evolutionMinutes, 2, 90, "%.0f min"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Wave size", &TUNE.waveSize, 2, 20, "%.0f mobs"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Spawn cd (evo 0)", &TUNE.spawnIntervalStart, 1, 60, "%.0f s"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Spawn cd (evo 1)", &TUNE.spawnIntervalEnd, 1, 60, "%.0f s"); y += DBG_ROW_H;

    y = DebugSection(x, y + 8, w, "ACTIONS");
    if (DebugButton((Rectangle){ x, y + 4, 170, 28 }, "SEND WAVE NOW", DBG_ACCENT)) {
        // March every idle mob at once — the emergent wave, on demand.
        for (int i = 0; i < MAX_MOBS; i++) {
            if (mobs[i].active && mobs[i].state == MOB_IDLE) {
                mobs[i].state = MOB_RAID;
                mobs[i].retarget = 0;
            }
        }
    }
    if (DebugButton((Rectangle){ x + 186, y + 4, 150, 28 }, "KILL ALL MOBS", DBG_RED)) {
        for (int i = 0; i < MAX_MOBS; i++) mobs[i].active = false;
    }
}

// ─── Page: creative & cheats ──────────────────────────────
static void DebugDrawCreativePanel(Rectangle panel, Player *p) {
    float x = panel.x + 16, w = panel.width - 32, y = panel.y + 14;
    Rectangle row;

    DrawText("CREATIVE & CHEATS", (int)x, (int)y, 26, DBG_GREEN);
    DrawText("live modes — none of this is written to the save file",
             (int)x, (int)y + 30, 13, DBG_FAINT);
    y += 56;

    y = DebugSection(x, y, w, "MODES");
    row = (Rectangle){ x, y, w, DBG_ROW_H };
    DebugToggle(row, "God mode (nothing can hurt you)", &devGodMode); y += DBG_ROW_H;
    row.y = y; DebugToggle(row, "Creative (C = take stacks, far zoom)", &devCreative); y += DBG_ROW_H;

    DrawText(devCreative
             ? "Creative is ON — press C in game for the item picker, or zoom out to paint."
             : "Turn Creative ON to unlock the item picker and the zoom-out map painter.",
             (int)x, (int)y + 4, 12, devCreative ? DBG_GREEN : DBG_FAINT);
    y += 26;

    y = DebugSection(x, y + 8, w, "PLAYER");
    if (DebugButton((Rectangle){ x, y + 4, 150, 28 }, "FULL HEAL", DBG_ACCENT)) {
        p->hp = PLAYER_MAX_HP;
        p->regenDelay = 0;
    }
    if (DebugButton((Rectangle){ x + 166, y + 4, 170, 28 }, "UNLOCK ALL TECH", DBG_ACCENT)) {
        for (int t = 1; t < TECH_COUNT; t++) p->techUnlocked[t] = true;
    }
    if (DebugButton((Rectangle){ x + 352, y + 4, 170, 28 }, "CLEAR INVENTORY", DBG_RED)) {
        for (int i = 0; i < INVENTORY_SIZE; i++) {
            p->inventorySlots[i] = ITEM_NONE;
            p->inventoryAmounts[i] = 0;
        }
        PlayerRecount(p);
    }
    y += 40;

    DrawText(TextFormat("hp %.0f/%d   god %s   creative %s   painting %s",
                        p->hp, PLAYER_MAX_HP,
                        devGodMode ? "ON" : "off",
                        devCreative ? "ON" : "off",
                        devMapEdit ? "ON" : "off"),
             (int)x, (int)(panel.y + panel.height - 22), 11, DBG_FAINT);
}

// ─── Page: map painter ────────────────────────────────────
// The console half of the painter. The other half — and the one
// meant for actual use — is zooming out past DEV_MAPEDIT_ZOOM in
// creative. Both land in exactly the same state, because both do
// the same single thing: move the camera.
static void DebugDrawMapPainterPanel(Rectangle panel) {
    float x = panel.x + 16, w = panel.width - 32, y = panel.y + 14;
    Rectangle row;

    DrawText("MAP PAINTER", (int)x, (int)y, 26, DBG_GREEN);
    DrawText("paint tiles straight onto the world — no cost, no drops, no mining",
             (int)x, (int)y + 30, 13, DBG_FAINT);
    y += 56;

    y = DebugSection(x, y, w, "BRUSH");
    row = (Rectangle){ x, y, w, DBG_ROW_H };
    DebugSliderTile(row, "Tile", &devBrushTile); y += DBG_ROW_H;
    row.y = y; DebugSliderInt(row, "Radius (0 = single tile)", &devBrushRadius, 0, DEV_BRUSH_MAX); y += DBG_ROW_H;
    row.y = y; DebugSliderInt(row, "Facing (belts/arms/drills)", &devBrushDir, 0, 3); y += DBG_ROW_H;

    DrawText(TextFormat("footprint %dx%d tiles   facing %s",
                        devBrushRadius * 2 + 1, devBrushRadius * 2 + 1,
                        (devBrushDir == 0) ? "East" : (devBrushDir == 1) ? "South" :
                        (devBrushDir == 2) ? "West" : "North"),
             (int)x, (int)y + 4, 12, DBG_FAINT);
    y += 26;

    // The palette, same swatch order as the in-game bar so the two
    // read as one tool.
    y = DebugSection(x, y + 8, w, "PALETTE");
    {
        float cell = 34, gap = 5;
        int perRow = (int)((w + gap) / (cell + gap));
        if (perRow < 1) perRow = 1;
        Vector2 m = GetMousePosition();
        for (int t = 0; t < TILE_COUNT; t++) {
            Rectangle r = { x + (t % perRow) * (cell + gap),
                            y + (t / perRow) * (cell + gap), cell, cell };
            bool hovered = CheckCollisionPointRec(m, r);
            bool sel = (t == (int)devBrushTile);
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) devBrushTile = (TileType)t;
            DrawRectangleRec(r, TILES[t].color);
            DrawRectangleLinesEx(r, sel ? 3 : (hovered ? 2 : 1),
                                 sel ? DBG_ACCENT : (hovered ? RAYWHITE : DBG_LINE));
            if (hovered) {
                DrawRectangle((int)r.x, (int)r.y - 17, MeasureText(TILES[t].name, 12) + 8, 15,
                              (Color){ 8, 12, 10, 240 });
                DrawText(TILES[t].name, (int)r.x + 4, (int)r.y - 16, 12, DBG_GREEN);
            }
        }
        y += ((TILE_COUNT + perRow - 1) / perRow) * (cell + gap) + 8;
    }

    y = DebugSection(x, y + 4, w, "MODE");
    if (!devMapEdit) {
        if (DebugButton((Rectangle){ x, y + 4, 230, 30 }, "START PAINTING (zooms out)", DBG_ACCENT)) {
            devCreative = true;          // painting implies creative
            devZoomRequest = DEV_MAPEDIT_ZOOM - 0.05f;   // main.c moves the camera
            debugMenuOpen = false;       // get out of the way
        }
    } else {
        if (DebugButton((Rectangle){ x, y + 4, 230, 30 }, "STOP PAINTING (zooms in)", DBG_GREEN)) {
            devZoomRequest = 1.0f;
            debugMenuOpen = false;
        }
    }

    DrawText("Or just Ctrl+wheel out past the threshold in creative — same mode, no menu.",
             (int)x, (int)(panel.y + panel.height - 22), 11, DBG_FAINT);
}

static void DebugDrawGlobalMachinesPanel(Rectangle panel) {
    float x = panel.x + 16, w = panel.width - 32, y = panel.y + 14;
    Rectangle row;

    DrawText("MACHINES & BOTS", (int)x, (int)y, 26, DBG_GREEN);
    DrawText("the automation layer: turrets, belts, hands, drones", (int)x, (int)y + 30, 13, DBG_FAINT);
    y += 56;

    y = DebugSection(x, y, w, "TURRETS");
    row = (Rectangle){ x, y, w, DBG_ROW_H };
    DebugSliderFloat(row, "Gun range", &TUNE.turretRange, 60, 800, "%.0f px"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Gun cooldown", &TUNE.turretCooldown, 0.05f, 3, "%.2f s"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Laser range", &TUNE.laserRange, 60, 800, "%.0f px"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Laser cooldown", &TUNE.laserCooldown, 0.1f, 5, "%.2f s"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Laser damage", &TUNE.laserDamage, 1, 80, "%.0f"); y += DBG_ROW_H;

    y = DebugSection(x, y + 8, w, "LOGISTICS");
    row.y = y; DebugSliderFloat(row, "Drill interval", &TUNE.drillInterval, 0.2f, 10, "%.1f s"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Belt interval", &TUNE.conveyorInterval, 0.05f, 2, "%.2f s"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Hand interval", &TUNE.inserterInterval, 0.1f, 4, "%.2f s"); y += DBG_ROW_H;

    y = DebugSection(x, y + 8, w, "MINING BOTS");
    row.y = y; DebugSliderFloat(row, "Flight speed", &TUNE.botSpeed, 30, 500, "%.0f px/s"); y += DBG_ROW_H;
    row.y = y; DebugSliderFloat(row, "Dig speed", &TUNE.botDPS, 0.5f, 40, "%.1f dps"); y += DBG_ROW_H;
}

static void DebugDrawTilePanel(Rectangle panel, TileType t) {
    TileInfo *ti = &TILES[t];
    float x = panel.x + 16, w = panel.width - 32, y = panel.y + 14;
    Rectangle row;

    // Identity header: live color swatch + name.
    DrawRectangle((int)x, (int)y, 52, 52, ti->color);
    DrawRectangleLines((int)x, (int)y, 52, 52, DBG_LINE);
    DrawText(ti->name, (int)x + 68, (int)y + 4, 26, DBG_GREEN);
    DrawText(TextFormat("tile id %d   %s", (int)t, ti->breakable ? "breakable" : "indestructible"),
             (int)x + 68, (int)y + 34, 13, DBG_FAINT);
    y += 68;

    y = DebugSection(x, y, w, "DURABILITY");
    row = (Rectangle){ x, y, w, DBG_ROW_H };
    DebugSliderFloat(row, "Max health", &ti->maxHealth, 0.5f, 100, "%.1f hp"); y += DBG_ROW_H;
    row.y = y; DebugToggle(row, "Breakable", &ti->breakable); y += DBG_ROW_H;

    y = DebugSection(x, y + 8, w, "LOOT");
    row.y = y; DebugSliderItem(row, "Drops", &ti->drops); y += DBG_ROW_H;
    row.y = y; DebugSliderInt(row, "Drop count", &ti->dropCount, 0, 50); y += DBG_ROW_H;

    y = DebugSection(x, y + 8, w, "APPEARANCE");
    row.y = y; DebugSliderByte(row, "Color R", &ti->color.r); y += DBG_ROW_H;
    row.y = y; DebugSliderByte(row, "Color G", &ti->color.g); y += DBG_ROW_H;
    row.y = y; DebugSliderByte(row, "Color B", &ti->color.b); y += DBG_ROW_H;

    DrawText("NOTE: health applies to newly placed/generated tiles; color is instant.",
             (int)x, (int)(y + 8), 12, DBG_FAINT);
    DrawText(TextFormat("RAW: hp=%.1f drops=%d x%d brk=%d rgb=(%d,%d,%d)",
                        ti->maxHealth, (int)ti->drops, ti->dropCount, ti->breakable,
                        ti->color.r, ti->color.g, ti->color.b),
             (int)x, (int)(panel.y + panel.height - 22), 11, DBG_FAINT);
}

static void DebugDrawItemPanel(Rectangle panel, ItemID id, Player *p) {
    ItemInfo *it = &ITEMS[id];
    float x = panel.x + 16, w = panel.width - 32, y = panel.y + 14;
    Rectangle row;

    // Identity header: live sprite (redraws with edited colors) + name.
    DrawItemSprite(id, x, y, 52);
    DrawText(it->name, (int)x + 68, (int)y + 4, 26, DBG_GREEN);
    DrawText(TextFormat("item id %d   yield x%d   %s", (int)id, it->yield,
                        it->miningDPS > 0 ? "TOOL" : (it->placeable ? "PLACEABLE" : "MATERIAL")),
             (int)x + 68, (int)y + 34, 13, DBG_FAINT);
    y += 68;

    y = DebugSection(x, y, w, "TOOL / COMBAT");
    row = (Rectangle){ x, y, w, DBG_ROW_H };
    DebugSliderFloat(row, "Mining DPS", &it->miningDPS, 0, 40, "%.1f dps"); y += DBG_ROW_H;

    y = DebugSection(x, y + 8, w, "CRAFTING RECIPE");
    row.y = y; DebugSliderItem(row, "Input A", &it->inA); y += DBG_ROW_H;
    row.y = y; DebugSliderInt(row, "Amount A", &it->nA, 0, 99); y += DBG_ROW_H;
    row.y = y; DebugSliderItem(row, "Input B", &it->inB); y += DBG_ROW_H;
    row.y = y; DebugSliderInt(row, "Amount B", &it->nB, 0, 99); y += DBG_ROW_H;
    row.y = y; DebugSliderInt(row, "Yield", &it->yield, 0, 64); y += DBG_ROW_H;

    y = DebugSection(x, y + 8, w, "BUILDING");
    row.y = y; DebugToggle(row, "Placeable", &it->placeable); y += DBG_ROW_H;
    row.y = y; DebugSliderTile(row, "Places tile", &it->places); y += DBG_ROW_H;

    y = DebugSection(x, y + 8, w, "APPEARANCE (recolors the sprite live)");
    row.y = y; DebugSliderByte(row, "Color R", &it->color.r); y += DBG_ROW_H;
    row.y = y; DebugSliderByte(row, "Color G", &it->color.g); y += DBG_ROW_H;
    row.y = y; DebugSliderByte(row, "Color B", &it->color.b); y += DBG_ROW_H;

    // Tuning a recipe you can't afford to test is half a tool, so the
    // page that edits an item can also hand you some.
    y = DebugSection(x, y + 8, w, TextFormat("GIVE  (you have %d)", p->inventory[id]));
    if (DebugButton((Rectangle){ x, y + 4, 90, 28 }, "+1", DBG_ACCENT)) {
        PlayerGiveItem(p, id, 1);
    }
    if (DebugButton((Rectangle){ x + 100, y + 4, 90, 28 }, "+10", DBG_ACCENT)) {
        PlayerGiveItem(p, id, 10);
    }
    if (DebugButton((Rectangle){ x + 200, y + 4, 120, 28 },
                    TextFormat("+%d STACK", STACK_MAX), DBG_ACCENT)) {
        PlayerGiveItem(p, id, STACK_MAX);
    }
    if (DebugButton((Rectangle){ x + 330, y + 4, 110, 28 }, "TAKE ALL", DBG_RED)) {
        PlayerRemoveItem(p, id, p->inventory[id]);
    }

    DrawText(TextFormat("RAW: dps=%.2f inA=%d nA=%d inB=%d nB=%d yield=%d place=%d->%d rgb=(%d,%d,%d)",
                        it->miningDPS, (int)it->inA, it->nA, (int)it->inB, it->nB,
                        it->yield, it->placeable, (int)it->places,
                        it->color.r, it->color.g, it->color.b),
             (int)x, (int)(panel.y + panel.height - 22), 11, DBG_FAINT);
}

// ─── The console itself ───────────────────────────────────
// Called from DrawGame every frame; does nothing while closed.
// Immediate mode: this ONE call is both the input handling and the
// rendering of the entire console.
static void DebugMenuDraw(Player *p) {
    if (!debugMenuOpen) return;
    DebugSaveDefaultsOnce();   // snapshot factory values on first open

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int px = 36, py = 30;
    int pw = sw - px * 2, ph = sh - py * 2;

    // Backdrop + double border for the "serious tool" look.
    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 170 });
    DrawRectangle(px, py, pw, ph, DBG_BG);
    DrawRectangleLines(px, py, pw, ph, DBG_GREEN);
    DrawRectangleLines(px + 3, py + 3, pw - 6, ph - 6, DBG_LINE);

    // Faint scanlines across the whole panel (pure decoration —
    // remove this loop if you ever profile it as slow; it won't be).
    for (int yy = py + 6; yy < py + ph - 6; yy += 4) {
        DrawLine(px + 4, yy, px + pw - 4, yy, (Color){ 120, 255, 160, 7 });
    }

    // Header: title + blinking cursor + live engine readouts.
    bool blink = ((int)(GetTime() * 2) % 2) == 0;
    DrawText(TextFormat("// HOME PLANET DEBUG CONSOLE v1.0 — LIVE TUNING INTERFACE%s", blink ? " _" : ""),
             px + 14, py + 10, 20, DBG_GREEN);
    DrawText(TextFormat("FPS %d | FRAME %.2fms | RES %dx%d | POS %.0f,%.0f | HOLDING %s | REGISTRY %d ENTRIES",
                        GetFPS(), GetFrameTime() * 1000.0f, sw, sh,
                        p->pos.x, p->pos.y, ITEMS[p->selected].name, DEBUG_ENTRY_COUNT),
             px + 14, py + 34, 13, DBG_DIM);
    DrawLine(px, py + 54, px + pw, py + 54, DBG_LINE);

    // RESET button lives in the header, right-aligned.
    if (DebugButton((Rectangle){ (float)(px + pw - 190), (float)(py + 12), 176, 32 },
                    "RESET ALL DEFAULTS", DBG_RED)) {
        DebugResetAll();
    }

    // ── Left: the registry list ──────────────────────────
    int listX = px + 12, listY = py + 64;
    int listW = 252,     listH = ph - 64 - 44;
    DrawRectangle(listX, listY, listW, listH, DBG_PANEL);
    DrawRectangleLines(listX, listY, listW, listH, DBG_LINE);

    const int rowH = 26;
    int visible = (listH - 8) / rowH;
    if (visible < 1) visible = 1;

    // Wheel over the list scrolls it.
    Vector2 mouse = GetMousePosition();
    Rectangle listRect = { (float)listX, (float)listY, (float)listW, (float)listH };
    if (CheckCollisionPointRec(mouse, listRect)) {
        float wheel = GetMouseWheelMove();
        if (wheel > 0) debugListScroll--;
        if (wheel < 0) debugListScroll++;
    }
    if (debugListScroll > DEBUG_ENTRY_COUNT - visible) debugListScroll = DEBUG_ENTRY_COUNT - visible;
    if (debugListScroll < 0) debugListScroll = 0;

    for (int i = debugListScroll; i < DEBUG_ENTRY_COUNT && i < debugListScroll + visible; i++) {
        Rectangle r = { (float)(listX + 4), (float)(listY + 4 + (i - debugListScroll) * rowH),
                        (float)(listW - 8), (float)(rowH - 2) };
        bool hovered = CheckCollisionPointRec(mouse, r);
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) debugListSel = i;
        bool sel = (i == debugListSel);
        if (sel || hovered) DrawRectangleRec(r, sel ? (Color){ 26, 60, 40, 255 } : (Color){ 18, 34, 26, 255 });

        // Row label + icon + a '*' marker when the entry has been
        // edited away from its factory defaults.
        const char *label;
        bool modified = false;
        if (i < DEBUG_GLOBAL_PAGES) {
            label = (i == 0) ? "PLAYER & WEAPONS" :
                    (i == 1) ? "MOBS & RAIDS"     :
                    (i == 2) ? "MACHINES & BOTS"  :
                    (i == 3) ? "CREATIVE & CHEATS" : "MAP PAINTER";
            // The two dev pages aren't tuning tables — they mark
            // themselves modified when their mode is actually live.
            if (i == 3)      modified = devGodMode || devCreative;
            else if (i == 4) modified = devMapEdit;
            else             modified = memcmp(&TUNE, &debugDefaultTune, sizeof(TUNE)) != 0;
            DrawText(">", (int)r.x + 6, (int)r.y + 5, 14, DBG_ACCENT);
        } else if (i < DEBUG_ENTRY_ITEMS_START) {
            TileType t = (TileType)(i - DEBUG_ENTRY_TILES_START);
            label = TextFormat("TILE  %s", TILES[t].name);
            modified = memcmp(&TILES[t], &debugDefaultTiles[t], sizeof(TileInfo)) != 0;
            DrawRectangle((int)r.x + 5, (int)r.y + 5, 14, 14, TILES[t].color);
        } else {
            ItemID id = (ItemID)(i - DEBUG_ENTRY_ITEMS_START + 1);
            label = TextFormat("ITEM  %s", ITEMS[id].name);
            modified = memcmp(&ITEMS[id], &debugDefaultItems[id], sizeof(ItemInfo)) != 0;
            DrawItemSprite(id, r.x + 4, r.y + 4, 16);
        }
        DrawText(label, (int)r.x + 26, (int)r.y + 5, 14, sel ? DBG_GREEN : DBG_DIM);
        if (modified) DrawText("*", (int)(r.x + r.width - 12), (int)r.y + 5, 14, DBG_ACCENT);
    }

    // ── Right: the attribute panel for the selection ─────
    Rectangle panel = { (float)(listX + listW + 12), (float)listY,
                        (float)(px + pw - (listX + listW + 12) - 12), (float)listH };
    DrawRectangleRec(panel, DBG_PANEL);
    DrawRectangleLinesEx(panel, 1, DBG_LINE);

    if (debugListSel == 0)      DebugDrawGlobalWeaponsPanel(panel);
    else if (debugListSel == 1) DebugDrawGlobalMobsPanel(panel);
    else if (debugListSel == 2) DebugDrawGlobalMachinesPanel(panel);
    else if (debugListSel == 3) DebugDrawCreativePanel(panel, p);
    else if (debugListSel == 4) DebugDrawMapPainterPanel(panel);
    else if (debugListSel < DEBUG_ENTRY_ITEMS_START) {
        DebugDrawTilePanel(panel, (TileType)(debugListSel - DEBUG_ENTRY_TILES_START));
    } else {
        DebugDrawItemPanel(panel, (ItemID)(debugListSel - DEBUG_ENTRY_ITEMS_START + 1), p);
    }

    // Footer: controls + the "this is live memory" warning.
    DrawLine(px, py + ph - 34, px + pw, py + ph - 34, DBG_LINE);
    DrawText("[F3/ESC] close   [WHEEL] scroll list   [LMB drag] adjust   [CLICK] toggle/select",
             px + 14, py + ph - 26, 13, DBG_DIM);
    const char *warn = "! LIVE MEMORY EDIT — NOT SAVED TO DISK";
    DrawText(warn, px + pw - MeasureText(warn, 13) - 14, py + ph - 26, 13, DBG_ACCENT);
}

#endif // DEBUG_H
