"""Audio format handling module for NexusLang.

Provides WAV file I/O, PCM manipulation, audio generation, and
analysis using only Python standard library modules (wave, struct,
math, random).  No external dependencies.

Features:
- WAV I/O: Read / write WAV files (8 / 16 / 24 / 32-bit, any channel count)
- Sample Access: Get / set normalized float samples per channel (-1.0 .. 1.0)
- Operations: mix, concat, slice, reverse, normalize, gain, fade, trim, resample
- Channel Ops: mono-to-stereo, stereo-to-mono, multi-channel downmix
- Generation: sine, square, sawtooth, white noise, silence
- Analysis: peak, RMS, dB conversion, zero crossing count
- Format Utils: detect file format, convert to/from raw PCM bytes

All audio objects are stored by integer ID (handle-based API, similar
to image_utils). Samples are kept as Python floats in the range -1.0 .. 1.0
irrespective of the on-disk bit depth.

Example usage in NexusLang:
    # Load, normalize, and resave a WAV file
    set aud to audio_load_wav with "speech.wav"
    set info to audio_info with aud
    set norm to audio_normalize with aud and 0.9
    audio_save_wav with norm and "speech_normalized.wav"

    # Generate a 440 Hz tone and save it
    set tone to audio_sine_wave with 440.0 and 2.0 and 44100 and 0.8
    audio_save_wav with tone and "tone_440hz.wav"

    # Mix two tracks together
    set mix to audio_mix with track1 and track2 and 0.7 and 0.7
    audio_save_wav with mix and "mix.wav"
"""

import wave
import math
import struct
import random

from ...runtime.runtime import Runtime

# ---------------------------------------------------------------------------
# Internal storage
# ---------------------------------------------------------------------------

_audios: dict = {}
_audio_counter: int = 0


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _new_audio(samples_per_channel, framerate, sampwidth, nchannels):
    """Store a new audio record and return its integer ID."""
    global _audio_counter
    audio_id = _audio_counter
    _audios[audio_id] = {
        "samples": samples_per_channel,  # list[list[float]]  [channel][frame]
        "framerate": int(framerate),
        "sampwidth": int(sampwidth),     # bytes per sample: 1/2/3/4
        "nchannels": int(nchannels),
    }
    _audio_counter += 1
    return audio_id


def _get_audio(audio_id):
    """Look up an audio record, raising RuntimeError if not found."""
    key = int(audio_id)
    if key not in _audios:
        raise RuntimeError(f"Audio ID {key} not found")
    return _audios[key]


def _nframes(audio):
    """Return the number of frames in an audio record."""
    if not audio["samples"] or not audio["samples"][0]:
        return 0
    return len(audio["samples"][0])


def _clamp(v, lo=-1.0, hi=1.0):
    """Clamp a float to [lo, hi]."""
    return lo if v < lo else (hi if v > hi else v)


def _bytes_to_samples(raw_bytes, sampwidth, nframes, nchannels):
    """Convert interleaved raw WAV bytes to per-channel float lists.

    Supports 8-bit unsigned (sampwidth=1), 16-bit signed (2),
    24-bit signed (3), and 32-bit signed (4) little-endian formats.
    """
    channels = [[] for _ in range(nchannels)]

    if sampwidth == 1:
        for i in range(nframes):
            for ch in range(nchannels):
                b = raw_bytes[i * nchannels + ch]
                channels[ch].append((b - 128) / 128.0)

    elif sampwidth == 2:
        frame_size = 2 * nchannels
        for i in range(nframes):
            base = i * frame_size
            for ch in range(nchannels):
                off = base + ch * 2
                v = struct.unpack_from("<h", raw_bytes, off)[0]
                channels[ch].append(v / 32768.0)

    elif sampwidth == 3:
        frame_size = 3 * nchannels
        for i in range(nframes):
            base = i * frame_size
            for ch in range(nchannels):
                off = base + ch * 3
                b0 = raw_bytes[off]
                b1 = raw_bytes[off + 1]
                b2 = raw_bytes[off + 2]
                v = b0 | (b1 << 8) | (b2 << 16)
                if v >= 0x800000:
                    v -= 0x1000000
                channels[ch].append(v / 8388608.0)

    elif sampwidth == 4:
        frame_size = 4 * nchannels
        for i in range(nframes):
            base = i * frame_size
            for ch in range(nchannels):
                off = base + ch * 4
                v = struct.unpack_from("<i", raw_bytes, off)[0]
                channels[ch].append(v / 2147483648.0)

    else:
        raise RuntimeError(f"Unsupported sample width: {sampwidth} bytes")

    return channels


