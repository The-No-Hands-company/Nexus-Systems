# DAWG Timeline/Arrangement API

## Core Concepts
- Timeline holds clips (audio, MIDI) and loop/tempo/time signature info.
- Clips have start time, length, and type (audio/midi).
- Transport controls playback, looping, and navigation.

## Editing API
- `addClip(clip)`: Add a clip to the timeline.
- `removeClip(clip)`: Remove a clip.
- `moveClip(clip, newStart)`: Move a clip to a new start time.
- `stretchClip(clip, newLength)`: Change clip length.
- `snapClip(clip, grid)`: Snap clip start/length to grid.
- `setLoop(start, end)`: Set loop region.
- `setTempo(bpm)`: Set timeline tempo.
- `setTimeSignature(num, den)`: Set time signature.

## UI Integration (Qt)
- `TimelineView` provides abstract UI hooks for rendering and editing.
- Implement concrete Qt classes for track view, clip editing, piano roll, etc.
- Connect UI actions to API methods above.

## Advanced Features
- Sample-accurate looping: loop region can be set in samples or beats.
- Clip stretching: time/pitch stretching for audio clips.
- Snapping: clips snap to grid (beats, bars, samples).
- Selection: select/multi-select clips for editing.
- Arrangement navigation: scroll, zoom, drag, etc.
