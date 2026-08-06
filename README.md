# HOME PLANET: VOID RUNNER

### A 2D mining / crafting / factory-building survival game — and a C learning project

Built with [raylib](https://www.raylib.com/) in plain C99. Roughly 6,500 lines
across one `.c` file and seven headers. The codebase is deliberately small,
heavily commented, and **data-driven**: the goal is to learn C by being able to
change one thing and see one thing change on screen.

You wake up on a hostile planet. Punch trees, craft a pickaxe, smelt ore, and
automate the whole chain with drills and belts — before the natives decide your
base looks edible.

---

## PROJECT STRUCTURE

```
HomePlanet/
├── README.md
├── Makefile             # cross-platform build (macOS + Windows)
├── saves/               # your worlds — eight slots (git-ignored)
│   └── slot0.sav …
├── .vscode/             # tasks.json (build) + launch.json (F5 debug)
└── src/
    ├── config.h         # every tweakable number: speeds, sizes, colors
    ├── gamedata.h       # WHAT exists: item table, tile table, tech tree, sprites
    ├── world.h          # the tile grid: biomes, generation, drawing, damage, fog
    ├── player.h         # movement, inventory, craft queue, research state
    ├── entities.h       # everything that MOVES: machines, mobs, bots, bullets
    ├── saves.h          # the save slots: names, dates, previews
    ├── ui.h             # hotbar, backpack, craft grid, block panels (screen space)
    ├── debug.h          # the F3 live-tuning console
    └── main.c           # game loop + input; the only file where they all meet
```

### The three rules that keep this codebase editable

**The dependency rule.** Files may only include files *above* them in that
list. `world.h` doesn't know players exist. `entities.h` moves mobs through
the world and hurts the player, so it sits above both. `ui.h` reads state but
never changes it. That one-way flow is why edits stay isolated.

**The data rule.** Every item, tile, and tech is ONE ROW in a table in
`gamedata.h`. Adding or removing an item touches only that file — the hotbar,
craft menu, mining, placing, and research tree all adapt automatically.

**The single-translation-unit rule.** There is exactly one `.c` file. Every
header holds real code marked `static`, and `main.c` includes them in
dependency order. This is not how large C projects are organized, and it is on
purpose: no header/source split to keep in sync, no forward declarations, no
link errors — the whole program compiles as one unit in about a second.

---

## BUILD & RUN

The same commands work on both machines. Clone, build, play:

```bash
git clone https://github.com/avidX027/HomePlanet.git
```

```bash
make && make run
```

On Windows use `mingw32-make`. In VS Code: **Ctrl/Cmd+Shift+B** builds,
**F5** builds and debugs. (Never use the ▶ "Run C/C++ File" button — it
bypasses the Makefile and won't link raylib.)

### One-time setup per machine

**macOS** — install raylib to `/usr/local` (build from source or
`brew install raylib`), plus the Xcode command-line tools. Builds with `clang`.

**Windows (MSYS2 UCRT64)** —
1. Install [MSYS2](https://www.msys2.org/), then in the MSYS2 shell:
   `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-gdb`
2. Download the raylib MinGW release from
   [raylib releases](https://github.com/raysan5/raylib/releases) and place the
   headers and libs at `C:/raylib/include` and `C:/raylib/lib`.

---

## CONTROLS

The authoritative list lives in the `CONTROLS[]` table in `main.c` and is shown
in-game under **Settings → Controls**.

| Key | Action |
|-----|--------|
| W A S D | Move (works with menus open) |
| Left click | Build (holding a block) / mine / shoot |
| Right click | Open block panels; in crafting, craft 5 |
| Drag (belt) | Underground belt: drag out the run, release to lay |
| Filter slot | Inserter / splitter: click to pick an item, RMB clears |
| Q | Close menus, draw / cycle weapons |
| Z (drag) | Feed coal to drills / inserters |
| F (hold) | Pull items off nearby belts |
| Drag / Ctrl+click | Move stacks between any open panels |
| Mouse wheel | Cycle hotbar |
| Ctrl + wheel | Zoom camera |
| 1 – 7 | Select hotbar slot |
| E | Backpack |
| TAB | Crafting |
| Arrow keys | Navigate menus |
| R | Reload gun; else rotate belt/arm or ghost |
| G (hold) | World map |
| F3 | Debug console |
| F5 / F9 | Quick save / quick load (the slot you're playing) |
| ESC | Close menu / pause |

---

## THE GAME

### The world

640×640 tiles of procedurally generated planet, split into four biomes —
meadow, forest, rocklands, and the corrupted **waste** at the edges where the
mobs live. Sulfur, metal, and coal generate as Factorio-style **ore fields**:
colored ground patches with solid ore nodes clustered on them. Mine the node,
then place a drill on the field underneath and it pumps ore forever.

Fog of war hides what you haven't walked past. A 480-second day/night cycle
swings the shadows around.

### The loop

Mine trees and rocks by hand → craft a pickaxe → build a furnace and smelt
metal → **research** at a placed Research Computer → unlock drills, belts,
inserters, turrets → automate mining so you can afford to fight.

Nothing crafts instantly: every recipe takes real seconds and goes through a
**craft queue** you can keep stacking while you walk away.

### Automation

| Machine | What it does |
|---------|--------------|
| Mining Drill | Auto-mines the tile in front of it; burns coal |
| Conveyor / Belt Corner | Moves items — and drags *you* if you stand on one |
| Splitter | One lane in, two out: alternates left and right |
| Underground Belt | A buried run of any length; costs one belt per tile |
| Inserter | Robotic hand: moves items between neighbors; burns coal |
| Chest | 49 slots of storage |
| Mining Bot | Autonomous drone that flies out and digs on its own |
| Gun Turret | Shoots mobs; needs bullets |
| Laser Turret | Zaps mobs; no ammo, slower |
| Research Computer | Opens the tech tree |

Belts, corners, splitters, inserters, and drills are **directional** — `R`
rotates them in place. Drills spit output onto whatever sits in front of them,
so a drill can feed a belt with no inserter at all. Coal is the fuel economy:
rocks drop it generously because every drill and inserter eats it.

#### Filters

Inserters and splitters both have a **filter**: open the machine's panel and
click the filter slot to get a grid of every item in the game (which, since
anything can ride a belt, is exactly the list worth sorting for). Right-click
the slot to clear it.

* A **filtered inserter** only ever picks up that one item — so an arm can hunt
  coal out of a chest full of ore instead of jamming with cargo the destination
  won't take.
* A **filtered splitter** stops alternating and starts sorting: matches go out
  the left mouth, everything else out the right. A filtered item waits for its
  own lane rather than escaping down the wrong one.

The chosen item is drawn small in the machine's top-left corner, so a filtered
line is readable from the factory floor without opening anything.

#### Underground belts

The underground belt has **no length limit**. It isn't placed a tile at a time
— press the mouse on the tile the run should start at, drag out to where it
should surface, and release. While you drag, a neon line shows the exact run:
its two mouths, a pip crawling the length to show which way cargo travels, and
the price. It costs **one belt per tile it spans**, so a twenty-tile tunnel
costs twenty, the same as laying twenty on the surface would.

Only the two mouths need open ground; everything between them is what the belt
is going under. Mining either mouth pulls the whole run up and refunds every
tile of it — half a tunnel is not a thing you can own. A faint dotted line
between linked mouths keeps a buried base readable.

### Saved worlds

The title screen's **SAVES** button opens a grid of eight slots. Each occupied
slot is a card showing a **preview** — a thumbnail of the map around where you
were standing, so worlds tell themselves apart at a glance — plus the date it
was last written and how long it's been played.

* **Click a card** to play that world.
* **Click its name** to rename it; Enter keeps the change, Escape throws it
  away. A rename rewrites only the file's header, not the whole 10MB world.
* **Click the x** to delete — twice. The first click arms it and says so; the
  second does it. There is no undo for a deleted world, so it costs two.
* **Click an empty slot** to start a fresh world there.

**NEW GAME** starts in the first free slot. If all eight are taken the world
still starts, it just isn't attached to a slot yet, and the first save sends
you to the grid to pick one — nothing is ever written over without a click.

Save metadata lives in the file's header, so the screen describes eight worlds
by reading a few KB each instead of loading eight full maps.

### The natives

Mobs spawn from nests scattered in ten big infestation patches far from your
spawn. They are **not** on a timer — they bunch up at their nest and, once a
brood reaches critical mass, set off toward you as an emergent wave. An
**evolution** stat climbs over 25 minutes of play, making them tougher and
their nests faster. They chew through walls and machines to get to you.

You have 100 HP and you can die. Build the base.

### Weapons

Slingshot → pistol → shotgun → SMG, in ascending order of rank (`Q`
quick-draws your best one). Real guns feed from a **magazine** and reload from
your bullet stack, which takes time — that reload window is what the mobs are
counting on. Everything auto-fires while held, but slightly slower than
clicking, so click-spam stays the skill ceiling. Bombs crack open nests and
walls.

### Current content

**30 items · 18 tile types · 8 technologies** — all defined in three tables in
`gamedata.h`. Open it and read the rows.

### How to add an item (the whole process)

1. Add `ITEM_MYTHING,` to the `ItemID` enum in `gamedata.h`
2. Add one row to the `ITEMS[]` table (name, color, recipe, tech, ...)
3. (If it places a tile) add a `TILE_` enum entry + a `TILES[]` row
4. (Optional) draw an 8×8 sprite for it in `ITEM_ART[]`
5. Rebuild. It's now in the hotbar, craft menu, and research gating.

---

## TWO THINGS WORTH READING THE CODE FOR

**Sprites are text.** There are no image files and no artist. Every item icon
is an 8×8 pixel-art sprite written as eight strings in `ITEM_ART[]`, one
character per pixel: `b` is the item's base color, `d` dark, `l` light, `s` a
secondary hue, `o` outline, `.` transparent. Because the colors are *derived*
from the item's table color at draw time, recoloring an item automatically
recolors its sprite. The art is data, like everything else.

**F3 opens the whole game's dashboard.** The debug console exposes every item,
every tile, and every global tunable as a live slider — crank the pickaxe to 40
DPS, walk over to a rock, feel the difference, dial it back, no restart. This
is why the `ITEMS[]` and `TILES[]` tables aren't `const` anymore, and why the
game reads its numbers from the `TUNE` struct in `config.h` instead of the
`#define`s directly (a `#define` is baked into the machine code at compile time
and can never change; a struct field is ordinary memory). Edits are live only —
they vanish on restart, which is the point.

---

## C CONCEPTS THIS CODEBASE TEACHES

| Where | Concept |
|-------|---------|
| config.h | `#define` macros vs. runtime variables, compound literals |
| gamedata.h | enums, structs, designated initializers, table-driven design |
| world.h | 2D arrays, bit-packing, binary file I/O (`fopen`/`fwrite`/`fread`) |
| player.h | pointers, `->`, pass-by-address, fixed-size queues |
| entities.h | object pools (fixed array + `active` flag) — no `malloc`, no leaks |
| ui.h | screen space vs. world space, single-source-of-truth geometry |
| debug.h | immediate-mode GUI; pointer identity as widget identity |
| main.c | the frame loop, delta time, state machines, scope |
| Makefile | how C actually builds: compile → link, per-OS libraries |

**On saving:** the save file is a magic number + version, then raw `fwrite` of
the structs and pools. It's fast and it's twelve lines of code — and it means
the format is only valid for one exact build. Change a struct and old saves
stop loading, which is exactly why `SAVE_VERSION` exists and gets bumped.

---

## VISION / IDEA BACKLOG

*Brainstorm territory — none of this exists yet. Kept here so it isn't lost;
promoted to the sections above only when implemented.*

**Industry chain:** more ores (copper, gold, aluminum) → alloys → electronics,
circuits, cables → engines, motors, compressors, pneumatics, plumbing,
plastics. Scaled crafting: bigger recipes yield more powerful versions (laser
modules S/M/L). Components (gears, bearings) as sub-parts of placeables rather
than standalone items.

**Logistics:** pipes and fluids, mergers, 3D printers, logistics bots with a
request network. *(Underground belts and splitters have landed — see
Automation.)*

**Power:** solar → capacitors → machines, a real grid instead of per-machine
coal. Energy storage. Brownouts when you overdraw.

**Base building:** computers and servers as a second research tier; heat
management (servers add heat, cooling removes it); dust and wind outside —
filters, cameras as anomaly observers; antenna dishes, satellites, rockets
S/M/L.

**Tech concepts taught in-game:** bandwidth/throughput, signal strength (dBm),
mesh radio (LoRa/Meshtastic-style relays), MHz vs. GHz, data-center cooling,
system logs. Blue-collar trades as a theme. *(Already in: persistence via
F5/F9, lookup tables via the recipe and tech tables, object pools.)*

**Further out:** simulated multiplayer packet trading, signal encryption,
procedural crater dungeons, a proper boss at the heart of an infestation.