def _samples_to_bytes(channels, sampwidth, nframes):
    """Convert per-channel float lists to interleaved raw WAV bytes."""
    nchannels = len(channels)
    parts = []

    if sampwidth == 1:
        for i in range(nframes):
            for ch in range(nchannels):
                v = channels[ch][i] if i < len(channels[ch]) else 0.0
                b = max(0, min(255, int(v * 128.0 + 128)))
                parts.append(bytes([b]))

    elif sampwidth == 2:
        for i in range(nframes):
            for ch in range(nchannels):
                v = channels[ch][i] if i < len(channels[ch]) else 0.0
                iv = max(-32768, min(32767, int(v * 32767.0)))
                parts.append(struct.pack("<h", iv))

    elif sampwidth == 3:
        for i in range(nframes):
            for ch in range(nchannels):
                v = channels[ch][i] if i < len(channels[ch]) else 0.0
                iv = max(-8388608, min(8388607, int(v * 8388607.0)))
                if iv < 0:
                    iv += 0x1000000
                parts.append(
                    bytes([iv & 0xFF, (iv >> 8) & 0xFF, (iv >> 16) & 0xFF])
                )

    elif sampwidth == 4:
        for i in range(nframes):
            for ch in range(nchannels):
                v = channels[ch][i] if i < len(channels[ch]) else 0.0
                iv = max(-2147483648, min(2147483647, int(v * 2147483647.0)))
                parts.append(struct.pack("<i", iv))

    else:
        raise RuntimeError(f"Unsupported sample width: {sampwidth} bytes")

    return b"".join(parts)


# ===========================================================================
# WAV I/O
# ===========================================================================

def audio_load_wav(filename):
    """Load a WAV file and return an audio ID.

    Args:
        filename: Path to the WAV file.

    Returns:
        Integer audio ID for use with other audio_* functions.
    """
    with wave.open(str(filename), "rb") as wf:
        nchannels = wf.getnchannels()
        sampwidth = wf.getsampwidth()
        framerate = wf.getframerate()
        nframes = wf.getnframes()
        raw = wf.readframes(nframes)
    channels = _bytes_to_samples(raw, sampwidth, nframes, nchannels)
    return _new_audio(channels, framerate, sampwidth, nchannels)


def audio_save_wav(audio_id, filename, sampwidth=None):
    """Save an audio to a WAV file.

    Args:
        audio_id: Integer audio ID.
        filename: Destination file path.
        sampwidth: Optional output bit depth (1=8-bit, 2=16-bit, 3=24-bit,
                   4=32-bit).  Defaults to the audio's own sampwidth.
    """
    audio = _get_audio(audio_id)
    sw = int(sampwidth) if sampwidth is not None else audio["sampwidth"]
    if sw not in (1, 2, 3, 4):
        raise RuntimeError(
            f"Invalid sampwidth {sw}: must be 1 (8-bit), 2 (16-bit), "
            "3 (24-bit), or 4 (32-bit)"
        )
    nframes = _nframes(audio)
    raw = _samples_to_bytes(audio["samples"], sw, nframes)
    with wave.open(str(filename), "wb") as wf:
        wf.setnchannels(audio["nchannels"])
        wf.setsampwidth(sw)
        wf.setframerate(audio["framerate"])
        wf.writeframes(raw)


def audio_create(framerate=44100, nchannels=1, sampwidth=2):
    """Create a new empty audio object (0 frames).

    Args:
        framerate: Sample rate in Hz (default 44100).
        nchannels: Number of channels (default 1 / mono).
        sampwidth: Bytes per sample (default 2 / 16-bit).

    Returns:
        Integer audio ID.
    """
    channels = [[] for _ in range(int(nchannels))]
    return _new_audio(channels, int(framerate), int(sampwidth), int(nchannels))


def audio_free(audio_id):
    """Remove an audio object from memory.

    Args:
        audio_id: Integer audio ID to free.
    """
    key = int(audio_id)
    if key in _audios:
        del _audios[key]


