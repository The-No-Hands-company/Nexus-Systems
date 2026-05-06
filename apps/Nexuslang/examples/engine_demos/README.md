# NexusLang Engine Demos

This directory contains example programs demonstrating the NexusLang engine ecosystem:

## Math & Graphics Demos

1. **01_math3d_demo.nlpl** - Vector3, Matrix4, Quaternion operations
2. **02_camera_demo.nlpl** - Camera transformations (coming soon)
3. **03_mesh_loading_demo.nlpl** - Load and display 3D models (coming soon)

## Physics Demos

4. **04_physics_basics.nlpl** - Rigid body physics (coming soon)
5. **05_collision_detection.nlpl** - Collision shapes and queries (coming soon)

## Audio Demos

6. **06_audio_playback.nlpl** - 3D audio positioning (coming soon)

## Full Engine Demos

7. **07_rotating_cube.nlpl** - Complete 3D rendering pipeline (coming soon)
8. **08_first_person_camera.nlpl** - FPS camera controls (coming soon)
9. **09_particle_system.nlpl** - Particle effects (coming soon)
10. **10_simple_game.nlpl** - Complete mini-game (coming soon)

## Running Demos

### Interpreter Mode (Quick testing):
```bash
python src/main.py examples/engine_demos/01_math3d_demo.nlpl
```

### Compiled Mode (Better performance):
```bash
./nlplc examples/engine_demos/01_math3d_demo.nlpl -o demo --run
```

## Requirements

- **Base demos** (01-03): No external dependencies
- **Physics demos** (04-05): Bullet Physics library
- **Audio demos** (06): OpenAL library
- **Rendering demos** (07-10): Vulkan or OpenGL

## Development Status

**Phase 1 (Feb-Apr 2026)**: Graphics Foundation
- [x] 3D math library (Vector3, Matrix4, Quaternion)
- [ ] Vulkan/OpenGL abstraction
- [ ] Mesh loading (OBJ, FBX, glTF)
- [ ] Material system

**Phase 2 (May-Jul 2026)**: Engine Architecture
- [ ] Scene graph & ECS
- [ ] Physics integration
- [ ] Audio engine

See `docs/8_planning/ECOSYSTEM_ROADMAP_2026.md` for full plan.
