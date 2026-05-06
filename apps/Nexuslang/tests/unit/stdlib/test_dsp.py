"""
Tests for the NexusLang Digital Signal Processing (DSP) stdlib module
(src/nlpl/stdlib/dsp).

Coverage
--------
FFT/Spectral:    fft, ifft, rfft, irfft, fft_magnitude, fft_phase,
                 fft_power, fft_freqs
Windowing:       window_rectangular, window_hann, window_hamming,
                 window_blackman, window_bartlett, apply_window
Filtering:       convolve, fft_convolve, correlate, fir_filter,
                 moving_average
Analysis:        stft, dominant_frequency, spectral_centroid, snr,
                 zero_crossing_rate
Utilities:       fft_shift, next_power_of_two, db_to_linear,
                 linear_to_db, resample
Registration
"""

import math
import cmath
import pytest

from nexuslang.stdlib.dsp import (
    # FFT/Spectral
    fft,
    ifft,
    rfft,
    irfft,
    fft_magnitude,
    fft_phase,
    fft_power,
    fft_freqs,
    # Windowing
    window_rectangular,
    window_hann,
    window_hamming,
    window_blackman,
    window_bartlett,
    apply_window,
    # Filtering
    convolve,
    fft_convolve,
    correlate,
    fir_filter,
    moving_average,
    # Analysis
    stft,
    dominant_frequency,
    spectral_centroid,
    snr,
    zero_crossing_rate,
    # Utilities
    fft_shift,
    next_power_of_two,
    db_to_linear,
    linear_to_db,
    resample,
    # Registration
    register_dsp_functions,
)

# ---------------------------------------------------------------------------
# Shared fixtures
# ---------------------------------------------------------------------------

def _sine(freq_hz: float, sample_rate: float, n: int) -> list:
    """Generate n samples of a pure sine wave at freq_hz."""
    return [math.sin(2 * math.pi * freq_hz * k / sample_rate) for k in range(n)]


def _make_signal_8() -> list:
    """8-sample sine: sin(2*pi*k/8), frequency = 1 cycle / 8 samples."""
    return [math.sin(2 * math.pi * k / 8) for k in range(8)]


RTOL = 1e-12   # tight tolerance for exact round-trips
LOOSE = 1e-4   # tolerance for windowed / approximate results


# ===========================================================================
# FFT
# ===========================================================================

class TestFft:
    def test_round_trip_sine(self):
        sig = _make_signal_8()
        rec = ifft(fft(sig))
        assert all(abs(rec[k].real - sig[k]) < RTOL for k in range(8))

    def test_length_zero_padded(self):
        # Input length 5 -> padded to 8
        X = fft([1, 0, 0, 0, 0])
        assert len(X) == 8

    def test_dc_component(self):
        # DC signal: fft should have all energy at bin 0
        sig = [1.0] * 8
        X = fft(sig)
        assert abs(X[0] - 8.0) < RTOL
        for k in range(1, 8):
            assert abs(X[k]) < RTOL

    def test_peak_bin_matches_frequency(self):
        # 8-sample signal at 1 cycle -> peak at bin 1
        sig = _make_signal_8()
        X = fft(sig)
        mags = [abs(v) for v in X]
        assert mags.index(max(mags)) == 1

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            fft([])

    def test_returns_complex_list(self):
        X = fft([1, 2, 3, 4])
        assert all(isinstance(v, complex) for v in X)


# ===========================================================================
# IFFT
# ===========================================================================

class TestIfft:
    def test_round_trip_from_fft(self):
        sig = _make_signal_8()
        assert all(abs(ifft(fft(sig))[k].real - sig[k]) < RTOL for k in range(8))

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            ifft([])

    def test_non_power_of_two_raises(self):
        with pytest.raises(ValueError):
            ifft([1 + 0j] * 5)

    def test_impulse_response(self):
        # IFFT of [N, 0, 0, ...] == [1, 1, 1, ...]
        n = 8
        X = [complex(n)] + [0j] * (n - 1)
        x = ifft(X)
        assert all(abs(v.real - 1.0) < RTOL for v in x)

    def test_length_preserved(self):
        X = fft([1, 2, 3, 4])
        x = ifft(X)
        assert len(x) == len(X)