def audio_info(audio_id):
    """Return a dict with metadata about an audio object.

    Returns:
        Dict with keys: channels, framerate, sampwidth, frames, duration,
        bit_depth.
    """
    audio = _get_audio(audio_id)
    nframes = _nframes(audio)
    duration = nframes / audio["framerate"] if audio["framerate"] > 0 else 0.0
    return {
        "channels": audio["nchannels"],
        "framerate": audio["framerate"],
        "sampwidth": audio["sampwidth"],
        "frames": nframes,
        "duration": duration,
        "bit_depth": audio["sampwidth"] * 8,
    }


# ===========================================================================
# Basic accessors
# ===========================================================================

def audio_duration(audio_id):
    """Return the duration of the audio in seconds.

    Args:
        audio_id: Integer audio ID.

    Returns:
        Duration as a float.
    """
    audio = _get_audio(audio_id)
    nframes = _nframes(audio)
    return nframes / audio["framerate"] if audio["framerate"] > 0 else 0.0


def audio_sample_rate(audio_id):
    """Return the sample rate (framerate) in Hz.

    Args:
        audio_id: Integer audio ID.

    Returns:
        Integer sample rate.
    """
    return _get_audio(audio_id)["framerate"]


def audio_channels(audio_id):
    """Return the number of channels.

    Args:
        audio_id: Integer audio ID.

    Returns:
        Integer channel count.
    """
    return _get_audio(audio_id)["nchannels"]


def audio_get_samples(audio_id, channel=0):
    """Return the sample list for a single channel as normalized floats.

    Values are in the range -1.0 .. 1.0.

    Args:
        audio_id: Integer audio ID.
        channel: Zero-based channel index (default 0).

    Returns:
        List of float samples.
    """
    audio = _get_audio(audio_id)
    ch = int(channel)
    if ch < 0 or ch >= audio["nchannels"]:
        raise RuntimeError(
            f"Channel {ch} out of range (0..{audio['nchannels'] - 1})"
        )
    return list(audio["samples"][ch])


def audio_set_samples(audio_id, channel, samples):
    """Replace the sample list for a single channel.

    Args:
        audio_id: Integer audio ID.
        channel: Zero-based channel index.
        samples: Iterable of floats in the range -1.0 .. 1.0.
    """
    audio = _get_audio(audio_id)
    ch = int(channel)
    if ch < 0 or ch >= audio["nchannels"]:
        raise RuntimeError(
            f"Channel {ch} out of range (0..{audio['nchannels'] - 1})"
        )
    audio["samples"][ch] = [float(s) for s in samples]


def audio_from_samples(samples, framerate=44100, sampwidth=2, nchannels=None):
    """Create an audio object from a Python list of samples.

    Args:
        samples: Either a flat list of floats (mono) or a list of per-channel
                 lists (multi-channel).
        framerate: Sample rate in Hz (default 44100).
        sampwidth: Bytes per sample (default 2 / 16-bit).
        nchannels: Override channel count.  When omitted the channel count is
                   inferred from the structure of *samples*.

    Returns:
        Integer audio ID.
    """
    framerate = int(framerate)
    sampwidth = int(sampwidth)

    if samples and isinstance(samples[0], (list, tuple)):
        # Multi-channel input
        nch = int(nchannels) if nchannels is not None else len(samples)
        channels = [
            [float(s) for s in samples[ch]] for ch in range(nch)
        ]
    else:
        # Flat / mono input
        nch = int(nchannels) if nchannels is not None else 1
        flat = [float(s) for s in samples]
        channels = [flat[:] for _ in range(nch)]

    return _new_audio(channels, framerate, sampwidth, nch)


# ===========================================================================
# Operations (all return a new audio ID; originals are unchanged)
# ===========================================================================

def audio_concat(audio_id1, audio_id2):
    """Concatenate two audio objects end-to-end.

    Both audios must have the same channel count and framerate.

    Args:
        audio_id1: First audio ID.
        audio_id2: Second audio ID.

    Returns:
        New integer audio ID.
    """
    a1 = _get_audio(audio_id1)
    a2 = _get_audio(audio_id2)
    if a1["nchannels"] != a2["nchannels"]:
        raise RuntimeError(
            "audio_concat: channel count mismatch "
            f"({a1['nchannels']} vs {a2['nchannels']})"
        )
    if a1["framerate"] != a2["framerate"]:
        raise RuntimeError(
            "audio_concat: framerate mismatch "
            f"({a1['framerate']} vs {a2['framerate']}); "
            "use audio_resample first"
        )
    channels = [
        a1["samples"][ch] + a2["samples"][ch]
        for ch in range(a1["nchannels"])
    ]
    return _new_audio(channels, a1["framerate"], a1["sampwidth"], a1["nchannels"])


