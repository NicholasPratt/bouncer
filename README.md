# SDL Game Project

A C++ game project using SDL2.

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
- C++17 compatible compiler

## Building

```bash
cd build
cmake ..
cmake --build .
```

## Running

After building, run the game:

```bash
./bin/SDLGame
```

## Controls

- ESC: Exit the game
- Window X button: Exit the game

## Features

The current implementation includes:
- SDL2 window creation
- Renderer setup
- Basic game loop
- Event handling
- Simple rendering (white rectangle on black background)