# ===========================================================================
# RFFT
# ===========================================================================

class TestRfft:
    def test_length_even(self):
        # 8-sample input -> 8//2+1 = 5 coefficients
        assert len(rfft(_make_signal_8())) == 5

    def test_length_padded(self):
        # 5 samples padded to 8 -> 5 rfft coefficients
        assert len(rfft([1, 0, 0, 0, 0])) == 5

    def test_peak_bin_real_sine(self):
        sig = _make_signal_8()
        rs = rfft(sig)
        mags = [abs(v) for v in rs]
        assert mags.index(max(mags)) == 1

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            rfft([])

    def test_dc_signal(self):
        X = rfft([1.0] * 8)
        # Bin 0 = 8, rest ~0
        assert abs(X[0] - 8.0) < RTOL
        for k in range(1, len(X)):
            assert abs(X[k]) < RTOL


# ===========================================================================
# IRFFT
# ===========================================================================

class TestIrfft:
    def test_round_trip(self):
        sig = _make_signal_8()
        rs = rfft(sig)
        rec = irfft(rs, len(sig))
        assert all(abs(rec[k] - sig[k]) < 1e-10 for k in range(len(sig)))

    def test_output_length_specified(self):
        rs = rfft([1.0] * 8)
        out = irfft(rs, 8)
        assert len(out) == 8

    def test_output_length_default(self):
        rs = rfft([1.0] * 8)
        out = irfft(rs)
        # default = 2 * (len(rs) - 1) = 8
        assert len(out) == 2 * (len(rs) - 1)

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            irfft([])

    def test_returns_floats(self):
        out = irfft(rfft([1, 2, 3, 4, 5, 6, 7, 8]), 8)
        assert all(isinstance(v, float) for v in out)


# ===========================================================================
# FFT Magnitude
# ===========================================================================

class TestFftMagnitude:
    def test_dc_signal(self):
        X = fft([2.0] * 8)
        mags = fft_magnitude(X)
        assert abs(mags[0] - 16.0) < RTOL

    def test_all_non_negative(self):
        X = fft(_make_signal_8())
        assert all(m >= 0 for m in fft_magnitude(X))

    def test_length_matches_spectrum(self):
        X = fft([1, 2, 3, 4])
        assert len(fft_magnitude(X)) == len(X)

    def test_pure_sine_peak(self):
        sig = _make_signal_8()
        X = fft(sig)
        mags = fft_magnitude(X)
        assert mags.index(max(mags)) == 1


# ===========================================================================
# FFT Phase
# ===========================================================================

class TestFftPhase:
    def test_length(self):
        X = fft(_make_signal_8())
        assert len(fft_phase(X)) == len(X)

    def test_range(self):
        X = fft(_make_signal_8())
        phases = fft_phase(X)
        assert all(-math.pi - 1e-9 <= p <= math.pi + 1e-9 for p in phases)

    def test_real_positive_dc_is_zero(self):
        X = [complex(4.0, 0.0)]
        assert abs(fft_phase(X)[0]) < RTOL


# ===========================================================================
# FFT Power
# ===========================================================================

class TestFftPower:
    def test_all_non_negative(self):
        X = fft(_make_signal_8())
        assert all(p >= 0 for p in fft_power(X))

    def test_equals_magnitude_squared(self):
        X = fft(_make_signal_8())
        pows = fft_power(X)
        mags = fft_magnitude(X)
        assert all(abs(pows[k] - mags[k] ** 2) < RTOL for k in range(len(X)))

    def test_dc_signal(self):
        X = fft([1.0] * 4)
        pows = fft_power(X)
        assert pows[0] == pytest.approx(16.0, abs=RTOL)


# ===========================================================================
# FFT Frequencies
# ===========================================================================

class TestFftFreqs:
    def test_dc_is_zero(self):
        freqs = fft_freqs(8, 8.0)
        assert freqs[0] == 0.0

    def test_positive_bins(self):
        freqs = fft_freqs(8, 8.0)
        assert abs(freqs[1] - 1.0) < RTOL
        assert abs(freqs[2] - 2.0) < RTOL

    def test_nyquist_negative_wrap(self):
        freqs = fft_freqs(8, 8.0)
        # bin 4 (n//2) should be negative or nyquist
        assert freqs[4] < 0 or abs(freqs[4] - 4.0) < RTOL

    def test_length(self):
        assert len(fft_freqs(16, 1.0)) == 16

    def test_invalid_n_raises(self):
        with pytest.raises(ValueError):
            fft_freqs(0)

    def test_default_sample_rate(self):
        freqs = fft_freqs(8)
        assert abs(freqs[1] - 1.0 / 8) < RTOL


