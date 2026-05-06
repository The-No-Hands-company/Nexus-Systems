"""Smoke test for the audio stdlib module.

Loads the module directly (bypassing full stdlib import chain) and
exercises every public function.  Run with:

    python dev_tools/smoke_audio.py
"""

import importlib.util
import os
import sys
import tempfile
import types

# ---------------------------------------------------------------------------
# Load the module directly to avoid triggering the full stdlib import chain.
# ---------------------------------------------------------------------------

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "src", "nlpl"))
_MOD_PATH = os.path.join(_ROOT, "stdlib", "audio", "__init__.py")


class _StubRuntime:
    registered = []

    def register_function(self, name, fn):
        self.registered.append(name)

    def register_module(self, name):
        pass


# Inject stub packages so the relative import resolves
for pkg in ("nlpl", "nexuslang.runtime", "nexuslang.runtime.runtime",
            "nexuslang.stdlib", "nexuslang.stdlib.audio"):
    if pkg not in sys.modules:
        sys.modules[pkg] = types.ModuleType(pkg)

sys.modules["nexuslang.runtime.runtime"].Runtime = _StubRuntime

spec = importlib.util.spec_from_file_location("nexuslang.stdlib.audio", _MOD_PATH)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

PASS = 0
FAIL = 0


def check(label, condition, got=None, expected=None):
    global PASS, FAIL
    if condition:
        print(f"  PASS  {label}")
        PASS += 1
    else:
        msg = f"  FAIL  {label}"
        if got is not None or expected is not None:
            msg += f"  (got {got!r}, expected {expected!r})"
        print(msg)
        FAIL += 1


def approx(a, b, tol=1e-5):
    return abs(float(a) - float(b)) < tol


# ---------------------------------------------------------------------------
# Generation
# ---------------------------------------------------------------------------
print("=== audio smoke test ===\n")
print("--- Generation ---")

sil = m.audio_silence(1.0, 44100, 1)
check("silence: duration ~1.0", approx(m.audio_duration(sil), 1.0))
check("silence: peak == 0", m.audio_peak(sil) == 0.0)
check("silence: nchannels == 1", m.audio_channels(sil) == 1)

tone = m.audio_sine_wave(440.0, 1.0, 44100, 0.5, 1)
check("sine: sample_rate", m.audio_sample_rate(tone) == 44100)
check("sine: channels", m.audio_channels(tone) == 1)
check("sine: peak ~0.5", approx(m.audio_peak(tone), 0.5, tol=0.002))

sq = m.audio_square_wave(220.0, 0.1, 44100, 0.8, 1)
check("square: peak ~0.8", approx(m.audio_peak(sq), 0.8, tol=0.001))

saw = m.audio_sawtooth_wave(110.0, 0.1, 44100, 0.6, 1)
check("sawtooth: has frames", len(m.audio_get_samples(saw, 0)) > 0)

noise = m.audio_noise(0.5, 44100, 1.0, 1)
check("noise: rms > 0.01", m.audio_rms(noise) > 0.01)

# ---------------------------------------------------------------------------
# WAV I/O
# ---------------------------------------------------------------------------
print()
print("--- WAV I/O ---")

with tempfile.TemporaryDirectory() as tmpdir:
    wav16 = os.path.join(tmpdir, "test16.wav")
    m.audio_save_wav(tone, wav16)
    check("save: file exists", os.path.isfile(wav16))

    loaded = m.audio_load_wav(wav16)
    check("load: framerate", m.audio_sample_rate(loaded) == 44100)
    check("load: channels", m.audio_channels(loaded) == 1)
    nf = len(m.audio_get_samples(loaded, 0))
    check("load: nframes ~44100", abs(nf - 44100) < 2)
    check("load: peak ~0.5", approx(m.audio_peak(loaded), 0.5, tol=0.002))

    wav8 = os.path.join(tmpdir, "test8.wav")
    m.audio_save_wav(tone, wav8, sampwidth=1)
    r8 = m.audio_load_wav(wav8)
    info8 = m.audio_info(r8)
    check("8-bit: sampwidth", info8["sampwidth"] == 1)
    check("8-bit: bit_depth", info8["bit_depth"] == 8)
    check("8-bit: peak ~0.5", approx(m.audio_peak(r8), 0.5, tol=0.01))

    wav24 = os.path.join(tmpdir, "test24.wav")
    m.audio_save_wav(tone, wav24, sampwidth=3)
    r24 = m.audio_load_wav(wav24)
    check("24-bit: sampwidth", m.audio_info(r24)["sampwidth"] == 3)
    check("24-bit: peak ~0.5", approx(m.audio_peak(r24), 0.5, tol=0.001))

    stereo_id = m.audio_mono_to_stereo(tone)
    wav_st = os.path.join(tmpdir, "stereo.wav")
    m.audio_save_wav(stereo_id, wav_st)
    ls = m.audio_load_wav(wav_st)
    check("stereo save/load: channels", m.audio_channels(ls) == 2)

    check("detect_format: wav", m.audio_detect_format(wav16) == "wav")

# ---------------------------------------------------------------------------
# Accessors
# ---------------------------------------------------------------------------
print()
print("--- Accessors ---")

info = m.audio_info(tone)
check("info: channels", info["channels"] == 1)
check("info: framerate", info["framerate"] == 44100)
check("info: bit_depth", info["bit_depth"] == 16)
check("info: duration ~1.0", approx(info["duration"], 1.0))

samples = m.audio_get_samples(tone, 0)
check("get_samples: length", len(samples) == 44100)
check("get_samples: is list", isinstance(samples, list))

