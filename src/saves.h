#ifndef SAVES_H
#define SAVES_H
// ============================================================
//  SAVES.H — many worlds instead of one file.
//
//  The game used to have exactly ONE save: homeplanet.sav, no name,
//  no date, no way to tell what was in it. This file turns that into
//  SLOTS — eight of them, each carrying enough metadata to be
//  recognizable from a menu:
//
//    name      you type it; it's the point of the whole screen
//    stamp     when it was last written
//    playSec   how long that world has been played
//    preview   a 96x96 thumbnail of the map around the player
//
//  All four live in the save file's HEADER, at the front, so the
//  saves screen can describe eight worlds by reading ~28KB each
//  instead of loading eight 10MB worlds.
//
//  Owns: the slot table and its thumbnails. Knows how to read a
//  header, write a name back, and paint a preview. Knows NOTHING
//  about the body of a save file — main.c still owns that, because
//  the body is player + world + entities and main.c is the only
//  place those three meet.
// ============================================================

#include <stdio.h>
#include <string.h>
#include <time.h>      // time, localtime, strftime — the "when"
#include "raylib.h"
#include "config.h"
#include "world.h"
#include "player.h"
#include "entities.h"  // entGameTime — the "how long"

#define SAVE_SLOTS          8      // cards on the saves screen: 4 x 2
#define SAVE_NAME_MAX       24     // characters in a world name, incl. NUL
#define SAVE_PREVIEW_DIM    96     // thumbnail is DIM x DIM pixels
#define SAVE_PREVIEW_TILES  192    // ...covering this many tiles of map
#define SAVE_DIR            "saves"

// The file header. `magic` and `version` stay FIRST and keep their
// old types on purpose: whatever else changes, any build can open
// any file, read eight bytes, and know whether to go further.
typedef struct {
    char      magic[4];
    int       version;
    char      name[SAVE_NAME_MAX];
    long long stamp;        // unix seconds at the moment of writing
    int       playSeconds;  // entGameTime, rounded down
    unsigned char preview[SAVE_PREVIEW_DIM * SAVE_PREVIEW_DIM * 3];   // RGB
} SaveHeader;

// One row of the saves screen's model.
//
// `blocked` is the case worth being careful about: a file IS sitting
// in this slot, but this build can't read its header — a save from an
// older format, or a damaged one. It is emphatically NOT an empty
// slot, because an empty slot is something you click to write into,
// and doing that here would destroy a world we merely failed to
// understand. So it gets its own state and refuses the click.
typedef struct {
    bool       used;      // is there a readable save in this slot?
    bool       blocked;   // ...or an unreadable file we must not touch?
    SaveHeader head;      // the metadata, when it IS readable
    Texture2D  thumb;     // the preview, uploaded for drawing
    bool       hasThumb;
} SaveSlot;

static SaveSlot saveSlots[SAVE_SLOTS];

// ─── Paths ────────────────────────────────────────────────
// TextFormat's buffer is recycled by raylib, so the returned string
// is only good until the next few TextFormat calls — fine for
// "open this file right now", which is all anyone does with it.
static const char *SaveSlotPath(int slot) {
    return TextFormat("%s/slot%d.sav", SAVE_DIR, slot);
}

static void SavesEnsureDir(void) {
    if (!DirectoryExists(SAVE_DIR)) MakeDirectory(SAVE_DIR);
}

// Is there a file in this slot at all? (Whether it's a save we can
// READ is a stronger question — see SaveReadHeader.)
static bool SaveSlotFileExists(int slot) {
    if (slot < 0 || slot >= SAVE_SLOTS) return false;
    return WorldHasSaveFile(SaveSlotPath(slot));
}