# ===========================================================================
# Windowing
# ===========================================================================

class TestWindowRectangular:
    def test_all_ones(self):
        assert window_rectangular(4) == [1.0, 1.0, 1.0, 1.0]

    def test_length(self):
        assert len(window_rectangular(10)) == 10

    def test_length_one(self):
        assert window_rectangular(1) == [1.0]

    def test_invalid_raises(self):
        with pytest.raises(ValueError):
            window_rectangular(0)


class TestWindowHann:
    def test_endpoints_zero(self):
        w = window_hann(8)
        assert abs(w[0]) < RTOL
        assert abs(w[-1]) < RTOL

    def test_length(self):
        assert len(window_hann(16)) == 16

    def test_peak_near_center(self):
        w = window_hann(9)
        assert w[4] == max(w)

    def test_single_sample(self):
        assert window_hann(1) == [1.0]

    def test_invalid_raises(self):
        with pytest.raises(ValueError):
            window_hann(0)


class TestWindowHamming:
    def test_endpoints_near_008(self):
        w = window_hamming(8)
        assert abs(w[0] - 0.08) < 1e-9
        assert abs(w[-1] - 0.08) < 1e-9

    def test_length(self):
        assert len(window_hamming(12)) == 12

    def test_never_zero(self):
        w = window_hamming(8)
        assert all(v > 0 for v in w)

    def test_invalid_raises(self):
        with pytest.raises(ValueError):
            window_hamming(0)


class TestWindowBlackman:
    def test_length(self):
        assert len(window_blackman(8)) == 8

    def test_single_sample(self):
        assert window_blackman(1) == [1.0]

    def test_symmetric(self):
        w = window_blackman(8)
        for i in range(4):
            assert abs(w[i] - w[7 - i]) < 1e-12

    def test_invalid_raises(self):
        with pytest.raises(ValueError):
            window_blackman(0)


class TestWindowBartlett:
    def test_endpoints_zero(self):
        w = window_bartlett(8)
        assert abs(w[0]) < RTOL
        assert abs(w[-1]) < RTOL

    def test_length(self):
        assert len(window_bartlett(5)) == 5

    def test_peak_near_center(self):
        w = window_bartlett(9)
        assert w[4] == max(w)

    def test_invalid_raises(self):
        with pytest.raises(ValueError):
            window_bartlett(0)


class TestApplyWindow:
    def test_rectangular_identity(self):
        sig = [1.0, 2.0, 3.0, 4.0]
        out = apply_window(sig, [1.0, 1.0, 1.0, 1.0])
        assert out == sig

    def test_zero_window(self):
        out = apply_window([1, 2, 3, 4], [0, 0, 0, 0])
        assert all(v == 0.0 for v in out)

    def test_scales_signal(self):
        out = apply_window([2.0, 4.0], [0.5, 0.5])
        assert out == [1.0, 2.0]

    def test_length_mismatch_raises(self):
        with pytest.raises(ValueError):
            apply_window([1, 2, 3], [1, 1])

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            apply_window([], [])


# ===========================================================================
# Filtering
# ===========================================================================

class TestConvolve:
    def test_basic(self):
        assert convolve([1, 2, 3], [1, 1]) == [1.0, 3.0, 5.0, 3.0]

    def test_output_length(self):
        assert len(convolve([1, 2, 3, 4], [1, 0, -1])) == 6

    def test_impulse_response(self):
        h = [1.0, 0.5]
        assert convolve([1, 0, 0], h)[:2] == pytest.approx([1.0, 0.5], abs=RTOL)

    def test_commutative(self):
        a, b = [1, 2, 3], [4, 5]
        assert convolve(a, b) == pytest.approx(convolve(b, a), abs=RTOL)

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            convolve([], [1])
        with pytest.raises(ValueError):
            convolve([1], [])


