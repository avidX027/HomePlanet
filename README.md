# HOME PLANET: VOID RUNNER v2.0
## A 2D Online Strategic Factory Survival Game

---

## FILE STRUCTURE
```
homeplanet/
├── README.md            # Project documentation (seen in your screenshot)
├── .vscode/             # Editor settings (tasks.json, launch.json)
└── src/                 # All source code lives here
    ├── main.c           # Entry point: Game loop & State Machine
    ├── world.h          # World Gen, Tile Logic, Conveyors, Blueprints
    ├── player.h         # Inventory, Recipes, Movement
    └── ui.h             # HUD, Menus, Comms Log

---

## BUILD & RUN
```bash
# Linux/Mac (requires raylib installed)
make run

# Or manually:
gcc -std=c99 -O2 main.c -o homeplanet -lraylib -lm
./homeplanet
```

Ideas
2D game, Sliced 3D

---

## CONTROLS

Movement and interaction
| Key | Action |
|-----|--------|
| WASD | Move |
| E | Interact |
| Left Click | Mine tile |
| Right Click | Place selected hotbar tile |

UI
| 1–8 | Select hotbar slot |
| Mouse Wheel | Scroll hotbar |
| TAB | Open/close Crafting Terminal |
| ↑↓ (in craft menu) | Navigate recipes |
| ENTER (in craft menu) | Craft item |

Menu
| F5 | Save world to disk |
| F9 | Load world from disk |
| ESC | Pause menu |
| F11 | Toggle fullscreen |

---

## ITEMS & CRAFTING

energy and resourse extraction
smelting
bioengineered plants - terraforming - mining
electronics, circuits, 
pnematics
plumbing
plastics raw


| Output | Input A | Input B |
|--------|---------|---------|
Ores Iron Copper Gold aluminum

Refinery rubber oil

| Electronics |  |  |
| Circuit | Electronics  | — |
| Cable | Copper | Rubber |


---

## HOTBAR DEFAULTS

1. steam and gas Engines
2. motors small med large, platic for fans and metal for steppers
3. compressors, heat exchangers, 
4. energy storage
5. Conveyor
6. pipes
7. Auto-Miner types and integrated systems
8. Chests shelves and bins
9. 3d printer
10. extruded metal
11. bearing gears motors and mechanical building blocks are components not interactable items, motors are like subcomponents of placable objects
12. laser modules small med large, this is where the scaled crafting system shines, youll see that when you add more to the craft, that you get a more robust and powerful item
13. 
14. 

---


## BASE COMPONENTS
| # | Component | Cost |
|---|-----------|------|
1. Computer
2. server
3. Antenna Dish
4. Cooling system made from a bp researched by analysing a toredown abandond ac
5. Satellite
6. Rocket S M L
7. 



 Computers and networks research & unlock new items
 theres is wind and dust outside
 warehouseing your compute is reccomended
 otherwise youll have to maintain dust filters and put up turrets to prevent destruction and scare theives
 cameras work as agents, observers that indicate of anomolies
> **Heat Warning:** Each Server adds x°C. Each Cooling Fan removes y°C.
> Keep heat below 30°C for stable operation!

---

## TECH CONCEPTS TAUGHT IN-GAME

| Game Mechanic | Real Concept |
|---------------|-------------|
| Bandwidth bar | Network throughput — how much data flows per second |
| Signal Strength | dbm |
| Mesh signal Hz | Radio frequency — the channel your nodes talk on |
| Mhz | Radio Frequenzy for local coms modules |
| Ghz | Computation speed |
| Radio Towers | Meshtastic/LoRa mesh nodes — signal relay devices 
| Conveyor |  |
| Save/Load (F5/F9) | Persistence — writing state to disk, reading it back |
| Crafting recipe lookup | Lookup table / hash map — input keys map to output values |
| 3D Base heat management | Data center cooling — real servers need thermal management |
| Comms Log messages | System log / stdout — how programs report internal events |
BLUE COLLAR TRADES


---

## NEXT EXPANSION IDEAS


1. **Multiplayer Packets**: Fake network "peers" that trade items via simulated UDP
2. **Power Grid**: Solar panels → capacitors → machines (teaches voltage/current)
3. **Signal Encryption**: Antenna dish + Ping Core = encrypted mesh channel
4. **PvP Arena State**: Already wired up in the state machine — just needs content
5. **Research Tree**: Spend Circuits to unlock new tile types
6. **Procedural Dungeons**: Enter craters for loot with roguelike rooms

---

*Built with [raylib](https://www.raylib.com/) — a simple and easy-to-use C library for games.*