// Copy a name in, truncated to fit and always terminated. Save names
// come from a text field the player types into, so this is the one
// place that has to be careful about length.
static void SaveSetName(char *dst, const char *src) {
    int i = 0;
    if (src != NULL) {
        for (; src[i] != '\0' && i < SAVE_NAME_MAX - 1; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
}

// A name for a slot that has never been named.
static const char *SaveDefaultName(int slot) {
    return TextFormat("World %d", slot + 1);
}

// ─── The preview ──────────────────────────────────────────
// A thumbnail of the map AROUND THE PLAYER, not of the whole world:
// at 750 tiles across, a whole-world thumbnail is a smear of biome
// colors that looks the same in every save. The 192-tile window is
// small enough that your base fills it, which is what makes one save
// tell itself apart from another at a glance.
//
// Colors follow exactly the minimap's rules — unexplored is dark,
// grass takes its ground color, everything else takes its tile color
// — so the preview and the map you played on agree.
static void SaveBuildPreview(Vector2 focus, unsigned char *rgb) {
    const int step = SAVE_PREVIEW_TILES / SAVE_PREVIEW_DIM;   // tiles per pixel
    int cx = (int)(focus.x / TILE_SIZE) - SAVE_PREVIEW_TILES / 2;
    int cy = (int)(focus.y / TILE_SIZE) - SAVE_PREVIEW_TILES / 2;

    for (int py = 0; py < SAVE_PREVIEW_DIM; py++) {
        for (int px = 0; px < SAVE_PREVIEW_DIM; px++) {
            int tx = cx + px * step, ty = cy + py * step;
            Color c = { 6, 6, 12, 255 };                 // off the map
            if (WorldInBounds(tx, ty)) {
                if (!worldExplored[tx][ty]) {
                    c = (Color){ 11, 11, 18, 255 };      // never scouted
                } else {
                    TileType t = world[tx][ty].type;
                    c = (t == TILE_GRASS) ? WorldGroundColor(tx, ty) : TILES[t].color;
                    if (t == TILE_SPAWNER) c = (Color){ 220, 60, 220, 255 };
                }
            }
            int i = (py * SAVE_PREVIEW_DIM + px) * 3;
            rgb[i] = c.r; rgb[i + 1] = c.g; rgb[i + 2] = c.b;
        }
    }

    // "You were here" — a small bright cross dead center, so the eye
    // has something to land on in a field of terrain.
    int mid = SAVE_PREVIEW_DIM / 2;
    for (int d = -2; d <= 2; d++) {
        int arms[2][2] = { { mid + d, mid }, { mid, mid + d } };
        for (int a = 0; a < 2; a++) {
            int x = arms[a][0], y = arms[a][1];
            if (x < 0 || x >= SAVE_PREVIEW_DIM || y < 0 || y >= SAVE_PREVIEW_DIM) continue;
            int i = (y * SAVE_PREVIEW_DIM + x) * 3;
            rgb[i] = 255; rgb[i + 1] = 255; rgb[i + 2] = 255;
        }
    }
}

// ─── Reading a slot's metadata ────────────────────────────
// Only the header, never the body: that's what makes the saves
// screen instant even with eight big worlds on disk. `wantVersion`
// is passed in rather than known here, because the format number
// belongs to whoever writes the body — main.c.
static bool SaveReadHeader(int slot, int wantVersion, SaveHeader *out) {
    if (slot < 0 || slot >= SAVE_SLOTS) return false;
    FILE *f = fopen(SaveSlotPath(slot), "rb");
    if (f == NULL) return false;
    SaveHeader h;
    bool ok = (fread(&h, sizeof(h), 1, f) == 1);
    fclose(f);
    if (!ok) return false;
    if (memcmp(h.magic, "HPSV", 4) != 0 || h.version != wantVersion) return false;
    // Untrusted file: the name must be a terminated string before
    // anything draws it, and the clock must not run backwards.
    h.name[SAVE_NAME_MAX - 1] = '\0';
    if (h.playSeconds < 0) h.playSeconds = 0;
    if (h.name[0] == '\0') SaveSetName(h.name, SaveDefaultName(slot));
    *out = h;
    return true;
}

// Rewrite JUST the name, in place. A rename shouldn't cost a 10MB
// re-write of a world that hasn't changed — "r+b" opens for update,
// and we put back exactly the bytes we read.
static bool SaveRenameSlot(int slot, int wantVersion, const char *newName) {
    SaveHeader h;
    if (!SaveReadHeader(slot, wantVersion, &h)) return false;
    SaveSetName(h.name, newName);
    if (h.name[0] == '\0') SaveSetName(h.name, SaveDefaultName(slot));
    FILE *f = fopen(SaveSlotPath(slot), "r+b");
    if (f == NULL) return false;
    bool ok = (fwrite(&h, sizeof(h), 1, f) == 1);
    fclose(f);
    return ok;
}

// ─── The slot table ───────────────────────────────────────
static void SavesUnloadThumbs(void) {
    for (int i = 0; i < SAVE_SLOTS; i++) {
        if (saveSlots[i].hasThumb) {
            UnloadTexture(saveSlots[i].thumb);
            saveSlots[i].hasThumb = false;
        }
    }
}

// Re-scan every slot and rebuild its thumbnail. Called when the
// saves screen opens and after anything that changes a file, so the
// screen is never showing a world that no longer exists.
static void SavesRefresh(int wantVersion) {
    SavesUnloadThumbs();
    for (int i = 0; i < SAVE_SLOTS; i++) {
        saveSlots[i].used = SaveReadHeader(i, wantVersion, &saveSlots[i].head);
        // Couldn't read it, but something is there → hands off.
        saveSlots[i].blocked = !saveSlots[i].used && SaveSlotFileExists(i);
        if (!saveSlots[i].used) continue;
        // Build the texture straight from the header's bytes. The
        // Image is a VIEW of them, not a copy, so it must never be
        // UnloadImage'd — LoadTextureFromImage only reads it.
        Image img = { saveSlots[i].head.preview, SAVE_PREVIEW_DIM, SAVE_PREVIEW_DIM,
                      1, PIXELFORMAT_UNCOMPRESSED_R8G8B8 };
        saveSlots[i].thumb = LoadTextureFromImage(img);
        saveSlots[i].hasThumb = (saveSlots[i].thumb.id != 0);
    }
}

// The first slot with nothing in it AT ALL, or -1 when every slot
// holds something. NEW GAME uses this, so starting a world never
// lands on top of one you already have — and "empty" here means the
// file doesn't exist, not merely that we couldn't read it.
static int SaveFirstEmptySlot(int wantVersion) {
    SaveHeader h;
    for (int i = 0; i < SAVE_SLOTS; i++) {
        if (SaveSlotFileExists(i)) continue;                  // occupied, readable or not
        if (!SaveReadHeader(i, wantVersion, &h)) return i;
    }
    return -1;
}

static bool SaveDeleteSlot(int slot) {
    if (slot < 0 || slot >= SAVE_SLOTS) return false;
    return remove(SaveSlotPath(slot)) == 0;
}

// ─── Formatting for the card ──────────────────────────────
// "2026-08-05  14:32". A stamp of 0 means the file predates stamps
// (or the clock was unreadable); say so rather than printing 1970.
static const char *SaveStampText(long long stamp) {
    if (stamp <= 0) return "unknown date";
    time_t t = (time_t)stamp;
    struct tm *lt = localtime(&t);
    if (lt == NULL) return "unknown date";
    static char buf[32];
    if (strftime(buf, sizeof(buf), "%Y-%m-%d  %H:%M", lt) == 0) return "unknown date";
    return buf;
}

// "3h 07m" / "12m" — playtime at the resolution anyone cares about.
static const char *SavePlayTimeText(int seconds) {
    if (seconds < 60) return "just started";
    int h = seconds / 3600, m = (seconds % 3600) / 60;
    return h > 0 ? TextFormat("%dh %02dm played", h, m) : TextFormat("%dm played", m);
}

#endif // SAVES_H
