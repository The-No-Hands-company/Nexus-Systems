"""Tests for the audio stdlib module.

Covers all 35 registered functions:
  WAV I/O & lifecycle : audio_load_wav, audio_save_wav, audio_create,
                        audio_free, audio_info
  Accessors           : audio_duration, audio_sample_rate, audio_channels,
                        audio_get_samples, audio_set_samples, audio_from_samples
  Operations          : audio_concat, audio_slice, audio_mix, audio_reverse,
                        audio_normalize, audio_apply_gain, audio_fade,
                        audio_trim_silence, audio_resample
  Channel ops         : audio_mono_to_stereo, audio_stereo_to_mono
  Generation          : audio_sine_wave, audio_square_wave, audio_sawtooth_wave,
                        audio_noise, audio_silence
  Analysis            : audio_peak, audio_rms, audio_to_db, audio_from_db,
                        audio_zero_crossings
  Format utilities    : audio_detect_format, audio_to_bytes, audio_from_bytes
"""

import importlib.util
import math
import os
import struct
import sys
import tempfile
import types

import pytest

# ---------------------------------------------------------------------------
# Fixture: import the audio module directly (avoids full stdlib import chain)
# ---------------------------------------------------------------------------

_HERE = os.path.dirname(os.path.abspath(__file__))
_AUDIO_INIT = os.path.join(
    _HERE, "..", "..", "..", "src", "nlpl", "stdlib", "audio", "__init__.py"
)


@pytest.fixture(scope="module")
def audio():
    """Return the audio stdlib module, loaded in isolation."""
    _pkgs = (
        "nlpl", "nexuslang.runtime", "nexuslang.runtime.runtime",
        "nexuslang.stdlib", "nexuslang.stdlib.audio",
    )
    # Save originals so we can restore after tests
    _originals = {pkg: sys.modules.get(pkg) for pkg in _pkgs}
    _had_runtime_cls = hasattr(sys.modules.get("nexuslang.runtime.runtime", object()), "Runtime")
    _orig_runtime_cls = getattr(sys.modules.get("nexuslang.runtime.runtime"), "Runtime", None) if _had_runtime_cls else None

    class _StubRuntime:
        def register_function(self, name, fn):
            pass
        def register_module(self, name):
            pass

    for pkg in _pkgs:
        if pkg not in sys.modules:
            sys.modules[pkg] = types.ModuleType(pkg)

    sys.modules["nexuslang.runtime.runtime"].Runtime = _StubRuntime

    spec = importlib.util.spec_from_file_location("nexuslang.stdlib.audio", _AUDIO_INIT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    yield mod

    # Restore original sys.modules state
    for pkg in _pkgs:
        if _originals[pkg] is None:
            sys.modules.pop(pkg, None)
        else:
            sys.modules[pkg] = _originals[pkg]
    if _had_runtime_cls and "nexuslang.runtime.runtime" in sys.modules:
        sys.modules["nexuslang.runtime.runtime"].Runtime = _orig_runtime_cls


# ---------------------------------------------------------------------------
# Helpers shared across tests
# ---------------------------------------------------------------------------

def approx_eq(a, b, tol=1e-5):
    return abs(float(a) - float(b)) < tol


# ===========================================================================
# Generation: audio_silence
# ===========================================================================


class TestAudioSilence:
    def test_duration(self, audio):
        sid = audio.audio_silence(1.0, 44100, 1)
        assert approx_eq(audio.audio_duration(sid), 1.0)

    def test_peak_is_zero(self, audio):
        sid = audio.audio_silence(0.5, 44100, 1)
        assert audio.audio_peak(sid) == 0.0

    def test_nchannels(self, audio):
        sid = audio.audio_silence(0.5, 44100, 2)
        assert audio.audio_channels(sid) == 2

    def test_nframes(self, audio):
        sid = audio.audio_silence(1.0, 8000, 1)
        s = audio.audio_get_samples(sid, 0)
        assert len(s) == 8000

    def test_all_samples_zero(self, audio):
        sid = audio.audio_silence(0.1, 8000, 1)
        assert all(v == 0.0 for v in audio.audio_get_samples(sid, 0))

    def test_default_framerate(self, audio):
        sid = audio.audio_silence(0.1)
        assert audio.audio_sample_rate(sid) == 44100


# ===========================================================================
# Generation: audio_sine_wave
# ===========================================================================


class TestAudioSineWave:
    def test_sample_rate(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)
        assert audio.audio_sample_rate(tid) == 44100

    def test_channels(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5, 1)
        assert audio.audio_channels(tid) == 1

    def test_peak_approx_amplitude(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)
        assert approx_eq(audio.audio_peak(tid), 0.5, tol=0.002)

    def test_rms_approx(self, audio):
        # RMS of a full-period sine with amplitude A is A / sqrt(2)
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)
        expected_rms = 0.5 / math.sqrt(2)
        assert approx_eq(audio.audio_rms(tid), expected_rms, tol=0.005)

    def test_nframes(self, audio):
        tid = audio.audio_sine_wave(440.0, 2.0, 44100, 1.0)
        s = audio.audio_get_samples(tid, 0)
        assert len(s) == 2 * 44100

    def test_sampwidth_16bit(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1)
        assert audio.audio_info(tid)["bit_depth"] == 16

    def test_stereo(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5, 2)
        assert audio.audio_channels(tid) == 2
        # Both channels should contain identical samples
        left = audio.audio_get_samples(tid, 0)
        right = audio.audio_get_samples(tid, 1)
        assert left == right


