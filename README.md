# HOME PLANET: VOID RUNNER
### A 2D mining / crafting / building sandbox — and a C learning project

Built with [raylib](https://www.raylib.com/) in plain C99. The codebase is
deliberately small, heavily commented, and **data-driven**: the goal is to
learn C by being able to change one thing and see one thing change on screen.

---

## PROJECT STRUCTURE

```
HomePlanet/
├── README.md
├── Makefile             # cross-platform build (Mac + Windows)
├── .vscode/             # tasks.json (build) + launch.json (F5 debug)
└── src/
    ├── config.h         # every tweakable number: colors, speeds, sizes
    ├── gamedata.h       # WHAT exists: the item table + tile table
    ├── world.h          # the tile grid: generate, draw, damage, save/load
    ├── player.h         # movement, inventory, crafting, placing
    ├── ui.h             # hotbar, craft menu, help text (screen space)
    └── main.c           # game loop + input; the only file where they meet
```

**The dependency rule:** files may only include files *above* them in this
list. `world.h` doesn't know players exist; `ui.h` reads state but never
changes it. That one-way flow is why edits stay isolated.

**The data rule:** every item and tile is ONE ROW in a table in
`gamedata.h`. Adding or removing an item touches only that file — the
hotbar, craft menu, mining, and placing all adapt automatically.

---

## BUILD & RUN

The same commands work on both machines. Clone, build, play:

```bash
git clone https://github.com/avidX027/HomePlanet.git
make          # Windows: mingw32-make  (VS Code: Ctrl/Cmd+Shift+B)
make run      # or ./bin/HomePlanet  /  bin\HomePlanet.exe
```

In VS Code: **Ctrl/Cmd+Shift+B** builds, **F5** builds and debugs.
(Never use the ▶ "Run C/C++ File" button — it bypasses the Makefile
and won't link raylib.)

### One-time setup per machine

**macOS** — install raylib to `/usr/local` (e.g. build from source or
`brew install raylib`), plus Xcode command-line tools.

**Windows (MSYS2 UCRT64)** —
1. Install [MSYS2](https://www.msys2.org/), then in the MSYS2 shell:
   `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-gdb`
2. Download the raylib MinGW release from
   [raylib releases](https://github.com/raysan5/raylib/releases) and place
   headers/libs at `C:/raylib/include` and `C:/raylib/lib`.

---

## CONTROLS

| Key | Action |
|-----|--------|
| WASD | Move |
| Left Click (hold) | Mine tile (pickaxe mines faster than hands) |
| Right Click | Place selected item (walls, drills, ...) |
| 1–9 / click hotbar | Select item |
| TAB | Crafting menu (↑↓ navigate, ENTER craft) |
| F5 / F9 | Save / load world |
| ESC | Back to title screen |
| ENTER | Start (title screen) |

---

## CURRENT GAME CONTENT

**Tiles:** grass, tree, rock, wall, mushroom, mining drill
**Items:** wood, stone, pickaxe, wall, mushroom, mining drill

Loop: mine trees/rocks → craft a pickaxe (mine faster) → craft walls and
drills → build. Everything above is defined in the two tables in
`gamedata.h` — open it and read the rows.

### How to add an item (the whole process)

1. Add `ITEM_MYTHING,` to the `ItemID` enum in `gamedata.h`
2. Add one row to the `ITEMS[]` table (name, color, recipe, ...)
3. (If it places a tile) add a `TILE_` enum entry + `TILES[]` row
4. Rebuild. It's now in the hotbar and craft menu.

---

## C CONCEPTS THIS CODEBASE TEACHES

| Where | Concept |
|-------|---------|
| config.h | `#define` macros vs variables |
| gamedata.h | enums, structs, designated initializers, sentinel values |
| player.h | pointers, `->`, pass-by-address |
| world.h | 2D arrays, binary file I/O (`fopen`/`fwrite`/`fread`) |
| ui.h | immediate-mode GUI, single-source-of-truth geometry |
| main.c | the frame loop, delta time, short-circuit `&&`, scope |
| Makefile | how C actually builds: compile → link, per-OS libraries |

---

## VISION / IDEA BACKLOG

*Brainstorm territory — none of this exists yet. Kept here so it isn't
lost; promoted to the sections above only when implemented.*

**Industry chain:** ores (iron, copper, gold, aluminum) → smelting →
electronics/circuits/cables → engines, motors, compressors, pneumatics,
plumbing, plastics. Scaled crafting: bigger recipes yield more powerful
versions (laser modules S/M/L). Components (gears, bearings) as
sub-parts of placeables rather than standalone items.

**Logistics:** conveyors, pipes, auto-miner systems, chests/shelves/bins,
3D printers, energy storage.

**Base building:** computers & servers unlock research; heat management
(servers add heat, cooling removes it); dust and wind outside — filters,
turrets, cameras as anomaly observers; antenna dishes, satellites,
rockets S/M/L.

**Tech concepts taught in-game:** bandwidth/throughput, signal strength
(dBm), mesh radio (LoRa/Meshtastic-style relays), MHz vs GHz,
persistence (already in: F5/F9), lookup tables (already in: recipes),
data-center cooling, system logs. Blue-collar trades as a theme.

**Further out:** simulated multiplayer packet trading, power grid
(solar → capacitors → machines), signal encryption, research tree,
procedural crater dungeons.

