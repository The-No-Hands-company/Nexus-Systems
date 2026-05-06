# Audio Engine Plan

## Goals

- Deliver a robust, scalable, and low-latency audio engine.
- Support both cinematic and gameplay-reactive sound design.
- Keep audio deterministic enough for gameplay timing and event sync.

## Core Capabilities

- 2D and 3D spatial audio
- Listener and emitter models with distance attenuation
- Mixer buses and submix routing
- Snapshot-based mix states (combat, menu, cinematic, etc.)
- Real-time effects (reverb, filters, compression, delay)
- Event-driven playback with parameter automation
- Streaming and memory-managed sample playback

## Runtime Architecture

- Audio graph abstraction with backend adapter layer.
- Separate audio update pipeline from render frame loop.
- Stable event bridge from gameplay/animation/scripting systems.
- Backend adapter boundary allows WebAudio/Babylon/native backends to plug into the same runtime contract.
- Registered audio assets now replace free-form cue identifiers.
- Asset metadata defines bus ownership, spatial behavior, default levels, and loading policy.
- Snapshot transitions can now interpolate over time instead of hard-switching bus gains.

## Integration Points

- Animation: footstep, impact, and notify-driven sound events.
- Physics: collision magnitude and material-driven audio responses.
- Scripting: visual/text APIs for event emission and parameter control.
- UI: dedicated non-spatial bus and priority control.

## Performance Strategy

- Voice prioritization and pooling.
- Stream long assets; pre-load critical short assets.
- Minimize runtime allocations in mixer and event dispatch paths.
- Candidate native acceleration for DSP-heavy paths.

## Milestones

1. Baseline event playback and bus routing.
2. 3D spatialization and attenuation model.
3. Snapshot/mix-state system and effects chain MVP.
4. Profiling and optimization for high-voice-count scenes.

## MVP Acceptance Criteria (Phase 2 Seed Slice)

- Audio runtime exposes a stable engine interface for queueing, update ticks, and stopping voices.
- Runtime provides a deterministic stub that can be integrated before backend-specific adapters.
- Runtime supports initial bus routing categories: master, music, sfx, ui.
- Voice limits are enforced to prevent uncontrolled growth.
- Tests validate queue processing, overflow behavior, and stop semantics.

## Current Implementation Status

- Interface contract and stub runtime implemented in `src/systems/audio-engine-system.ts`.
- Initial tests implemented in `tests/audio-engine-system.test.ts`.
- This establishes milestone 1 groundwork for event playback and bus routing before spatial and DSP features.

## Current MVP Increment

- Gameplay audio feedback is wired for melee hit, enemy death, and UI confirm flows.
- Stub runtime now supports bus gain controls and snapshot presets (`default`, `combat`, `menu`).
- Backend adapter boundary now exposes voice start/stop, bus gain, snapshot, and per-frame update hooks.
- Audio feedback mapping is currently handled by `src/systems/arpg-audio-feedback-system.ts`.

## Asset Registration And Loading Policy

- `src/systems/audio-engine-system.ts` now owns a registry of `AudioAssetDefinition` entries.
- Assets define:
	- source URL
	- target bus
	- default volume/loop
	- spatial behavior
	- loading policy (`preload`, `on-demand`, `stream`)
- Current default gameplay assets are registered for melee hit, enemy death, and UI confirm.
- Runtime assets are now packaged at `public/audio/*.wav` and referenced via `/audio/<name>.wav` URLs.

## Session Lifecycle Policy

- `prewarmAssets()` prepares preload assets at startup and can prewarm specific on-demand assets.
- `unloadUnusedAssets()` applies idle-time unloading for non-preload assets to reduce long-session memory pressure.
- Default idle unload window is 45 seconds and is configurable per engine instance.

## Mixer State Orchestration

- The runtime now separates base bus gains from effective bus gains.
- Effective gains are derived from snapshot state plus temporary ducking overlays.
- `duckBus()` supports short-lived mix suppression for UI and pickup events without mutating long-lived snapshot intent.

## Music Loop State

- Registered loop assets now include `ambient-loop` and `combat-loop`.
- The ARPG manager ensures the correct music loop is active for the current gameplay state.
- Ambient music runs outside combat; combat music replaces it while lock/combat state is active.

## Authored Runtime Assets

- Packaged engine audio now uses synthesized authored WAV content instead of placeholder near-empty files.
- The current shipped set includes one-shots for melee, enemy death, UI confirm/navigation, footsteps, and loot pickup.
- The loop set now includes `menu-loop`, `ambient-loop`, `cinematic-loop`, and `combat-loop`.
- Assets remain simple and fully in-repo, but they now provide distinct timing, envelopes, and tonal profiles suitable for editor/runtime integration work.

## Editor Mixer Controls

- The editor inspector now exposes mixer controls for the four current buses: `master`, `music`, `sfx`, `ui`.
- The mixer panel also exposes:
	- music-mode override (`auto`, `menu`, `ambient`, `cinematic`, `combat`)
	- active loop state
	- current snapshot / transition target visibility
- While the editor is paused, the manager now continues ticking audio state through a lightweight editor-only path so loop previews, ducking previews, and mode overrides remain audible without entering Play mode.

## Expanded Music-State Orchestration

- Music-state resolution now supports `menu`, `ambient`, `cinematic`, and `combat`.
- `auto` resolves to:
	- `menu` while the game is not in active Play mode
	- `combat` while combat/lock state is active
	- `cinematic` while the camera is in isometric mode
	- `ambient` otherwise
- Editor controls can override that automatic resolution for tuning and preview workflows.

## Babylon Backend Increment

- `src/systems/babylon-audio-backend.ts` provides the first concrete backend adapter.
- The adapter creates Babylon `Sound` instances per active voice and updates effective volume as bus gains change.
- ARPG scene setup now attaches the Babylon backend when the default stub engine is in use.
- The backend also respects loop playback orchestration and unload requests.

## Snapshot Transition Policy

- Snapshot application now supports explicit transition durations.
- Combat feedback uses timed interpolation into the `combat` snapshot rather than a hard cut.
- The runtime exposes both `activeSnapshotId` and `targetSnapshotId` for in-flight transition visibility.

## Gameplay Emitter Coverage

- Combat: melee hit, enemy death
- Movement: cadence-based footsteps
- Loot: pickup event audio
- UI: confirm and navigation events
- Music: menu/ambient/cinematic/combat loop switching

## Near-Term Next Steps

1. Replace the synthesized in-repo assets with final authored production packs and import pipeline support.
2. Add multi-bus snapshot blending instead of single-snapshot transitions.
3. Add per-bus metering, solo/mute, and persistent mixer presets in the editor.
4. Expand music-state orchestration with timeline/cinematic triggers instead of relying only on camera/gameplay heuristics.
