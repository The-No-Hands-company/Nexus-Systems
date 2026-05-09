# Simple Game Loop Showcase

File: examples/game/simple_game_loop.nxl

This showcase demonstrates a compact game-loop architecture in NexusLang:

- World and player state structs
- Frame-by-frame update loop
- Input -> simulation -> render style flow
- Boundary collision penalties and health depletion
- Deterministic scripted input for reproducible behavior

## Core Loop Pattern

1. Read command for the current frame
2. Apply input to velocity
3. Update position and collision state
4. Render frame information
5. Advance frame counter

## Why This Matters

Game and simulation programs rely on stable update-loop semantics. This example shows that NexusLang can model that flow directly with readable control logic and explicit state transitions.

## Run

python src/main.py examples/game/simple_game_loop.nxl