def audio_slice(audio_id, start_sec, end_sec=None):
    """Extract a time-range slice from an audio object.

    Args:
        audio_id: Integer audio ID.
        start_sec: Start time in seconds (inclusive).
        end_sec: End time in seconds (exclusive).  Defaults to the end.

    Returns:
        New integer audio ID containing the extracted portion.
    """
    audio = _get_audio(audio_id)
    nframes = _nframes(audio)
    fr = audio["framerate"]

    start_frame = max(0, int(float(start_sec) * fr))
    if end_sec is None:
        end_frame = nframes
    else:
        end_frame = min(nframes, int(float(end_sec) * fr))

    channels = [
        audio["samples"][ch][start_frame:end_frame]
        for ch in range(audio["nchannels"])
    ]
    return _new_audio(channels, fr, audio["sampwidth"], audio["nchannels"])


def audio_mix(audio_id1, audio_id2, gain1=1.0, gain2=1.0):
    """Mix two audio objects sample-by-sample with independent gain factors.

    If the audios have different lengths the shorter one is zero-padded.
    Mixed samples are clamped to -1.0 .. 1.0.

    Args:
        audio_id1: First audio ID.
        audio_id2: Second audio ID.
        gain1: Linear gain for the first input (default 1.0).
        gain2: Linear gain for the second input (default 1.0).

    Returns:
        New integer audio ID.
    """
    a1 = _get_audio(audio_id1)
    a2 = _get_audio(audio_id2)
    if a1["nchannels"] != a2["nchannels"]:
        raise RuntimeError(
            "audio_mix: channel count mismatch "
            f"({a1['nchannels']} vs {a2['nchannels']})"
        )
    if a1["framerate"] != a2["framerate"]:
        raise RuntimeError(
            "audio_mix: framerate mismatch "
            f"({a1['framerate']} vs {a2['framerate']})"
        )

    g1 = float(gain1)
    g2 = float(gain2)
    n1 = _nframes(a1)
    n2 = _nframes(a2)
    n = max(n1, n2)

    channels = []
    for ch in range(a1["nchannels"]):
        s1 = a1["samples"][ch]
        s2 = a2["samples"][ch]
        mixed = [
            _clamp(
                (s1[i] * g1 if i < n1 else 0.0) +
                (s2[i] * g2 if i < n2 else 0.0)
            )
            for i in range(n)
        ]
        channels.append(mixed)

    return _new_audio(channels, a1["framerate"], a1["sampwidth"], a1["nchannels"])


def audio_reverse(audio_id):
    """Reverse the sample order of an audio object.

    Args:
        audio_id: Integer audio ID.

    Returns:
        New integer audio ID.
    """
    audio = _get_audio(audio_id)
    channels = [
        list(reversed(audio["samples"][ch]))
        for ch in range(audio["nchannels"])
    ]
    return _new_audio(
        channels, audio["framerate"], audio["sampwidth"], audio["nchannels"]
    )


def audio_normalize(audio_id, target_peak=1.0):
    """Scale all samples so that the peak absolute amplitude equals target_peak.

    If the audio is silent (all zeros), a copy is returned unchanged.

    Args:
        audio_id: Integer audio ID.
        target_peak: Desired peak absolute value (default 1.0).

    Returns:
        New integer audio ID.
    """
    audio = _get_audio(audio_id)
    target = float(target_peak)

    peak = max(
        (abs(s) for ch in range(audio["nchannels"]) for s in audio["samples"][ch]),
        default=0.0,
    )

    if peak < 1e-10:
        channels = [list(audio["samples"][ch]) for ch in range(audio["nchannels"])]
        return _new_audio(
            channels, audio["framerate"], audio["sampwidth"], audio["nchannels"]
        )

    scale = target / peak
    channels = [
        [_clamp(s * scale) for s in audio["samples"][ch]]
        for ch in range(audio["nchannels"])
    ]
    return _new_audio(
        channels, audio["framerate"], audio["sampwidth"], audio["nchannels"]
    )


def audio_apply_gain(audio_id, gain):
    """Multiply every sample by gain.  Samples are clamped to -1.0 .. 1.0.

    Args:
        audio_id: Integer audio ID.
        gain: Linear gain factor.

    Returns:
        New integer audio ID.
    """
    audio = _get_audio(audio_id)
    g = float(gain)
    channels = [
        [_clamp(s * g) for s in audio["samples"][ch]]
        for ch in range(audio["nchannels"])
    ]
    return _new_audio(
        channels, audio["framerate"], audio["sampwidth"], audio["nchannels"]
    )


