# Bouncer

A simple 2D C++ game using SDL2.

## Project Structure

```
.
├── src/            # Source files (.cpp)
├── include/        # Header files (.h)
├── assets/         # Game assets (images, sounds, etc.)
├── build/          # Build directory (generated)
├── bin/            # Compiled executable (generated)
└── CMakeLists.txt  # Build configuration
```

## Prerequisites

- CMake (3.10 or higher)
- SDL2
- SDL2_image
- SDL2_ttf
- C++17 compatible compiler

## Building

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

### Ubuntu/Mint packages

```bash
sudo apt install -y build-essential cmake libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
```

## Running

After building, run the game from the project root:

```bash
cd ..
./bin/SDLGame
```

## Current gameplay

There are two ball modes you can toggle between:

- **Soccer ball (blue):** kick/dribble style control.
- **Basketball (orange):** energy-based vertical bounces + mid-air shooting.

A small HUD is shown in the top-left with the current ball mode, position, velocity, and (for basketball) energy.

## Controls

### General

- **ESC**: quit

### Gameplay

- **Arrow keys / A,D**: move left/right
- **Space**: bounce / jump timing (also used for “bounce next to” interactions)
- **TAB**: toggle ball mode (Soccer ↔ Basketball)

### Basketball mode

- **E**: shoot (only when the player and ball are touching *and* both are in the air)

### Editor

- **P**: toggle editor mode
- **1**: place solid platform
- **2**: place fall-through platform
- **3**: set start point
- **4**: set finish point
- **5**: delete platform
- **6**: place basket target
- **Mouse left click**: place the selected item (grid-snapped)
- **Arrow keys** (in editor): move camera by half a screen
- **S**: save level to `level.txt`
- **L**: load level from `level.txt`

## Level format

Levels are stored in `level.txt`. Newer versions append basket targets after the platform list. Older `level.txt` files (without baskets) should still load.
