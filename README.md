# HOME PLANET: VOID RUNNER v2.0
## A 2D/3D Hybrid Factory & Networking Sandbox

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

---

## CONTROLS

### Surface (2D)
| Key | Action |
|-----|--------|
| WASD | Move |
| Left Click | Mine tile |
| Right Click | Place selected hotbar tile |
| 1–9 | Select hotbar slot |
| Mouse Wheel | Scroll hotbar |
| TAB | Open/close Crafting Terminal |
| ↑↓ (in craft menu) | Navigate recipes |
| ENTER (in craft menu) | Craft item |
| B | Open/close Blueprint Terminal |
| C (in blueprint) | Capture 5×5 blueprint around you |
| P (in blueprint) | Paste blueprint at your position |
| E (on Rocket Pad) | Enter 3D Base |
| F5 | Save world to disk |
| F9 | Load world from disk |
| ESC | Pause menu |
| F11 | Toggle fullscreen |

### 3D Base
| Key | Action |
|-----|--------|
| Mouse (orbital) | Rotate camera |
| 1–4 | Select component type |
| SPACE | Place selected component |
| ESC | Exit back to surface |

---

## ITEMS & CRAFTING

| Output | Input A | Input B |
|--------|---------|---------|
| Electronics (1) | Wood ×3 | Stone ×2 |
| Circuit (1) | Electronics ×2 | — |
| Cable (2) | Wood ×2 | Electronics ×1 |
| Antenna (1) | Circuit ×1 | Stone ×3 |
| Ping Core (1) | Antenna ×1 | Circuit ×2 |

---

## HOTBAR DEFAULTS
1. Ping Tower (costs 20 Wood)
2. Conveyor ↑ (costs 5 Wood)
3. Conveyor ↓ (costs 5 Wood)
4. Conveyor → (costs 5 Wood)
5. Conveyor ← (costs 5 Wood)
6. Auto-Miner (costs 20 Stone)
7. Chest (costs 10 Wood)
8. Dirt
9. Dirt

---

## 3D BASE COMPONENTS
| # | Component | Cost |
|---|-----------|------|
| 1 | Server Unit | Electronics ×3 |
| 2 | Cooling Fan | Stone ×5 |
| 3 | Antenna Dish | Antenna ×1 |
| 4 | Cable Rack | Cable ×2 |

> **Heat Warning:** Each Server adds 10°C. Each Cooling Fan removes 8°C.
> Keep heat below 30°C for stable operation!

---

## TECH CONCEPTS TAUGHT IN-GAME

| Game Mechanic | Real Concept |
|---------------|-------------|
| Bandwidth bar | Network throughput — how much data flows per second |
| Ping Towers | Meshtastic/LoRa mesh nodes — signal relay devices |
| Mesh signal Hz | Radio frequency — the channel your nodes talk on |
| Conveyor belt speed scaling with bandwidth | QoS (Quality of Service) — faster network = faster throughput |
| Auto-Miner daemon | Background process / daemon — runs without player input |
| Blueprint code string | Serialisation — converting a structure into shareable data |
| Save/Load (F5/F9) | Persistence — writing state to disk, reading it back |
| Crafting recipe lookup | Lookup table / hash map — input keys map to output values |
| 3D Base heat management | Data center cooling — real servers need thermal management |
| Comms Log messages | System log / stdout — how programs report internal events |

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