# ===========================================================================
# Generation: audio_square_wave
# ===========================================================================


class TestAudioSquareWave:
    def test_peak(self, audio):
        sid = audio.audio_square_wave(220.0, 0.1, 44100, 0.8)
        assert approx_eq(audio.audio_peak(sid), 0.8, tol=0.001)

    def test_duration(self, audio):
        sid = audio.audio_square_wave(220.0, 0.5, 44100, 1.0)
        assert approx_eq(audio.audio_duration(sid), 0.5, tol=0.002)

    def test_channels(self, audio):
        sid = audio.audio_square_wave(220.0, 0.1, 44100, 1.0, 1)
        assert audio.audio_channels(sid) == 1

    def test_nframes(self, audio):
        sid = audio.audio_square_wave(220.0, 0.1, 8000, 1.0)
        assert len(audio.audio_get_samples(sid, 0)) == 800

    def test_only_two_values(self, audio):
        sid = audio.audio_square_wave(220.0, 0.1, 44100, 1.0)
        vals = set(audio.audio_get_samples(sid, 0))
        assert len(vals) == 2


# ===========================================================================
# Generation: audio_sawtooth_wave
# ===========================================================================


class TestAudioSawtoothWave:
    def test_has_frames(self, audio):
        sid = audio.audio_sawtooth_wave(110.0, 0.1, 44100, 0.6)
        assert len(audio.audio_get_samples(sid, 0)) > 0

    def test_peak_approx_amplitude(self, audio):
        sid = audio.audio_sawtooth_wave(110.0, 0.5, 44100, 0.6)
        assert approx_eq(audio.audio_peak(sid), 0.6, tol=0.01)

    def test_channels(self, audio):
        sid = audio.audio_sawtooth_wave(110.0, 0.1, 44100, 0.5, 2)
        assert audio.audio_channels(sid) == 2

    def test_duration(self, audio):
        sid = audio.audio_sawtooth_wave(110.0, 0.3, 44100, 1.0)
        assert approx_eq(audio.audio_duration(sid), 0.3, tol=0.002)


# ===========================================================================
# Generation: audio_noise
# ===========================================================================


class TestAudioNoise:
    def test_rms_nonzero(self, audio):
        nid = audio.audio_noise(0.5, 44100, 1.0, 1)
        assert audio.audio_rms(nid) > 0.1

    def test_peak_le_amplitude(self, audio):
        nid = audio.audio_noise(0.5, 44100, 0.5, 1)
        assert audio.audio_peak(nid) <= 0.5 + 1e-9

    def test_channels(self, audio):
        nid = audio.audio_noise(0.1, 8000, 1.0, 2)
        assert audio.audio_channels(nid) == 2

    def test_duration(self, audio):
        nid = audio.audio_noise(1.0, 44100, 1.0)
        assert approx_eq(audio.audio_duration(nid), 1.0)

    def test_deterministic(self, audio):
        # Two calls with same args should produce identical output (seed=42)
        n1 = audio.audio_noise(0.1, 8000, 1.0)
        n2 = audio.audio_noise(0.1, 8000, 1.0)
        s1 = audio.audio_get_samples(n1, 0)
        s2 = audio.audio_get_samples(n2, 0)
        assert s1 == s2


# ===========================================================================
# WAV I/O: audio_save_wav / audio_load_wav
# ===========================================================================