def audio_fade(audio_id, fade_in=0.0, fade_out=0.0):
    """Apply linear fade-in and/or fade-out envelopes.

    Args:
        audio_id: Integer audio ID.
        fade_in: Duration of the fade-in ramp in seconds (default 0.0).
        fade_out: Duration of the fade-out ramp in seconds (default 0.0).

    Returns:
        New integer audio ID.
    """
    audio = _get_audio(audio_id)
    nframes = _nframes(audio)
    fr = audio["framerate"]

    fade_in_frames = min(nframes, int(float(fade_in) * fr))
    fade_out_frames = min(nframes, int(float(fade_out) * fr))

    channels = []
    for ch in range(audio["nchannels"]):
        new_ch = list(audio["samples"][ch])
        for i in range(fade_in_frames):
            if fade_in_frames > 0:
                new_ch[i] *= i / fade_in_frames
        for i in range(fade_out_frames):
            frame_idx = nframes - fade_out_frames + i
            if 0 <= frame_idx < nframes and fade_out_frames > 0:
                new_ch[frame_idx] *= (fade_out_frames - i) / fade_out_frames
        channels.append(new_ch)

    return _new_audio(
        channels, fr, audio["sampwidth"], audio["nchannels"]
    )


def audio_trim_silence(audio_id, threshold=0.01, channel=0):
    """Trim leading and trailing silence from an audio object.

    Silence is defined as any frame where the reference channel has
    absolute sample value below *threshold*.

    Args:
        audio_id: Integer audio ID.
        threshold: Amplitude threshold below which a sample is considered
                   silent (default 0.01).
        channel: Reference channel index for silence detection (default 0).

    Returns:
        New integer audio ID.
    """
    audio = _get_audio(audio_id)
    nframes = _nframes(audio)
    ch = int(channel)
    if ch >= audio["nchannels"]:
        ch = 0
    thresh = float(threshold)
    samples = audio["samples"][ch]

    start = 0
    while start < nframes and abs(samples[start]) < thresh:
        start += 1

    end = nframes - 1
    while end > start and abs(samples[end]) < thresh:
        end -= 1
    end += 1

    channels = [
        audio["samples"][c][start:end] for c in range(audio["nchannels"])
    ]
    return _new_audio(
        channels, audio["framerate"], audio["sampwidth"], audio["nchannels"]
    )


def audio_resample(audio_id, new_framerate):
    """Resample an audio object to a new sample rate using linear interpolation.

    Args:
        audio_id: Integer audio ID.
        new_framerate: Target sample rate in Hz.

    Returns:
        New integer audio ID resampled to new_framerate.
    """
    audio = _get_audio(audio_id)
    old_fr = audio["framerate"]
    new_fr = int(new_framerate)

    if old_fr == new_fr:
        channels = [list(audio["samples"][ch]) for ch in range(audio["nchannels"])]
        return _new_audio(
            channels, new_fr, audio["sampwidth"], audio["nchannels"]
        )

    old_nframes = _nframes(audio)
    new_nframes = int(old_nframes * new_fr / old_fr) if old_fr > 0 else 0
    ratio = old_nframes / new_nframes if new_nframes > 0 else 1.0

    channels = []
    for ch in range(audio["nchannels"]):
        src = audio["samples"][ch]
        dst = []
        for i in range(new_nframes):
            src_pos = i * ratio
            src_idx = int(src_pos)
            frac = src_pos - src_idx
            if src_idx + 1 < old_nframes:
                v = src[src_idx] * (1.0 - frac) + src[src_idx + 1] * frac
            elif src_idx < old_nframes:
                v = src[src_idx]
            else:
                v = 0.0
            dst.append(v)
        channels.append(dst)

    return _new_audio(channels, new_fr, audio["sampwidth"], audio["nchannels"])


# ===========================================================================
# Channel operations
# ===========================================================================

def audio_mono_to_stereo(audio_id):
    """Duplicate a mono audio into a two-channel stereo object.

    Args:
        audio_id: Integer audio ID of a mono (1-channel) audio.

    Returns:
        New integer audio ID with 2 channels (left and right are identical).
    """
    audio = _get_audio(audio_id)
    if audio["nchannels"] != 1:
        raise RuntimeError(
            "audio_mono_to_stereo: input must be mono (1 channel), "
            f"got {audio['nchannels']} channels"
        )
    channels = [list(audio["samples"][0]), list(audio["samples"][0])]
    return _new_audio(
        channels, audio["framerate"], audio["sampwidth"], 2
    )


