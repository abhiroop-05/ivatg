# 🎮 IVATG — The Simulation

A 3D open-world city simulation game built from scratch in C using [Raylib](https://www.raylib.com/).

## 🕹️ Play Now (Browser)

**▶️ [Click here to play IVATG directly in your browser!](https://abhiroop-05.github.io/ivatg/)**

No download required — runs on WebAssembly.

---

## Features

- **Open World** — Explore a procedurally generated city with roads, sidewalks, buildings, and grass
- **Vehicle System** — Enter/exit parked cars, drive around the city with realistic physics
- **Shooting** — Pistol with 7-round magazine, infinite ammo, auto-reload, and muzzle flash effects
- **Drive-by Shooting** — Shoot while driving vehicles
- **Wanted System** — Kill 3+ NPCs and police will chase you. Get neutralized → BUSTED screen → game restarts
- **Sprint & Jump** — Hold Shift to sprint, Space to jump with gravity physics
- **Camera Modes** — Toggle between third-person isometric and first-person perspective with V
- **Main Menu** — Title screen, Start/Settings/Quit menu
- **Pause Menu** — ESC to pause with Resume/Settings/Quit options
- **Key Remapping** — Customize all keyboard controls in the Settings screen
- **AI NPCs** — Civilians walk around, flee from danger. Police patrol, pursue, and engage wanted players

## Controls

| Action | Key |
|---|---|
| Move | WASD / Arrow keys |
| Sprint | Left Shift (hold) |
| Jump | Space |
| Shoot | Left Mouse Button / F |
| Reload | R |
| Enter/Exit Vehicle | E |
| Brake (in vehicle) | Space |
| Toggle Camera (FPP/TPP) | V |
| Pause | ESC |

## Building from Source (Windows)

### Prerequisites
- [MSYS2](https://www.msys2.org/) with MinGW-w64
- Install dependencies:
  ```bash
  pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-raylib
  ```

### Build
```bash
cd gta69-master
C:\msys64\mingw64\bin\mingw32-make.exe -f Makefile.win clean all
```

### Run
```bash
.\gta_sim.exe
```

> **Note**: `libraylib.dll` and `glfw3.dll` must be in the same directory as the executable.

## Tech Stack

- **Language**: C11
- **Graphics**: Raylib (3D rendering, input, audio)
- **Platform**: Windows (native), Web (WebAssembly via Emscripten)
- **Build**: MinGW-w64 (Windows), Emscripten (Web)

## Project Structure

```
├── main.c          # Game loop, state machine, player controls, shooting
├── renderer.c/h    # 3D rendering, UI screens, HUD, camera system
├── entity.c/h      # Entity system (player, NPCs, vehicles, weapons)
├── physics.c/h     # Physics integration, collisions, gravity
├── ai.c/h          # NPC AI (civilians, police pathfinding & combat)
├── wanted.c/h      # Wanted level system, crime tracking
├── vehicle.c/h     # Vehicle physics & enter/exit mechanics
├── input.c/h       # Input polling with configurable keybinds
├── world.c/h       # World generation, spatial hashing, tile system
├── keybinds.h      # Key remapping configuration
├── config.h        # Game constants and tuning parameters
├── Makefile.win    # Windows build configuration
└── index.html      # Web build shell (GitHub Pages)
```

## License

This project is for educational purposes.