class TestAudioSaveLoadWav:
    def _make_tone(self, audio, freq=440.0, dur=0.1, rate=44100, amp=0.5):
        return audio.audio_sine_wave(freq, dur, rate, amp, 1)

    def test_save_creates_file(self, audio, tmp_path):
        tid = self._make_tone(audio)
        p = str(tmp_path / "out.wav")
        audio.audio_save_wav(tid, p)
        assert os.path.isfile(p)

    def test_roundtrip_16bit_framerate(self, audio, tmp_path):
        tid = self._make_tone(audio)
        p = str(tmp_path / "t16.wav")
        audio.audio_save_wav(tid, p)
        loaded = audio.audio_load_wav(p)
        assert audio.audio_sample_rate(loaded) == 44100

    def test_roundtrip_16bit_channels(self, audio, tmp_path):
        tid = self._make_tone(audio)
        p = str(tmp_path / "t16c.wav")
        audio.audio_save_wav(tid, p)
        loaded = audio.audio_load_wav(p)
        assert audio.audio_channels(loaded) == 1

    def test_roundtrip_16bit_peak(self, audio, tmp_path):
        tid = self._make_tone(audio)
        p = str(tmp_path / "t16p.wav")
        audio.audio_save_wav(tid, p)
        loaded = audio.audio_load_wav(p)
        assert approx_eq(audio.audio_peak(loaded), 0.5, tol=0.002)

    def test_roundtrip_8bit(self, audio, tmp_path):
        tid = self._make_tone(audio)
        p = str(tmp_path / "t8.wav")
        audio.audio_save_wav(tid, p, sampwidth=1)
        loaded = audio.audio_load_wav(p)
        assert audio.audio_info(loaded)["sampwidth"] == 1
        assert approx_eq(audio.audio_peak(loaded), 0.5, tol=0.02)

    def test_roundtrip_24bit(self, audio, tmp_path):
        tid = self._make_tone(audio)
        p = str(tmp_path / "t24.wav")
        audio.audio_save_wav(tid, p, sampwidth=3)
        loaded = audio.audio_load_wav(p)
        assert audio.audio_info(loaded)["sampwidth"] == 3
        assert approx_eq(audio.audio_peak(loaded), 0.5, tol=0.001)

    def test_roundtrip_stereo(self, audio, tmp_path):
        tid = self._make_tone(audio)
        st = audio.audio_mono_to_stereo(tid)
        p = str(tmp_path / "stereo.wav")
        audio.audio_save_wav(st, p)
        loaded = audio.audio_load_wav(p)
        assert audio.audio_channels(loaded) == 2

    def test_invalid_sampwidth_raises(self, audio, tmp_path):
        tid = self._make_tone(audio)
        with pytest.raises(RuntimeError):
            audio.audio_save_wav(tid, str(tmp_path / "bad.wav"), sampwidth=5)


# ===========================================================================
# audio_info
# ===========================================================================


class TestAudioInfo:
    def test_keys_present(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1)
        info = audio.audio_info(tid)
        for key in ("channels", "framerate", "sampwidth", "frames", "duration", "bit_depth"):
            assert key in info

    def test_channels_value(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5, 1)
        assert audio.audio_info(tid)["channels"] == 1

    def test_framerate_value(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 22050)
        assert audio.audio_info(tid)["framerate"] == 22050

    def test_bit_depth_16(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1)
        assert audio.audio_info(tid)["bit_depth"] == 16

    def test_duration_approx(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.5)
        assert approx_eq(audio.audio_info(tid)["duration"], 0.5, tol=0.002)


# ===========================================================================
# audio_create
# ===========================================================================


class TestAudioCreate:
    def test_empty(self, audio):
        cid = audio.audio_create(44100, 1, 2)
        assert len(audio.audio_get_samples(cid, 0)) == 0

    def test_framerate(self, audio):
        cid = audio.audio_create(22050, 1, 2)
        assert audio.audio_sample_rate(cid) == 22050

    def test_channels(self, audio):
        cid = audio.audio_create(44100, 4, 2)
        assert audio.audio_channels(cid) == 4

    def test_sampwidth(self, audio):
        cid = audio.audio_create(44100, 1, 4)
        assert audio.audio_info(cid)["sampwidth"] == 4


# ===========================================================================
# audio_free
# ===========================================================================


class TestAudioFree:
    def test_removes_from_store(self, audio):
        fid = audio.audio_silence(0.1)
        audio.audio_free(fid)
        assert fid not in audio._audios

    def test_double_free_safe(self, audio):
        fid = audio.audio_silence(0.1)
        audio.audio_free(fid)
        audio.audio_free(fid)  # must not raise

    def test_invalid_id_raises_on_use(self, audio):
        with pytest.raises(RuntimeError):
            audio.audio_get_samples(9999999, 0)


# ===========================================================================
# audio_get_samples / audio_set_samples
# ===========================================================================