def audio_stereo_to_mono(audio_id):
    """Average all channels into a single mono channel.

    Args:
        audio_id: Integer audio ID with 2 or more channels.

    Returns:
        New integer audio ID with 1 channel.
    """
    audio = _get_audio(audio_id)
    if audio["nchannels"] < 2:
        raise RuntimeError(
            "audio_stereo_to_mono: input must have at least 2 channels, "
            f"got {audio['nchannels']}"
        )
    nframes = _nframes(audio)
    nch = audio["nchannels"]
    mono = [
        _clamp(sum(audio["samples"][ch][i] for ch in range(nch)) / nch)
        for i in range(nframes)
    ]
    return _new_audio(
        [mono], audio["framerate"], audio["sampwidth"], 1
    )


# ===========================================================================
# Generation (returns new audio IDs)
# ===========================================================================

def audio_sine_wave(freq, duration, framerate=44100, amplitude=1.0, nchannels=1):
    """Generate a sinusoidal tone.

    Args:
        freq: Frequency in Hz.
        duration: Duration in seconds.
        framerate: Sample rate (default 44100).
        amplitude: Peak amplitude in the range 0.0 .. 1.0 (default 1.0).
        nchannels: Number of output channels (default 1).

    Returns:
        Integer audio ID (sampwidth = 2, 16-bit).
    """
    freq = float(freq)
    duration = float(duration)
    framerate = int(framerate)
    amplitude = float(amplitude)
    nchannels = int(nchannels)
    nframes = int(duration * framerate)

    tau = 2.0 * math.pi
    mono = [
        _clamp(amplitude * math.sin(tau * freq * i / framerate))
        for i in range(nframes)
    ]
    channels = [mono[:] for _ in range(nchannels)]
    return _new_audio(channels, framerate, 2, nchannels)


def audio_square_wave(freq, duration, framerate=44100, amplitude=1.0, nchannels=1):
    """Generate a square wave.

    Args:
        freq: Frequency in Hz.
        duration: Duration in seconds.
        framerate: Sample rate (default 44100).
        amplitude: Peak amplitude (default 1.0).
        nchannels: Number of output channels (default 1).

    Returns:
        Integer audio ID (sampwidth = 2).
    """
    freq = float(freq)
    duration = float(duration)
    framerate = int(framerate)
    amplitude = float(amplitude)
    nchannels = int(nchannels)
    nframes = int(duration * framerate)
    period = framerate / freq if freq > 0 else float(nframes)

    mono = [
        amplitude if (i % period) < (period / 2) else -amplitude
        for i in range(nframes)
    ]
    channels = [mono[:] for _ in range(nchannels)]
    return _new_audio(channels, framerate, 2, nchannels)


def audio_sawtooth_wave(freq, duration, framerate=44100, amplitude=1.0, nchannels=1):
    """Generate a sawtooth wave (linear ramp per period).

    Args:
        freq: Frequency in Hz.
        duration: Duration in seconds.
        framerate: Sample rate (default 44100).
        amplitude: Peak amplitude (default 1.0).
        nchannels: Number of output channels (default 1).

    Returns:
        Integer audio ID (sampwidth = 2).
    """
    freq = float(freq)
    duration = float(duration)
    framerate = int(framerate)
    amplitude = float(amplitude)
    nchannels = int(nchannels)
    nframes = int(duration * framerate)
    period = framerate / freq if freq > 0 else float(nframes)

    mono = [
        amplitude * (2.0 * ((i % period) / period) - 1.0)
        for i in range(nframes)
    ]
    channels = [mono[:] for _ in range(nchannels)]
    return _new_audio(channels, framerate, 2, nchannels)


def audio_noise(duration, framerate=44100, amplitude=1.0, nchannels=1):
    """Generate white noise (uniform distribution over [-amplitude, amplitude]).

    Uses a fixed seed for reproducibility in tests.

    Args:
        duration: Duration in seconds.
        framerate: Sample rate (default 44100).
        amplitude: Peak amplitude (default 1.0).
        nchannels: Number of output channels (default 1).

    Returns:
        Integer audio ID (sampwidth = 2).
    """
    duration = float(duration)
    framerate = int(framerate)
    amplitude = float(amplitude)
    nchannels = int(nchannels)
    nframes = int(duration * framerate)

    rng = random.Random(42)
    mono = [_clamp(amplitude * (rng.random() * 2.0 - 1.0)) for _ in range(nframes)]
    channels = [mono[:] for _ in range(nchannels)]
    return _new_audio(channels, framerate, 2, nchannels)


