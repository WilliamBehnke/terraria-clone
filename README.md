# Terra-Clone

This repository houses the beginnings of a modern C++ Terraria-like sandbox game. The code base is organized to let you expand toward a feature-complete sandbox RPG using cleanly separated modules.

## Project Goals

- **Modern C++20** implementation with a modular architecture.
- **Data-oriented core** where world state and entities are clearly separated from systems that manipulate them.
- **Renderer/input abstraction** so you can later plug in SDL, GLFW, bgfx, or another backend.
- **Procedural world generation pipeline** ready for expansion.

## Layout

```
include/terraria/     Public headers grouped by subsystem.
src/                  Source files mirroring the header layout.
assets/               (future) textures, sounds, localization.
cmake/                (future) reusable cmake modules or toolchains.
```

Key subsystems scaffolded:

- `core` – application bootstrap, configuration utilities.
- `game` – high-level game loop management.
- `world` – tiles, world chunks, procedural generation entry point.
- `entities` – player and NPC representations.
- `rendering` – renderer abstraction backed by an SDL2 polygon renderer.
- `input` – input system interface.

## Prerequisites

- SDL2 (macOS: `brew install sdl2`, Linux: `sudo apt install libsdl2-dev`)

## Building

```bash
make          # builds to build/terra_clone
./build/terra_clone
```

> Prefer `make` for quick iteration. A `CMakeLists.txt` is also provided if you want IDE integration or cross-platform generators.

Controls: `A/D` or arrow keys to move, `Space` to jump, left mouse to break tiles (hold to mine, watch the crack animation), right mouse to place the selected block within reach, number keys `1-8` to choose a hotbar slot, `C` toggles fast free-camera mode (use arrows/WASD to pan quickly), `Esc` or window close to quit. Tiles inside your placement reach highlight, and the HUD shows block counts in real time.

## Next Steps

1. Expand the SDL2 renderer/input stack with lighting, UI, particle effects, and richer controls.
2. Flesh out `WorldGenerator` with true procedural terrain, biomes, caves, and ore placement.
3. Implement a robust update loop with delta timing, physics, crafting, inventory, multiplayer, etc.
4. Add automated testing (Catch2, doctest, or GoogleTest) for world generation and entity logic.