class TestFftConvolve:
    def test_matches_direct_convolve(self):
        a = [1, 2, 3, 4, 5]
        b = [1, -1, 1]
        direct = convolve(a, b)
        fast = fft_convolve(a, b)
        assert all(abs(direct[k] - fast[k]) < 1e-10 for k in range(len(direct)))

    def test_longer_signals(self):
        a = list(range(1, 17))
        b = [0.5, 0.5]
        d = convolve(a, b)
        f = fft_convolve(a, b)
        assert all(abs(d[k] - f[k]) < 1e-10 for k in range(len(d)))

    def test_output_length(self):
        assert len(fft_convolve([1, 2, 3], [1, 1])) == 4

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            fft_convolve([], [1])


class TestCorrelate:
    def test_auto_correlation_dc(self):
        # autocorrelation of [1,1,1] at lag 0 should be max
        r = correlate([1, 1, 1], [1, 1, 1])
        assert r[len(r) // 2] == max(r)

    def test_identity_kernel(self):
        # correlate(a, [1,0]) = convolve(a, reversed([1,0])) = convolve(a, [0,1])
        # => [0,1,2,3] (delayed by one sample)
        r = correlate([1, 2, 3], [1, 0])
        assert r == [0.0, 1.0, 2.0, 3.0]

    def test_output_length(self):
        assert len(correlate([1, 2, 3], [1, 1])) == 4

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            correlate([], [1])


class TestFirFilter:
    def test_passthrough_impulse(self):
        out = fir_filter([1, 2, 3, 4, 5], [1.0])
        assert out == [1.0, 2.0, 3.0, 4.0, 5.0]

    def test_averaging_filter(self):
        out = fir_filter([1, 2, 3, 4, 5], [0.5, 0.5])
        assert out == pytest.approx([0.5, 1.5, 2.5, 3.5, 4.5], abs=RTOL)

    def test_output_length_equals_input(self):
        out = fir_filter([1, 2, 3, 4, 5, 6], [1, 0, -1])
        assert len(out) == 6

    def test_zero_filter(self):
        out = fir_filter([1, 2, 3, 4], [0.0, 0.0])
        assert all(v == 0.0 for v in out)

    def test_empty_signal_raises(self):
        with pytest.raises(ValueError):
            fir_filter([], [1])

    def test_empty_coefficients_raises(self):
        with pytest.raises(ValueError):
            fir_filter([1, 2, 3], [])


class TestMovingAverage:
    def test_window_one_is_identity(self):
        sig = [1.0, 2.0, 3.0, 4.0]
        assert moving_average(sig, 1) == sig

    def test_window_equals_length(self):
        # Average of entire signal in last position
        out = moving_average([2.0, 4.0, 6.0], 3)
        assert abs(out[-1] - 4.0) < RTOL

    def test_output_length(self):
        out = moving_average([1, 2, 3, 4, 5], 3)
        assert len(out) == 5

    def test_constant_signal(self):
        # Causal FIR: first (window_size-1) outputs use a partial window;
        # only from index (window_size-1) onward is the full average exact.
        out = moving_average([5.0] * 8, 4)
        assert all(abs(v - 5.0) < RTOL for v in out[3:])

    def test_invalid_window_raises(self):
        with pytest.raises(ValueError):
            moving_average([1, 2, 3], 0)

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            moving_average([], 2)


# ===========================================================================
# Analysis
# ===========================================================================

class TestStft:
    def _sinusoid(self):
        return _sine(10.0, 100.0, 256)

    def test_returns_list_of_spectra(self):
        frames = stft(self._sinusoid(), frame_size=32, hop_size=16)
        assert isinstance(frames, list)
        assert len(frames) > 0

    def test_frame_length(self):
        frames = stft(self._sinusoid(), frame_size=32, hop_size=16)
        assert all(len(f) == 32 for f in frames)

    def test_default_hann_window(self):
        # Should not raise; runs with built-in default window
        frames = stft(self._sinusoid())
        assert len(frames) > 0

    def test_custom_window(self):
        w = window_rectangular(32)
        frames = stft(self._sinusoid(), frame_size=32, hop_size=16, window=w)
        assert len(frames) > 0

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            stft([])

    def test_frame_size_invalid_raises(self):
        with pytest.raises(ValueError):
            stft([1, 2, 3, 4], frame_size=0, hop_size=1)

    def test_hop_size_invalid_raises(self):
        with pytest.raises(ValueError):
            stft([1, 2, 3, 4], frame_size=2, hop_size=0)

    def test_window_length_mismatch_raises(self):
        with pytest.raises(ValueError):
            stft([1.0] * 64, frame_size=32, hop_size=16, window=[1.0] * 16)


class TestDominantFrequency:
    def test_10hz_sine(self):
        sig = _sine(10.0, 100.0, 256)
        df = dominant_frequency(sig, 100.0)
        assert abs(df - 10.0) < 1.0

    def test_dc_signal(self):
        sig = [1.0] * 64
        df = dominant_frequency(sig, 1.0)
        assert df == 0.0

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            dominant_frequency([])

    def test_returns_float(self):
        assert isinstance(dominant_frequency([1.0, 0.0, -1.0, 0.0], 4.0), float)


class TestSpectralCentroid:
    def test_symmetric_spectrum(self):
        # Symmetric positive magnitudes cancel if freqs include negatives;
        # with equal magnitudes at +f and -f, centroid = 0
        mags = [1.0, 2.0, 1.0]
        freqs = [-1.0, 0.0, 1.0]
        sc = spectral_centroid(mags, freqs)
        assert abs(sc) < RTOL

    def test_single_bin(self):
        sc = spectral_centroid([3.0], [5.0])
        assert abs(sc - 5.0) < RTOL

    def test_length_mismatch_raises(self):
        with pytest.raises(ValueError):
            spectral_centroid([1, 2, 3], [1, 2])

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            spectral_centroid([], [])

    def test_zero_magnitude_raises(self):
        with pytest.raises(ValueError):
            spectral_centroid([0.0, 0.0], [1.0, 2.0])


class TestSnr:
    def test_high_snr(self):
        clean = [math.sin(2 * math.pi * k / 16) for k in range(64)]
        noise = [0.001 * math.cos(2.3 * k) for k in range(64)]
        assert snr(clean, noise) > 50

    def test_low_snr(self):
        clean = [1.0] * 32
        noise = [0.9] * 32
        assert snr(clean, noise) < 5

    def test_equal_power(self):
        clean = [1.0] * 16
        noise = [1.0] * 16
        assert abs(snr(clean, noise)) < 1e-6

    def test_length_mismatch_raises(self):
        with pytest.raises(ValueError):
            snr([1, 2, 3], [1, 2])

    def test_zero_noise_raises(self):
        with pytest.raises(ValueError):
            snr([1.0, 2.0], [0.0, 0.0])

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            snr([], [])


class TestZeroCrossingRate:
    def test_square_wave(self):
        sq = [1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0]
        assert zero_crossing_rate(sq) == 1.0

    def test_constant_positive(self):
        assert zero_crossing_rate([1.0] * 8) == 0.0

    def test_single_zero_crossing(self):
        zcr = zero_crossing_rate([1.0, 1.0, 1.0, -1.0, -1.0])
        assert abs(zcr - 1 / 4) < RTOL

    def test_too_short_raises(self):
        with pytest.raises(ValueError):
            zero_crossing_rate([1.0])

    def test_returns_in_unit_interval(self):
        sig = [math.sin(2 * math.pi * k / 8) for k in range(16)]
        zcr = zero_crossing_rate(sig)
        assert 0.0 <= zcr <= 1.0


# ===========================================================================
# Utilities
# ===========================================================================

class TestFftShift:
    def test_basic_even(self):
        assert fft_shift([0, 1, 2, 3, 4, 5, 6, 7]) == [4, 5, 6, 7, 0, 1, 2, 3]

    def test_basic_odd(self):
        shifted = fft_shift([0, 1, 2, 3, 4])
        assert shifted == [2, 3, 4, 0, 1]

    def test_idempotent_double_shift_even(self):
        original = [0, 1, 2, 3, 4, 5, 6, 7]
        assert fft_shift(fft_shift(original)) == original

    def test_empty_raises(self):
        with pytest.raises(ValueError):
            fft_shift([])

    def test_single_element(self):
        assert fft_shift([42]) == [42]


class TestNextPowerOfTwo:
    def test_exact_powers(self):
        for exp in range(1, 12):
            n = 2 ** exp
            assert next_power_of_two(n) == n

    def test_one_above(self):
        assert next_power_of_two(5) == 8
        assert next_power_of_two(9) == 16
        assert next_power_of_two(33) == 64

    def test_one_is_one(self):
        assert next_power_of_two(1) == 1

    def test_invalid_raises(self):
        with pytest.raises(ValueError):
            next_power_of_two(0)


class TestDbToLinear:
    def test_0db(self):
        assert abs(db_to_linear(0.0) - 1.0) < RTOL

    def test_20db(self):
        assert abs(db_to_linear(20.0) - 10.0) < 1e-9

    def test_minus_20db(self):
        assert abs(db_to_linear(-20.0) - 0.1) < 1e-9

    def test_6db_approx_double(self):
        # +6 dB ~ factor 2
        assert abs(db_to_linear(6.0) - 2.0) < 0.01


class TestLinearToDb:
    def test_unity(self):
        assert abs(linear_to_db(1.0)) < RTOL

    def test_10_is_20db(self):
        assert abs(linear_to_db(10.0) - 20.0) < 1e-9

    def test_01_is_minus_20db(self):
        assert abs(linear_to_db(0.1) - (-20.0)) < 1e-9

    def test_zero_raises(self):
        with pytest.raises(ValueError):
            linear_to_db(0.0)

    def test_negative_raises(self):
        with pytest.raises(ValueError):
            linear_to_db(-1.0)

    def test_round_trip(self):
        for v in [0.5, 1.0, 2.0, 100.0]:
            assert abs(db_to_linear(linear_to_db(v)) - v) < 1e-9


class TestResample:
    def test_same_length(self):
        sig = [1.0, 2.0, 3.0, 4.0]
        out = resample(sig, 4)
        assert out == pytest.approx(sig, abs=RTOL)

    def test_upsample_endpoints(self):
        out = resample([0.0, 1.0, 2.0, 3.0], 7)
        assert abs(out[0] - 0.0) < RTOL
        assert abs(out[-1] - 3.0) < RTOL

    def test_downsample_length(self):
        out = resample([0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0], 4)
        assert len(out) == 4

    def test_upsample_monotonic(self):
        out = resample([0.0, 10.0], 5)
        diffs = [out[i + 1] - out[i] for i in range(len(out) - 1)]
        assert all(d > 0 for d in diffs)

    def test_too_short_raises(self):
        with pytest.raises(ValueError):
            resample([1.0], 4)

    def test_invalid_target_raises(self):
        with pytest.raises(ValueError):
            resample([1.0, 2.0], 0)

    def test_linear_interpolation(self):
        out = resample([0.0, 4.0], 5)
        assert out == pytest.approx([0.0, 1.0, 2.0, 3.0, 4.0], abs=RTOL)


# ===========================================================================
# Registration
# ===========================================================================

class TestRegistration:
    EXPECTED = [
        "fft", "ifft", "rfft", "irfft",
        "fft_magnitude", "fft_phase", "fft_power", "fft_freqs",
        "window_rectangular", "window_hann", "window_hamming",
        "window_blackman", "window_bartlett", "apply_window",
        "convolve", "fft_convolve", "correlate", "fir_filter", "moving_average",
        "stft", "dominant_frequency", "spectral_centroid", "snr",
        "zero_crossing_rate",
        "fft_shift", "next_power_of_two", "db_to_linear", "linear_to_db",
        "resample",
    ]

    def _make_runtime(self):
        class FakeRuntime:
            def __init__(self):
                self.functions = {}
            def register_function(self, name, fn):
                self.functions[name] = fn
        return FakeRuntime()

    def test_all_expected_registered(self):
        rt = self._make_runtime()
        register_dsp_functions(rt)
        for name in self.EXPECTED:
            assert name in rt.functions, f"Missing: {name}"

    def test_all_callables(self):
        rt = self._make_runtime()
        register_dsp_functions(rt)
        for name, fn in rt.functions.items():
            assert callable(fn), f"{name} is not callable"

    def test_count(self):
        rt = self._make_runtime()
        register_dsp_functions(rt)
        assert len(rt.functions) == len(self.EXPECTED)