flat8 = [0.0, 0.5, 1.0, 0.5, 0.0, -0.5, -1.0, -0.5]
fs_id = m.audio_from_samples(flat8, 8000, 2)
check("from_samples: channels", m.audio_channels(fs_id) == 1)
check("from_samples: length", len(m.audio_get_samples(fs_id, 0)) == 8)

edit = m.audio_create(8000, 1, 2)
m.audio_set_samples(edit, 0, [0.1, 0.2, 0.3])
s = m.audio_get_samples(edit, 0)
check("set_samples: value", approx(s[1], 0.2))

# ---------------------------------------------------------------------------
# Operations
# ---------------------------------------------------------------------------
print()
print("--- Operations ---")

a = m.audio_silence(0.5, 44100, 1)
b = m.audio_silence(0.5, 44100, 1)
cat = m.audio_concat(a, b)
check("concat: duration ~1.0", approx(m.audio_duration(cat), 1.0))

sl = m.audio_slice(tone, 0.25, 0.75)
check("slice: duration ~0.5", approx(m.audio_duration(sl), 0.5, tol=0.002))

rev = m.audio_reverse(tone)
orig_s = m.audio_get_samples(tone, 0)
rev_s = m.audio_get_samples(rev, 0)
check("reverse: last==first-of-rev", approx(orig_s[-1], rev_s[0]))

quiet = m.audio_apply_gain(tone, 0.3)
norm = m.audio_normalize(quiet, 0.9)
check("normalize: peak ~0.9", approx(m.audio_peak(norm), 0.9, tol=0.01))

mix = m.audio_mix(a, b, 1.0, 1.0)
check("mix: silent result", m.audio_peak(mix) == 0.0)
check("mix: duration ~0.5", approx(m.audio_duration(mix), 0.5))

faded = m.audio_fade(tone, fade_in=0.05, fade_out=0.05)
first_s = m.audio_get_samples(faded, 0)[0]
check("fade: first sample near 0", approx(first_s, 0.0, tol=0.01))

padded = [0.0] * 100 + [0.5, 0.6, 0.5] + [0.0] * 100
pad_id = m.audio_from_samples(padded, 44100, 2)
trimmed = m.audio_trim_silence(pad_id, threshold=0.01)
trim_len = len(m.audio_get_samples(trimmed, 0))
check("trim_silence: shorter than original", trim_len < len(padded))

resampled = m.audio_resample(tone, 22050)
check("resample: new rate", m.audio_sample_rate(resampled) == 22050)
check("resample: duration preserved", approx(m.audio_duration(resampled), 1.0, tol=0.01))

# ---------------------------------------------------------------------------
# Channel operations
# ---------------------------------------------------------------------------
print()
print("--- Channel operations ---")

st2 = m.audio_mono_to_stereo(tone)
check("mono_to_stereo: channels", m.audio_channels(st2) == 2)
left = m.audio_get_samples(st2, 0)
right = m.audio_get_samples(st2, 1)
check("mono_to_stereo: L==R", left[:10] == right[:10])

mono_back = m.audio_stereo_to_mono(st2)
check("stereo_to_mono: channels", m.audio_channels(mono_back) == 1)
check("stereo_to_mono: peak preserved", approx(m.audio_peak(mono_back), 0.5, tol=0.002))

# ---------------------------------------------------------------------------
# Analysis
# ---------------------------------------------------------------------------
print()
print("--- Analysis ---")

check("peak: ~0.5", approx(m.audio_peak(tone), 0.5, tol=0.002))
rms_v = m.audio_rms(tone)
check("rms: sine RMS ~ A/sqrt(2)", approx(rms_v, 0.5 / 2 ** 0.5, tol=0.005))
check("rms: channel arg", approx(m.audio_rms(tone, 0), rms_v))

check("to_db: 1.0 == 0 dB", approx(m.audio_to_db(1.0), 0.0))
check("to_db: 0.5 ~ -6 dB", approx(m.audio_to_db(0.5), -6.0206, tol=0.001))
check("to_db: 0.0 == -inf", m.audio_to_db(0.0) == float("-inf"))

check("from_db: 0 dB == 1.0", approx(m.audio_from_db(0.0), 1.0))
check("from_db: -6 dB ~ 0.5", approx(m.audio_from_db(-6.0206), 0.5, tol=0.001))
check("from_db: -inf == 0.0", m.audio_from_db(float("-inf")) == 0.0)

zc = m.audio_zero_crossings(tone, 0)
check("zero_crossings: 440 Hz ~880 events", 800 < zc < 960)

# ---------------------------------------------------------------------------
# Format utilities
# ---------------------------------------------------------------------------
print()
print("--- Format utilities ---")

raw = m.audio_to_bytes(tone, 2)
check("to_bytes: length == nframes * 2", len(raw) == 44100 * 2)

back = m.audio_from_bytes(raw, 44100, 1, 2)
check("from_bytes: framerate", m.audio_sample_rate(back) == 44100)
check("from_bytes: channels", m.audio_channels(back) == 1)
check("from_bytes: peak ~0.5", approx(m.audio_peak(back), 0.5, tol=0.002))

# ---------------------------------------------------------------------------
# Lifecycle (audio_free)
# ---------------------------------------------------------------------------
print()
print("--- Lifecycle ---")

tmp_id = m.audio_silence(0.1)
m.audio_free(tmp_id)
check("free: audio removed", tmp_id not in m._audios)
m.audio_free(tmp_id)  # double-free must not raise
check("free: double-free is safe", True)

# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------
print()
print("--- Registration ---")

stub = _StubRuntime()
m.register_audio_functions(stub)
n = len(stub.registered)
check("registered 35 functions", n == 35, got=n, expected=35)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
print()
print(f"=== {PASS} passed, {FAIL} failed ===")
if FAIL:
    sys.exit(1)