class TestAudioGetSetSamples:
    def test_returns_list(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1)
        assert isinstance(audio.audio_get_samples(tid, 0), list)

    def test_correct_length(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 8000)
        assert len(audio.audio_get_samples(tid, 0)) == 8000

    def test_channel_out_of_range_raises(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5, 1)
        with pytest.raises(RuntimeError):
            audio.audio_get_samples(tid, 5)

    def test_set_replaces(self, audio):
        cid = audio.audio_create(8000, 1, 2)
        audio.audio_set_samples(cid, 0, [0.1, 0.2, 0.3])
        s = audio.audio_get_samples(cid, 0)
        assert len(s) == 3
        assert approx_eq(s[2], 0.3)

    def test_set_converts_to_float(self, audio):
        cid = audio.audio_create(8000, 1, 2)
        audio.audio_set_samples(cid, 0, [1, 0, -1])
        s = audio.audio_get_samples(cid, 0)
        assert all(isinstance(v, float) for v in s)

    def test_set_channel_out_of_range_raises(self, audio):
        cid = audio.audio_create(8000, 1, 2)
        with pytest.raises(RuntimeError):
            audio.audio_set_samples(cid, 5, [0.0])


# ===========================================================================
# audio_from_samples
# ===========================================================================


class TestAudioFromSamples:
    def test_flat_mono(self, audio):
        fid = audio.audio_from_samples([0.1, 0.2, 0.3], 8000, 2)
        assert audio.audio_channels(fid) == 1
        assert len(audio.audio_get_samples(fid, 0)) == 3

    def test_multi_channel(self, audio):
        fid = audio.audio_from_samples([[0.1, 0.2], [0.3, 0.4]], 8000, 2)
        assert audio.audio_channels(fid) == 2

    def test_framerate_stored(self, audio):
        fid = audio.audio_from_samples([0.0] * 10, 22050, 2)
        assert audio.audio_sample_rate(fid) == 22050

    def test_sampwidth_stored(self, audio):
        fid = audio.audio_from_samples([0.0] * 10, 44100, 4)
        assert audio.audio_info(fid)["sampwidth"] == 4

    def test_flat_nchannels_override(self, audio):
        fid = audio.audio_from_samples([0.1, 0.2, 0.3], 8000, 2, nchannels=2)
        # Both channels get the same flat list when mono input is duplicated
        assert audio.audio_channels(fid) == 2


# ===========================================================================
# audio_concat
# ===========================================================================


class TestAudioConcat:
    def test_duration_doubled(self, audio):
        a = audio.audio_silence(0.5, 44100, 1)
        b = audio.audio_silence(0.5, 44100, 1)
        c = audio.audio_concat(a, b)
        assert approx_eq(audio.audio_duration(c), 1.0)

    def test_channel_mismatch_raises(self, audio):
        a = audio.audio_silence(0.1, 44100, 1)
        b = audio.audio_silence(0.1, 44100, 2)
        with pytest.raises(RuntimeError):
            audio.audio_concat(a, b)

    def test_framerate_mismatch_raises(self, audio):
        a = audio.audio_silence(0.1, 44100, 1)
        b = audio.audio_silence(0.1, 22050, 1)
        with pytest.raises(RuntimeError):
            audio.audio_concat(a, b)

    def test_sample_order(self, audio):
        # First half silence, second half silence with a tone should preserve order
        a = audio.audio_from_samples([1.0, 1.0], 8000, 2)
        b = audio.audio_from_samples([-1.0, -1.0], 8000, 2)
        c = audio.audio_concat(a, b)
        s = audio.audio_get_samples(c, 0)
        assert s[0] == 1.0
        assert s[-1] == -1.0

    def test_preserves_framerate(self, audio):
        a = audio.audio_silence(0.1, 22050, 1)
        b = audio.audio_silence(0.1, 22050, 1)
        c = audio.audio_concat(a, b)
        assert audio.audio_sample_rate(c) == 22050


# ===========================================================================
# audio_slice
# ===========================================================================


class TestAudioSlice:
    def _tone(self, audio):
        return audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)

    def test_half_duration(self, audio):
        tid = self._tone(audio)
        sl = audio.audio_slice(tid, 0.25, 0.75)
        assert approx_eq(audio.audio_duration(sl), 0.5, tol=0.002)

    def test_start_only(self, audio):
        tid = self._tone(audio)
        sl = audio.audio_slice(tid, 0.5)
        assert approx_eq(audio.audio_duration(sl), 0.5, tol=0.002)

    def test_full_slice(self, audio):
        tid = self._tone(audio)
        sl = audio.audio_slice(tid, 0.0, 1.0)
        assert approx_eq(audio.audio_duration(sl), 1.0, tol=0.002)

    def test_empty_slice(self, audio):
        tid = self._tone(audio)
        sl = audio.audio_slice(tid, 0.5, 0.5)
        assert len(audio.audio_get_samples(sl, 0)) == 0

    def test_preserves_framerate(self, audio):
        tid = self._tone(audio)
        sl = audio.audio_slice(tid, 0.0, 0.5)
        assert audio.audio_sample_rate(sl) == 44100


# ===========================================================================
# audio_mix
# ===========================================================================