def audio_silence(duration, framerate=44100, nchannels=1):
    """Generate a silent (all-zero) audio object.

    Args:
        duration: Duration in seconds.
        framerate: Sample rate (default 44100).
        nchannels: Number of output channels (default 1).

    Returns:
        Integer audio ID (sampwidth = 2).
    """
    duration = float(duration)
    framerate = int(framerate)
    nchannels = int(nchannels)
    nframes = int(duration * framerate)

    channels = [[0.0] * nframes for _ in range(nchannels)]
    return _new_audio(channels, framerate, 2, nchannels)


# ===========================================================================
# Analysis
# ===========================================================================

def audio_peak(audio_id):
    """Return the peak absolute sample value across all channels.

    Args:
        audio_id: Integer audio ID.

    Returns:
        Float in the range 0.0 .. 1.0.
    """
    audio = _get_audio(audio_id)
    return max(
        (abs(s) for ch in range(audio["nchannels"]) for s in audio["samples"][ch]),
        default=0.0,
    )


def audio_rms(audio_id, channel=None):
    """Return the root-mean-square (RMS) amplitude.

    Args:
        audio_id: Integer audio ID.
        channel: If given, compute RMS for that channel only.  If None
                 (default), compute RMS across all channels combined.

    Returns:
        Float RMS value in the range 0.0 .. 1.0.
    """
    audio = _get_audio(audio_id)

    if channel is not None:
        ch = int(channel)
        samples = audio["samples"][ch]
        if not samples:
            return 0.0
        return math.sqrt(sum(s * s for s in samples) / len(samples))

    all_samples = [s for ch in range(audio["nchannels"]) for s in audio["samples"][ch]]
    if not all_samples:
        return 0.0
    return math.sqrt(sum(s * s for s in all_samples) / len(all_samples))


def audio_to_db(amplitude):
    """Convert a linear amplitude to decibels (20 * log10(amplitude)).

    Args:
        amplitude: Non-negative float.  0.0 returns -inf.

    Returns:
        Float dB value.
    """
    amp = float(amplitude)
    if amp <= 0.0:
        return float("-inf")
    return 20.0 * math.log10(amp)


def audio_from_db(db):
    """Convert a dB value back to linear amplitude (10 ** (db / 20)).

    Args:
        db: Float dB value.  Negative infinity returns 0.0.

    Returns:
        Float linear amplitude.
    """
    d = float(db)
    if d == float("-inf"):
        return 0.0
    return 10.0 ** (d / 20.0)


def audio_zero_crossings(audio_id, channel=0):
    """Count the number of zero crossing events in a channel.

    A zero crossing is counted whenever consecutive samples differ in sign.

    Args:
        audio_id: Integer audio ID.
        channel: Zero-based channel index (default 0).

    Returns:
        Integer count of zero crossings.
    """
    audio = _get_audio(audio_id)
    ch = int(channel)
    if ch >= audio["nchannels"]:
        ch = 0
    samples = audio["samples"][ch]
    count = 0
    for i in range(1, len(samples)):
        if (samples[i - 1] >= 0.0) != (samples[i] >= 0.0):
            count += 1
    return count


# ===========================================================================
# Format utilities
# ===========================================================================

def audio_detect_format(filename):
    """Detect an audio file's format by inspecting its header bytes.

    Args:
        filename: Path to the audio file.

    Returns:
        Lowercase format string: 'wav', 'mp3', 'flac', 'ogg', 'm4a', or
        'unknown'.
    """
    try:
        with open(str(filename), "rb") as f:
            header = f.read(12)
    except (OSError, IOError) as exc:
        raise RuntimeError(f"Cannot read file: {exc}") from exc

    if len(header) >= 12 and header[:4] == b"RIFF" and header[8:12] == b"WAVE":
        return "wav"
    if len(header) >= 3 and (
        header[:3] == b"ID3" or (header[:2] in (b"\xff\xfb", b"\xff\xf3", b"\xff\xf2"))
    ):
        return "mp3"
    if len(header) >= 4 and header[:4] == b"fLaC":
        return "flac"
    if len(header) >= 4 and header[:4] == b"OggS":
        return "ogg"
    if len(header) >= 8 and header[4:8] == b"ftyp":
        return "m4a"
    return "unknown"


