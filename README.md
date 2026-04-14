<p align="center">
  <h1 align="center">🎮 IVATG</h1>
  <p align="center"><strong>A 3D open-world city simulation game built from scratch in C</strong></p>
  <p align="center">
    <img src="https://img.shields.io/badge/Language-C11-blue?style=flat-square" alt="Language">
    <img src="https://img.shields.io/badge/Graphics-Raylib-orange?style=flat-square" alt="Raylib">
    <img src="https://img.shields.io/badge/Platform-Windows-green?style=flat-square" alt="Platform">
  </p>
</p>

---

## Overview

IVATG is a GTA-inspired 3D city simulation featuring an open world to explore on foot or by vehicle, a full combat system with a pistol, a dynamic wanted system with police chases, and multiple camera perspectives — all written in pure C with Raylib for rendering.

## Features

- **Open World** — Procedurally generated city with roads, sidewalks, buildings, and parks
- **Vehicle System** — Enter and exit parked cars, drive with realistic steering and braking physics
- **Combat** — Pistol with a 7-round magazine, infinite reserve ammo, auto/manual reload, muzzle flash, and bullet tracers
- **Drive-by Shooting** — Fire your weapon while behind the wheel
- **Wanted System** — Kill 3+ NPCs and the police will pursue and attempt to neutralize you
- **BUSTED Mechanic** — Get caught by police and you're shown a cinematic BUSTED screen before the game restarts
- **Sprint & Jump** — Hold Shift to run, press Space to jump with gravity-based physics
- **Dual Camera** — Toggle between third-person isometric and first-person perspective
- **Pause Menu** — Press ESC to pause with Resume, Settings, and Quit options
- **Key Remapping** — Fully customize your keyboard controls from the Settings screen
- **AI** — Civilians walk around and flee from danger; police patrol, pursue, and engage wanted players

## Controls

| Action | Default Key |
|:--|:--|
| Move | `W` `A` `S` `D` |
| Sprint | `Left Shift` (hold) |
| Jump | `Space` |
| Shoot | `Left Mouse Button` / `F` |
| Reload | `R` |
| Enter / Exit Vehicle | `E` |
| Brake (in vehicle) | `Space` |
| Toggle Camera | `V` |
| Pause | `ESC` |

> All controls can be remapped in **Settings**.

## Building from Source

### Prerequisites

- **[MSYS2](https://www.msys2.org/)** with MinGW-w64 toolchain
- Install the required packages:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-raylib
```

### Build & Run

```bash
cd ivatg
C:\msys64\mingw64\bin\mingw32-make.exe -f Makefile.win clean all
.\gta_sim.exe
```

> **Note:** Ensure `libraylib.dll` and `glfw3.dll` are in the same directory as the executable, or are available on your system PATH.

## Project Structure

```
ivatg/
├── main.c           Game loop, state machine, player logic, shooting
├── renderer.c/.h    3D rendering, UI screens, HUD, camera system
├── entity.c/.h      Entity system — player, NPCs, vehicles, weapons
├── physics.c/.h     Physics engine with gravity, collisions, Z-axis
├── ai.c/.h          NPC AI — civilians, police pathfinding & combat
├── wanted.c/.h      Wanted level system, crime tracking, police spawning
├── vehicle.c/.h     Vehicle physics, enter/exit mechanics
├── input.c/.h       Input polling with configurable keybinds
├── world.c/.h       World generation, spatial hashing, tile system
├── keybinds.h       Key remapping configuration & defaults
├── config.h         Game constants and tuning parameters
└── Makefile.win     Windows build configuration
```

## Tech Stack

| Component | Technology |
|:--|:--|
| Language | C11 |
| Rendering | [Raylib](https://www.raylib.com/) (OpenGL) |
| Build System | GNU Make (MinGW-w64) |
| Platform | Windows |

## License

This project is for educational and demonstration purposes.