class TestAudioMix:
    def test_silent_mix_is_silent(self, audio):
        a = audio.audio_silence(0.5, 44100, 1)
        b = audio.audio_silence(0.5, 44100, 1)
        mx = audio.audio_mix(a, b)
        assert audio.audio_peak(mx) == 0.0

    def test_gain_applied(self, audio):
        a = audio.audio_from_samples([1.0] * 100, 44100, 2)
        b = audio.audio_silence(0.5, 44100, 1)
        # b is too short -- but a and b share framerate=44100, nchannels=1
        b2 = audio.audio_from_samples([0.0] * 100, 44100, 2)
        mx = audio.audio_mix(a, b2, gain1=0.5, gain2=0.0)
        s = audio.audio_get_samples(mx, 0)
        assert approx_eq(s[0], 0.5)

    def test_zero_pad_shorter(self, audio):
        a = audio.audio_from_samples([0.5] * 200, 44100, 2)
        b = audio.audio_from_samples([0.5] * 100, 44100, 2)
        mx = audio.audio_mix(a, b, 1.0, 1.0)
        s = audio.audio_get_samples(mx, 0)
        assert len(s) == 200

    def test_clamp_on_overflow(self, audio):
        a = audio.audio_from_samples([1.0] * 10, 44100, 2)
        b = audio.audio_from_samples([1.0] * 10, 44100, 2)
        mx = audio.audio_mix(a, b, 1.0, 1.0)
        assert all(v <= 1.0 for v in audio.audio_get_samples(mx, 0))

    def test_channel_mismatch_raises(self, audio):
        a = audio.audio_silence(0.1, 44100, 1)
        b = audio.audio_silence(0.1, 44100, 2)
        with pytest.raises(RuntimeError):
            audio.audio_mix(a, b)

    def test_framerate_mismatch_raises(self, audio):
        a = audio.audio_silence(0.1, 44100, 1)
        b = audio.audio_silence(0.1, 22050, 1)
        with pytest.raises(RuntimeError):
            audio.audio_mix(a, b)


# ===========================================================================
# audio_reverse
# ===========================================================================


class TestAudioReverse:
    def test_last_becomes_first(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5)
        orig = audio.audio_get_samples(tid, 0)
        rev = audio.audio_reverse(tid)
        r = audio.audio_get_samples(rev, 0)
        assert approx_eq(orig[-1], r[0])

    def test_first_becomes_last(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5)
        orig = audio.audio_get_samples(tid, 0)
        rev = audio.audio_reverse(tid)
        r = audio.audio_get_samples(rev, 0)
        assert approx_eq(orig[0], r[-1])

    def test_length_preserved(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1)
        orig_len = len(audio.audio_get_samples(tid, 0))
        rev = audio.audio_reverse(tid)
        assert len(audio.audio_get_samples(rev, 0)) == orig_len

    def test_peak_preserved(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.5, 44100, 0.7)
        rev = audio.audio_reverse(tid)
        assert approx_eq(audio.audio_peak(rev), 0.7, tol=0.002)


# ===========================================================================
# audio_normalize
# ===========================================================================


class TestAudioNormalize:
    def test_peak_after_normalize(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.5, 44100, 0.3)
        norm = audio.audio_normalize(tid, 0.9)
        assert approx_eq(audio.audio_peak(norm), 0.9, tol=0.01)

    def test_silent_no_crash(self, audio):
        sid = audio.audio_silence(0.1, 44100, 1)
        result = audio.audio_normalize(sid, 0.9)
        assert audio.audio_peak(result) == 0.0

    def test_default_target_one(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.5, 44100, 0.4)
        norm = audio.audio_normalize(tid)
        assert approx_eq(audio.audio_peak(norm), 1.0, tol=0.01)

    def test_original_unchanged(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.5, 44100, 0.3)
        orig_peak = audio.audio_peak(tid)
        audio.audio_normalize(tid, 0.9)
        assert approx_eq(audio.audio_peak(tid), orig_peak)

    def test_scale_correct(self, audio):
        vals = [0.0, 0.25, 0.5, 0.25, 0.0, -0.25, -0.5, -0.25]
        fid = audio.audio_from_samples(vals, 8000, 2)
        norm = audio.audio_normalize(fid, 0.5)
        s = audio.audio_get_samples(norm, 0)
        assert approx_eq(max(abs(v) for v in s), 0.5, tol=1e-6)


# ===========================================================================
# audio_apply_gain
# ===========================================================================


class TestAudioApplyGain:
    def test_halves_peak(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.5, 44100, 0.5)
        g = audio.audio_apply_gain(tid, 0.5)
        assert approx_eq(audio.audio_peak(g), 0.25, tol=0.002)

    def test_zero_gain_is_silence(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1)
        g = audio.audio_apply_gain(tid, 0.0)
        assert audio.audio_peak(g) == 0.0

    def test_clamp_on_gain_gt_one(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.9)
        g = audio.audio_apply_gain(tid, 2.0)
        assert all(abs(v) <= 1.0 for v in audio.audio_get_samples(g, 0))

    def test_negative_gain_inverts(self, audio):
        fid = audio.audio_from_samples([0.5], 44100, 2)
        g = audio.audio_apply_gain(fid, -1.0)
        s = audio.audio_get_samples(g, 0)
        assert approx_eq(s[0], -0.5)