def audio_to_bytes(audio_id, sampwidth=None):
    """Serialize an audio object to raw interleaved PCM bytes.

    Args:
        audio_id: Integer audio ID.
        sampwidth: Bytes per sample (default: audio's own sampwidth).

    Returns:
        bytes object containing the raw interleaved PCM data.
    """
    audio = _get_audio(audio_id)
    sw = int(sampwidth) if sampwidth is not None else audio["sampwidth"]
    nframes = _nframes(audio)
    return _samples_to_bytes(audio["samples"], sw, nframes)


def audio_from_bytes(data, framerate=44100, nchannels=1, sampwidth=2):
    """Create an audio object from raw interleaved PCM bytes.

    Args:
        data: bytes (or list of ints) containing little-endian PCM samples.
        framerate: Sample rate in Hz (default 44100).
        nchannels: Number of interleaved channels (default 1).
        sampwidth: Bytes per sample (default 2 / 16-bit).

    Returns:
        Integer audio ID.
    """
    framerate = int(framerate)
    nchannels = int(nchannels)
    sampwidth = int(sampwidth)

    if isinstance(data, (list, tuple)):
        data = bytes(int(b) for b in data)

    frame_size = sampwidth * nchannels
    if len(data) % frame_size != 0:
        raise RuntimeError(
            f"Data length {len(data)} is not a multiple of "
            f"frame size {frame_size} (sampwidth={sampwidth}, nchannels={nchannels})"
        )
    nframes = len(data) // frame_size
    channels = _bytes_to_samples(data, sampwidth, nframes, nchannels)
    return _new_audio(channels, framerate, sampwidth, nchannels)


# ===========================================================================
# Registration
# ===========================================================================

def register_audio_functions(runtime: Runtime) -> None:
    """Register all audio_* functions with the NexusLang runtime."""

    # WAV I/O & lifecycle
    runtime.register_function("audio_load_wav", audio_load_wav)
    runtime.register_function("audio_save_wav", audio_save_wav)
    runtime.register_function("audio_create", audio_create)
    runtime.register_function("audio_free", audio_free)
    runtime.register_function("audio_info", audio_info)

    # Accessors
    runtime.register_function("audio_duration", audio_duration)
    runtime.register_function("audio_sample_rate", audio_sample_rate)
    runtime.register_function("audio_channels", audio_channels)
    runtime.register_function("audio_get_samples", audio_get_samples)
    runtime.register_function("audio_set_samples", audio_set_samples)
    runtime.register_function("audio_from_samples", audio_from_samples)

    # Operations
    runtime.register_function("audio_concat", audio_concat)
    runtime.register_function("audio_slice", audio_slice)
    runtime.register_function("audio_mix", audio_mix)
    runtime.register_function("audio_reverse", audio_reverse)
    runtime.register_function("audio_normalize", audio_normalize)
    runtime.register_function("audio_apply_gain", audio_apply_gain)
    runtime.register_function("audio_fade", audio_fade)
    runtime.register_function("audio_trim_silence", audio_trim_silence)
    runtime.register_function("audio_resample", audio_resample)

    # Channel operations
    runtime.register_function("audio_mono_to_stereo", audio_mono_to_stereo)
    runtime.register_function("audio_stereo_to_mono", audio_stereo_to_mono)

    # Generation
    runtime.register_function("audio_sine_wave", audio_sine_wave)
    runtime.register_function("audio_square_wave", audio_square_wave)
    runtime.register_function("audio_sawtooth_wave", audio_sawtooth_wave)
    runtime.register_function("audio_noise", audio_noise)
    runtime.register_function("audio_silence", audio_silence)

    # Analysis
    runtime.register_function("audio_peak", audio_peak)
    runtime.register_function("audio_rms", audio_rms)
    runtime.register_function("audio_to_db", audio_to_db)
    runtime.register_function("audio_from_db", audio_from_db)
    runtime.register_function("audio_zero_crossings", audio_zero_crossings)

    # Format utilities
    runtime.register_function("audio_detect_format", audio_detect_format)
    runtime.register_function("audio_to_bytes", audio_to_bytes)
    runtime.register_function("audio_from_bytes", audio_from_bytes)