# ===========================================================================
# audio_fade
# ===========================================================================


class TestAudioFade:
    def test_fade_in_first_sample_near_zero(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 1.0)
        faded = audio.audio_fade(tid, fade_in=0.1)
        first = audio.audio_get_samples(faded, 0)[0]
        assert approx_eq(first, 0.0, tol=0.01)

    def test_fade_out_last_sample_near_zero(self, audio):
        fid = audio.audio_from_samples([1.0] * 4410, 44100, 2)
        faded = audio.audio_fade(fid, fade_out=0.1)
        last = audio.audio_get_samples(faded, 0)[-1]
        assert approx_eq(last, 0.0, tol=0.01)

    def test_no_fade_unchanged(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1)
        orig = audio.audio_get_samples(tid, 0)[:]
        faded = audio.audio_fade(tid, 0.0, 0.0)
        assert audio.audio_get_samples(faded, 0) == orig

    def test_combined_fade(self, audio):
        fid = audio.audio_from_samples([1.0] * 8820, 44100, 2)
        faded = audio.audio_fade(fid, fade_in=0.1, fade_out=0.1)
        s = audio.audio_get_samples(faded, 0)
        assert approx_eq(s[0], 0.0, tol=0.01)
        assert approx_eq(s[-1], 0.0, tol=0.01)

    def test_length_unchanged(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.5)
        n = len(audio.audio_get_samples(tid, 0))
        faded = audio.audio_fade(tid, 0.05, 0.05)
        assert len(audio.audio_get_samples(faded, 0)) == n


# ===========================================================================
# audio_trim_silence
# ===========================================================================


class TestAudioTrimSilence:
    def test_shorter_than_original(self, audio):
        padded = [0.0] * 100 + [0.5, 0.5] + [0.0] * 100
        pid = audio.audio_from_samples(padded, 44100, 2)
        trimmed = audio.audio_trim_silence(pid, threshold=0.01)
        assert len(audio.audio_get_samples(trimmed, 0)) < len(padded)

    def test_content_preserved(self, audio):
        padded = [0.0] * 50 + [0.5, 0.6, 0.5] + [0.0] * 50
        pid = audio.audio_from_samples(padded, 44100, 2)
        trimmed = audio.audio_trim_silence(pid, threshold=0.01)
        s = audio.audio_get_samples(trimmed, 0)
        assert approx_eq(s[0], 0.5, tol=0.001)

    def test_all_silence_not_crash(self, audio):
        sid = audio.audio_silence(0.1, 44100, 1)
        result = audio.audio_trim_silence(sid, threshold=0.0001)
        # Should return an empty or very short audio without raising
        assert audio.audio_peak(result) == 0.0

    def test_no_silence_unchanged_length(self, audio):
        # No leading/trailing silence beyond threshold
        vals = [0.5] * 100
        fid = audio.audio_from_samples(vals, 44100, 2)
        trimmed = audio.audio_trim_silence(fid, threshold=0.01)
        assert len(audio.audio_get_samples(trimmed, 0)) == 100


# ===========================================================================
# audio_resample
# ===========================================================================


class TestAudioResample:
    def test_new_sample_rate(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)
        r = audio.audio_resample(tid, 22050)
        assert audio.audio_sample_rate(r) == 22050

    def test_nframes_halved(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)
        r = audio.audio_resample(tid, 22050)
        n = len(audio.audio_get_samples(r, 0))
        assert abs(n - 22050) < 5

    def test_duration_preserved(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)
        r = audio.audio_resample(tid, 22050)
        assert approx_eq(audio.audio_duration(r), 1.0, tol=0.01)

    def test_same_rate_returns_copy(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5)
        r = audio.audio_resample(tid, 44100)
        assert audio.audio_sample_rate(r) == 44100
        assert len(audio.audio_get_samples(r, 0)) == len(audio.audio_get_samples(tid, 0))

    def test_peak_approximately_preserved(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)
        r = audio.audio_resample(tid, 22050)
        assert approx_eq(audio.audio_peak(r), 0.5, tol=0.02)


# ===========================================================================
# audio_mono_to_stereo
# ===========================================================================


class TestAudioMonoToStereo:
    def test_two_channels(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5, 1)
        st = audio.audio_mono_to_stereo(tid)
        assert audio.audio_channels(st) == 2

    def test_channels_equal(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5, 1)
        st = audio.audio_mono_to_stereo(tid)
        assert audio.audio_get_samples(st, 0)[:10] == audio.audio_get_samples(st, 1)[:10]

    def test_error_on_non_mono(self, audio):
        st = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5, 2)
        with pytest.raises(RuntimeError):
            audio.audio_mono_to_stereo(st)

    def test_peak_preserved(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.5, 44100, 0.7, 1)
        st = audio.audio_mono_to_stereo(tid)
        assert approx_eq(audio.audio_peak(st), 0.7, tol=0.002)


# ===========================================================================
# audio_stereo_to_mono
# ===========================================================================


class TestAudioStereoToMono:
    def test_one_channel(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5, 2)
        mono = audio.audio_stereo_to_mono(tid)
        assert audio.audio_channels(mono) == 1

    def test_error_on_mono_input(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5, 1)
        with pytest.raises(RuntimeError):
            audio.audio_stereo_to_mono(tid)

    def test_peak_approximately_preserved(self, audio):
        st = audio.audio_mono_to_stereo(audio.audio_sine_wave(440.0, 0.5, 44100, 0.5, 1))
        mono = audio.audio_stereo_to_mono(st)
        assert approx_eq(audio.audio_peak(mono), 0.5, tol=0.002)

    def test_average_of_equal_channels(self, audio):
        # Two channels with constant value 0.5 -> mono average 0.5
        a = audio.audio_from_samples([[0.5] * 10, [0.5] * 10], 44100, 2)
        mono = audio.audio_stereo_to_mono(a)
        s = audio.audio_get_samples(mono, 0)
        assert all(approx_eq(v, 0.5) for v in s)


# ===========================================================================
# audio_peak / audio_rms
# ===========================================================================


class TestAudioPeak:
    def test_silence_is_zero(self, audio):
        sid = audio.audio_silence(0.1)
        assert audio.audio_peak(sid) == 0.0

    def test_sine_approx_amplitude(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.7)
        assert approx_eq(audio.audio_peak(tid), 0.7, tol=0.002)

    def test_max_of_both_channels(self, audio):
        a = audio.audio_from_samples([[0.3] * 10, [0.8] * 10], 44100, 2)
        assert approx_eq(audio.audio_peak(a), 0.8)

    def test_negative_values(self, audio):
        fid = audio.audio_from_samples([-0.6, -0.4, 0.2], 44100, 2)
        assert approx_eq(audio.audio_peak(fid), 0.6)


class TestAudioRms:
    def test_silence_is_zero(self, audio):
        sid = audio.audio_silence(0.1)
        assert audio.audio_rms(sid) == 0.0

    def test_sine_approx_a_over_sqrt2(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)
        assert approx_eq(audio.audio_rms(tid), 0.5 / math.sqrt(2), tol=0.005)

    def test_channel_arg(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)
        rms_all = audio.audio_rms(tid)
        rms_ch0 = audio.audio_rms(tid, 0)
        assert approx_eq(rms_all, rms_ch0)

    def test_rms_greater_than_zero_for_noise(self, audio):
        nid = audio.audio_noise(0.5, 44100, 0.5)
        assert audio.audio_rms(nid) > 0.0

    def test_constant_signal(self, audio):
        # RMS of constant value v is v
        fid = audio.audio_from_samples([0.4] * 1000, 44100, 2)
        assert approx_eq(audio.audio_rms(fid), 0.4, tol=1e-6)


# ===========================================================================
# audio_to_db / audio_from_db
# ===========================================================================


class TestAudioToFromDb:
    def test_to_db_zero_is_minus_inf(self, audio):
        assert audio.audio_to_db(0.0) == float("-inf")

    def test_to_db_one_is_zero(self, audio):
        assert approx_eq(audio.audio_to_db(1.0), 0.0)

    def test_to_db_half(self, audio):
        assert approx_eq(audio.audio_to_db(0.5), -6.0206, tol=0.001)

    def test_from_db_zero_is_one(self, audio):
        assert approx_eq(audio.audio_from_db(0.0), 1.0)

    def test_from_db_minus_inf_is_zero(self, audio):
        assert audio.audio_from_db(float("-inf")) == 0.0

    def test_roundtrip(self, audio):
        for amp in (0.1, 0.5, 0.9, 1.0):
            db = audio.audio_to_db(amp)
            recovered = audio.audio_from_db(db)
            assert approx_eq(recovered, amp, tol=1e-9)


# ===========================================================================
# audio_zero_crossings
# ===========================================================================


class TestAudioZeroCrossings:
    def test_sine_440hz_in_one_second(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)
        zc = audio.audio_zero_crossings(tid, 0)
        # A 440 Hz sine has 880 zero crossings per second
        assert 800 < zc < 960

    def test_silence_no_crossings(self, audio):
        sid = audio.audio_silence(0.1)
        # Silence transitions from 0 to 0 -- no sign change
        assert audio.audio_zero_crossings(sid, 0) == 0

    def test_square_wave_crossings(self, audio):
        # 100 Hz square wave in 1 s -> ~200 crossings
        sid = audio.audio_square_wave(100.0, 1.0, 44100, 0.5)
        zc = audio.audio_zero_crossings(sid, 0)
        assert 150 < zc < 250

    def test_single_sample_no_crossing(self, audio):
        fid = audio.audio_from_samples([0.5], 44100, 2)
        assert audio.audio_zero_crossings(fid, 0) == 0

    def test_two_samples_opposite_sign(self, audio):
        fid = audio.audio_from_samples([0.5, -0.5], 44100, 2)
        assert audio.audio_zero_crossings(fid, 0) == 1


# ===========================================================================
# audio_detect_format
# ===========================================================================


class TestAudioDetectFormat:
    def test_wav_file(self, audio, tmp_path):
        tid = audio.audio_sine_wave(440.0, 0.1)
        p = str(tmp_path / "t.wav")
        audio.audio_save_wav(tid, p)
        assert audio.audio_detect_format(p) == "wav"

    def test_unknown_returns_unknown(self, audio, tmp_path):
        p = tmp_path / "t.bin"
        p.write_bytes(b"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b")
        assert audio.audio_detect_format(str(p)) == "unknown"

    def test_mp3_header(self, audio, tmp_path):
        p = tmp_path / "t.mp3"
        p.write_bytes(b"ID3" + b"\x00" * 20)
        assert audio.audio_detect_format(str(p)) == "mp3"

    def test_flac_header(self, audio, tmp_path):
        p = tmp_path / "t.flac"
        p.write_bytes(b"fLaC" + b"\x00" * 20)
        assert audio.audio_detect_format(str(p)) == "flac"

    def test_missing_file_raises(self, audio, tmp_path):
        with pytest.raises(RuntimeError):
            audio.audio_detect_format(str(tmp_path / "nonexistent.wav"))


# ===========================================================================
# audio_to_bytes / audio_from_bytes
# ===========================================================================


class TestAudioToFromBytes:
    def test_to_bytes_length_16bit(self, audio):
        tid = audio.audio_sine_wave(440.0, 1.0, 44100, 0.5)
        raw = audio.audio_to_bytes(tid, 2)
        assert len(raw) == 44100 * 2

    def test_roundtrip_16bit(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 44100, 0.5)
        raw = audio.audio_to_bytes(tid, 2)
        back = audio.audio_from_bytes(raw, 44100, 1, 2)
        assert approx_eq(audio.audio_peak(back), 0.5, tol=0.002)

    def test_from_bytes_framerate(self, audio):
        tid = audio.audio_silence(0.01, 22050)
        raw = audio.audio_to_bytes(tid, 2)
        back = audio.audio_from_bytes(raw, 22050, 1, 2)
        assert audio.audio_sample_rate(back) == 22050

    def test_from_bytes_stereo(self, audio):
        # Construct 10 stereo frames of 16-bit silence
        raw = struct.pack("<h", 0) * 20  # 10 frames * 2 channels
        bid = audio.audio_from_bytes(raw, 44100, 2, 2)
        assert audio.audio_channels(bid) == 2

    def test_invalid_length_raises(self, audio):
        # 3 bytes is not a multiple of frame size 2 for 16-bit mono
        with pytest.raises(RuntimeError):
            audio.audio_from_bytes(b"\x00\x01\x02", 44100, 1, 2)

    def test_to_bytes_sampwidth_override(self, audio):
        tid = audio.audio_sine_wave(440.0, 0.1, 8000, 0.5)
        raw8 = audio.audio_to_bytes(tid, 1)
        assert len(raw8) == 800  # 0.1s * 8000 Hz * 1 byte


# ===========================================================================
# Registration
# ===========================================================================


class TestRegistration:
    def test_register_count(self, audio):
        registered = []

        class _MockRuntime:
            def register_function(self, name, fn):
                registered.append(name)
            def register_module(self, name):
                pass

        audio.register_audio_functions(_MockRuntime())
        assert len(registered) == 35

    def test_all_names_start_with_audio_(self, audio):
        registered = []

        class _MockRuntime:
            def register_function(self, name, fn):
                registered.append(name)
            def register_module(self, name):
                pass

        audio.register_audio_functions(_MockRuntime())
        bad = [n for n in registered if not n.startswith("audio_")]
        assert bad == []

    def test_no_duplicate_registrations(self, audio):
        registered = []

        class _MockRuntime:
            def register_function(self, name, fn):
                registered.append(name)
            def register_module(self, name):
                pass

        audio.register_audio_functions(_MockRuntime())
        assert len(registered) == len(set(registered))
