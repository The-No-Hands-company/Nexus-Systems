"""
Digital Signal Processing (DSP) stdlib module for NexusLang.

Provides FFT/IFFT, windowing functions, FIR filtering, convolution,
spectral analysis, and short-time Fourier transform.

All implementations are pure Python using only the standard library
(cmath, math). No numpy or scipy required.

Registered NexusLang functions (29 total):
  FFT / Spectral (8):
    fft, ifft, rfft, irfft,
    fft_magnitude, fft_phase, fft_power, fft_freqs

  Windowing (6):
    window_rectangular, window_hann, window_hamming,
    window_blackman, window_bartlett, apply_window

  Filtering (5):
    convolve, fft_convolve, correlate, fir_filter, moving_average

  Analysis (5):
    stft, dominant_frequency, spectral_centroid, snr, zero_crossing_rate

  Utilities (5):
    fft_shift, next_power_of_two, db_to_linear, linear_to_db, resample
"""

import cmath
import math
from typing import List, Union

from ...runtime.runtime import Runtime

# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _to_float(v) -> float:
    """Coerce a value to float, raising TypeError on failure."""
    if isinstance(v, (int, float)):
        return float(v)
    if isinstance(v, complex):
        return float(v.real)
    raise TypeError(f"Expected a numeric value, got {type(v).__name__}")


def _to_complex(v) -> complex:
    """Coerce a value to complex."""
    if isinstance(v, (int, float)):
        return complex(float(v), 0.0)
    if isinstance(v, complex):
        return v
    raise TypeError(f"Expected a numeric value, got {type(v).__name__}")


def _next_pow2(n: int) -> int:
    """Return the smallest power of 2 >= n."""
    if n <= 0:
        return 1
    n -= 1
    n |= n >> 1
    n |= n >> 2
    n |= n >> 4
    n |= n >> 8
    n |= n >> 16
    n |= n >> 32
    return n + 1


def _bit_reverse_copy(x: list) -> list:
    """Return a copy of x with elements in bit-reversal order. len(x) must be power of 2."""
    n = len(x)
    out = list(x)
    bits = n.bit_length() - 1  # log2(n)
    for i in range(n):
        rev = int(f"{i:0{bits}b}"[::-1], 2)
        if rev > i:
            out[i], out[rev] = out[rev], out[i]
    return out


def _fft_inplace(x: list) -> list:
    """
    Iterative Cooley-Tukey radix-2 DIT FFT (in-place on list x).
    len(x) MUST be a power of 2. Elements are coerced to complex.
    Returns the same list (mutated).
    """
    n = len(x)
    # Bit-reversal permutation
    bits = n.bit_length() - 1
    for i in range(n):
        rev = int(f"{i:0{bits}b}"[::-1], 2)
        if rev > i:
            x[i], x[rev] = x[rev], x[i]
    # Butterfly stages
    length = 2
    while length <= n:
        half = length >> 1
        w_step = cmath.exp(-2j * math.pi / length)
        for start in range(0, n, length):
            w = 1.0 + 0j
            for k in range(half):
                u = x[start + k]
                v = x[start + k + half] * w
                x[start + k] = u + v
                x[start + k + half] = u - v
                w *= w_step
        length <<= 1
    return x


def _prepare_signal(signal: list) -> list:
    """Convert signal to complex list, zero-padded to next power of 2."""
    n = len(signal)
    if n == 0:
        raise ValueError("Signal must not be empty")
    m = _next_pow2(n)
    out = [_to_complex(v) for v in signal]
    out.extend([0j] * (m - n))
    return out


# ---------------------------------------------------------------------------
# FFT / Spectral
# ---------------------------------------------------------------------------

def fft(signal: list) -> list:
    """
    Compute the Discrete Fourier Transform of a real or complex signal.

    Zero-pads to the next power of 2 if needed. Returns a list of complex
    values of length N (the padded length).

    Args:
        signal: List of numeric values (int, float, or complex).

    Returns:
        List of complex DFT coefficients.

    Raises:
        ValueError: If signal is empty.
    """
    x = _prepare_signal(signal)
    _fft_inplace(x)
    return x


def ifft(spectrum: list) -> list:
    """
    Compute the Inverse Discrete Fourier Transform.

    Uses the identity: IFFT(X) = conj(FFT(conj(X))) / N.
    Input length must be a power of 2.

    Args:
        spectrum: List of complex DFT coefficients.

    Returns:
        List of complex reconstructed signal values.

    Raises:
        ValueError: If spectrum is empty or length is not a power of 2.
    """
    n = len(spectrum)
    if n == 0:
        raise ValueError("Spectrum must not be empty")
    if n & (n - 1) != 0:
        raise ValueError(f"Spectrum length must be a power of 2, got {n}")
    # Conjugate
    x = [_to_complex(v).conjugate() for v in spectrum]
    # Forward FFT
    _fft_inplace(x)
    # Conjugate and scale
    x = [v.conjugate() / n for v in x]
    return x


def rfft(signal: list) -> list:
    """
    One-sided FFT for real-valued signals.

    Returns only the non-negative frequency bins: length n//2 + 1 where n
    is the (padded) signal length.

    Args:
        signal: List of real or complex numeric values.

    Returns:
        List of complex coefficients for frequencies 0 .. fs/2.

    Raises:
        ValueError: If signal is empty.
    """
    x = fft(signal)
    n = len(x)
    return x[: n // 2 + 1]


def irfft(spectrum: list, n: int = 0) -> list:
    """
    Inverse one-sided FFT, reconstructing a real signal.

    Args:
        spectrum: One-sided complex spectrum (output of rfft).
        n:        Desired output length. If 0, output length = 2*(len(spectrum)-1).

    Returns:
        List of float values (real part of IFFT output).

    Raises:
        ValueError: If spectrum is empty.
    """
    if len(spectrum) == 0:
        raise ValueError("Spectrum must not be empty")
    m = len(spectrum)
    full_n = 2 * (m - 1) if n == 0 else n
    # Reconstruct full two-sided spectrum
    full = list(spectrum)
    for i in range(m - 2, 0, -1):
        full.append(spectrum[i].conjugate() if isinstance(spectrum[i], complex) else complex(float(spectrum[i])).conjugate())
    # Pad or trim to next power of 2 for IFFT
    padded_n = _next_pow2(len(full))
    full.extend([0j] * (padded_n - len(full)))
    result = ifft(full)
    return [v.real for v in result[:full_n]]


def fft_magnitude(spectrum: list) -> list:
    """
    Compute the magnitude (absolute value) of each DFT coefficient.

    Args:
        spectrum: List of complex DFT coefficients.

    Returns:
        List of float magnitudes.
    """
    return [abs(_to_complex(v)) for v in spectrum]


def fft_phase(spectrum: list) -> list:
    """
    Compute the phase angle (in radians) of each DFT coefficient.

    Args:
        spectrum: List of complex DFT coefficients.

    Returns:
        List of float phase angles in [-pi, pi].
    """
    return [cmath.phase(_to_complex(v)) for v in spectrum]


def fft_power(spectrum: list) -> list:
    """
    Compute the power spectrum |X[k]|^2 for each DFT coefficient.

    Args:
        spectrum: List of complex DFT coefficients.

    Returns:
        List of float power values.
    """
    return [abs(_to_complex(v)) ** 2 for v in spectrum]


def fft_freqs(n: int, sample_rate: float = 1.0) -> list:
    """
    Return the DFT sample frequencies for a signal of length n.

    The result is the list [0, fs/n, 2*fs/n, ..., (n-1)*fs/n] with
    frequencies above the Nyquist folded to negative, matching numpy.fft.fftfreq.

    Args:
        n:           Number of samples (signal length).
        sample_rate: Sampling rate in Hz (default 1.0).

    Returns:
        List of float frequency values.

    Raises:
        ValueError: If n < 1.
    """
    if n < 1:
        raise ValueError(f"n must be >= 1, got {n}")
    n = int(n)
    fs = float(sample_rate)
    half = (n - 1) // 2 + 1
    # Positive frequencies then negative
    pos = [k * fs / n for k in range(half)]
    neg = [-(n // 2 - k) * fs / n for k in range(n // 2)]
    return pos + neg


# ---------------------------------------------------------------------------
# Windowing
# ---------------------------------------------------------------------------

def window_rectangular(n: int) -> list:
    """
    Rectangular (boxcar) window of length n. All coefficients are 1.0.

    Args:
        n: Window length.

    Returns:
        List of n float values equal to 1.0.

    Raises:
        ValueError: If n < 1.
    """
    n = int(n)
    if n < 1:
        raise ValueError(f"Window length must be >= 1, got {n}")
    return [1.0] * n


def window_hann(n: int) -> list:
    """
    Hann (Hanning) window of length n.

    w[k] = 0.5 * (1 - cos(2*pi*k / (n-1)))

    Args:
        n: Window length.

    Returns:
        List of n float coefficients.

    Raises:
        ValueError: If n < 1.
    """
    n = int(n)
    if n < 1:
        raise ValueError(f"Window length must be >= 1, got {n}")
    if n == 1:
        return [1.0]
    return [0.5 * (1.0 - math.cos(2.0 * math.pi * k / (n - 1))) for k in range(n)]


def window_hamming(n: int) -> list:
    """
    Hamming window of length n.

    w[k] = 0.54 - 0.46 * cos(2*pi*k / (n-1))

    Args:
        n: Window length.

    Returns:
        List of n float coefficients.

    Raises:
        ValueError: If n < 1.
    """
    n = int(n)
    if n < 1:
        raise ValueError(f"Window length must be >= 1, got {n}")
    if n == 1:
        return [1.0]
    return [0.54 - 0.46 * math.cos(2.0 * math.pi * k / (n - 1)) for k in range(n)]


def window_blackman(n: int) -> list:
    """
    Blackman window of length n.

    w[k] = 0.42 - 0.5*cos(2*pi*k/(n-1)) + 0.08*cos(4*pi*k/(n-1))

    Args:
        n: Window length.

    Returns:
        List of n float coefficients.

    Raises:
        ValueError: If n < 1.
    """
    n = int(n)
    if n < 1:
        raise ValueError(f"Window length must be >= 1, got {n}")
    if n == 1:
        return [1.0]
    return [
        0.42 - 0.5 * math.cos(2.0 * math.pi * k / (n - 1))
            + 0.08 * math.cos(4.0 * math.pi * k / (n - 1))
        for k in range(n)
    ]


def window_bartlett(n: int) -> list:
    """
    Bartlett (triangular) window of length n.

    w[k] = 1 - |2k/(n-1) - 1|

    Args:
        n: Window length.

    Returns:
        List of n float coefficients.

    Raises:
        ValueError: If n < 1.
    """
    n = int(n)
    if n < 1:
        raise ValueError(f"Window length must be >= 1, got {n}")
    if n == 1:
        return [1.0]
    return [1.0 - abs(2.0 * k / (n - 1) - 1.0) for k in range(n)]


def apply_window(signal: list, window: list) -> list:
    """
    Multiply a signal element-wise by a window function.

    Args:
        signal: List of numeric values.
        window: List of window coefficients. Must match len(signal).

    Returns:
        List of float windowed values.

    Raises:
        ValueError: If lengths differ or either list is empty.
    """
    if len(signal) == 0 or len(window) == 0:
        raise ValueError("Signal and window must not be empty")
    if len(signal) != len(window):
        raise ValueError(
            f"Signal length {len(signal)} does not match window length {len(window)}"
        )
    return [_to_float(s) * _to_float(w) for s, w in zip(signal, window)]


# ---------------------------------------------------------------------------
# Filtering
# ---------------------------------------------------------------------------

def convolve(a: list, b: list) -> list:
    """
    Compute the linear (direct) convolution of two sequences.

    Output length = len(a) + len(b) - 1.

    Args:
        a: First input sequence (list of numeric values).
        b: Second input sequence (list of numeric values).

    Returns:
        List of float convolution values.

    Raises:
        ValueError: If either list is empty.
    """
    if len(a) == 0 or len(b) == 0:
        raise ValueError("Input sequences must not be empty")
    la, lb = len(a), len(b)
    fa = [_to_float(v) for v in a]
    fb = [_to_float(v) for v in b]
    result = [0.0] * (la + lb - 1)
    for i in range(la):
        for j in range(lb):
            result[i + j] += fa[i] * fb[j]
    return result


def fft_convolve(a: list, b: list) -> list:
    """
    Compute linear convolution using the FFT (faster for long sequences).

    Output length = len(a) + len(b) - 1, trimmed from the full padded FFT.

    Args:
        a: First input sequence.
        b: Second input sequence.

    Returns:
        List of float convolution values.

    Raises:
        ValueError: If either list is empty.
    """
    if len(a) == 0 or len(b) == 0:
        raise ValueError("Input sequences must not be empty")
    out_len = len(a) + len(b) - 1
    n = _next_pow2(out_len)
    # Zero-pad both to n
    fa = [_to_complex(v) for v in a] + [0j] * (n - len(a))
    fb = [_to_complex(v) for v in b] + [0j] * (n - len(b))
    _fft_inplace(fa)
    _fft_inplace(fb)
    product = [fa[k] * fb[k] for k in range(n)]
    result = ifft(product)
    return [v.real for v in result[:out_len]]


def correlate(a: list, b: list) -> list:
    """
    Compute the cross-correlation of two sequences.

    Equivalent to convolving a with the time-reversed b.
    Output length = len(a) + len(b) - 1.

    Args:
        a: First input sequence.
        b: Second input sequence.

    Returns:
        List of float cross-correlation values.

    Raises:
        ValueError: If either list is empty.
    """
    if len(a) == 0 or len(b) == 0:
        raise ValueError("Input sequences must not be empty")
    b_rev = list(reversed([_to_float(v) for v in b]))
    return convolve([_to_float(v) for v in a], b_rev)


def fir_filter(signal: list, coefficients: list) -> list:
    """
    Apply a Finite Impulse Response (FIR) filter to a signal.

    Computes the causal convolution: each output sample depends only on
    current and previous input samples.

    y[n] = sum(h[k] * x[n-k] for k in range(len(h)))

    Args:
        signal:       Input signal (list of numeric values).
        coefficients: FIR filter coefficients h[0..M-1].

    Returns:
        List of float output values, same length as signal.

    Raises:
        ValueError: If either list is empty.
    """
    if len(signal) == 0:
        raise ValueError("Signal must not be empty")
    if len(coefficients) == 0:
        raise ValueError("Coefficients must not be empty")
    x = [_to_float(v) for v in signal]
    h = [_to_float(v) for v in coefficients]
    m = len(h)
    n = len(x)
    out = []
    for i in range(n):
        acc = 0.0
        for k in range(m):
            if i - k >= 0:
                acc += h[k] * x[i - k]
        out.append(acc)
    return out


def moving_average(signal: list, window_size: int) -> list:
    """
    Apply a simple moving average (box) filter to a signal.

    Each output sample is the mean of the surrounding window_size input samples.
    Edge values use a smaller window (causal: only past samples).

    Args:
        signal:      Input signal.
        window_size: Number of samples to average.

    Returns:
        List of float smoothed values, same length as signal.

    Raises:
        ValueError: If signal is empty or window_size < 1.
    """
    if len(signal) == 0:
        raise ValueError("Signal must not be empty")
    w = int(window_size)
    if w < 1:
        raise ValueError(f"window_size must be >= 1, got {w}")
    x = [_to_float(v) for v in signal]
    coeffs = [1.0 / w] * w
    return fir_filter(x, coeffs)


# ---------------------------------------------------------------------------
# Analysis
# ---------------------------------------------------------------------------

def stft(
    signal: list,
    frame_size: int = 256,
    hop_size: int = 128,
    window: list = None,
) -> list:
    """
    Short-Time Fourier Transform.

    Splits the signal into overlapping frames, applies a window function,
    and computes the FFT of each frame.

    Args:
        signal:     Input signal (list of numeric values).
        frame_size: Number of samples per frame (default 256).
        hop_size:   Step between frame starts in samples (default 128).
        window:     Window coefficients of length frame_size. If None,
                    a Hann window is used.

    Returns:
        List of spectra: each element is a list of frame_size complex values.

    Raises:
        ValueError: If signal is empty, frame_size < 1, or hop_size < 1.
    """
    if len(signal) == 0:
        raise ValueError("Signal must not be empty")
    frame_size = int(frame_size)
    hop_size = int(hop_size)
    if frame_size < 1:
        raise ValueError(f"frame_size must be >= 1, got {frame_size}")
    if hop_size < 1:
        raise ValueError(f"hop_size must be >= 1, got {hop_size}")
    if window is None:
        window = window_hann(frame_size)
    if len(window) != frame_size:
        raise ValueError(
            f"window length {len(window)} does not match frame_size {frame_size}"
        )
    x = [_to_float(v) for v in signal]
    n = len(x)
    spectra = []
    start = 0
    while start + frame_size <= n:
        frame = x[start : start + frame_size]
        windowed = [frame[k] * window[k] for k in range(frame_size)]
        spec = fft(windowed)
        # fft zero-pads to power of 2; trim back to frame_size
        spectra.append(spec[:frame_size])
        start += hop_size
    return spectra


def dominant_frequency(signal: list, sample_rate: float = 1.0) -> float:
    """
    Find the frequency with the highest magnitude in a signal's spectrum.

    Uses the one-sided FFT to find the bin with maximum magnitude among
    positive frequencies.

    Args:
        signal:      Input signal (list of numeric values).
        sample_rate: Sampling rate in Hz (default 1.0).

    Returns:
        Dominant frequency in Hz.

    Raises:
        ValueError: If signal is empty.
    """
    if len(signal) == 0:
        raise ValueError("Signal must not be empty")
    spec = rfft(signal)
    mags = [abs(v) for v in spec]
    n = len(signal)
    dominant_bin = mags.index(max(mags))
    n_padded = _next_pow2(n)
    freq = dominant_bin * float(sample_rate) / n_padded
    return freq


def spectral_centroid(magnitudes: list, freqs: list) -> float:
    """
    Compute the spectral centroid — the weighted mean frequency.

    centroid = sum(f[k] * |X[k]|) / sum(|X[k]|)

    Args:
        magnitudes: List of float magnitude values (from fft_magnitude).
        freqs:      Corresponding frequency values (from fft_freqs).

    Returns:
        Spectral centroid frequency.

    Raises:
        ValueError: If lengths differ, lists are empty, or total magnitude is zero.
    """
    if len(magnitudes) == 0 or len(freqs) == 0:
        raise ValueError("magnitudes and freqs must not be empty")
    if len(magnitudes) != len(freqs):
        raise ValueError(
            f"magnitudes length {len(magnitudes)} does not match freqs length {len(freqs)}"
        )
    mags = [_to_float(v) for v in magnitudes]
    frs = [_to_float(v) for v in freqs]
    total = sum(mags)
    if total == 0.0:
        raise ValueError("Total magnitude is zero; spectral centroid undefined")
    return sum(f * m for f, m in zip(frs, mags)) / total


def snr(signal: list, noise: list) -> float:
    """
    Compute Signal-to-Noise Ratio in decibels.

    SNR_dB = 10 * log10(P_signal / P_noise)

    where P = sum(x^2) / N is the mean square power.

    Args:
        signal: Clean signal samples.
        noise:  Noise samples of the same length as signal.

    Returns:
        SNR in dB (float).

    Raises:
        ValueError: If lengths differ, either list is empty, or noise power is 0.
    """
    if len(signal) == 0 or len(noise) == 0:
        raise ValueError("signal and noise must not be empty")
    if len(signal) != len(noise):
        raise ValueError(
            f"signal length {len(signal)} does not match noise length {len(noise)}"
        )
    p_sig = sum(_to_float(v) ** 2 for v in signal) / len(signal)
    p_noi = sum(_to_float(v) ** 2 for v in noise) / len(noise)
    if p_noi == 0.0:
        raise ValueError("Noise power is zero; SNR undefined")
    return 10.0 * math.log10(p_sig / p_noi)


def zero_crossing_rate(signal: list) -> float:
    """
    Compute the zero-crossing rate of a signal.

    ZCR = (number of sign changes) / (N - 1)

    Args:
        signal: Input signal (list of numeric values, length >= 2).

    Returns:
        Float in [0, 1] representing the fraction of consecutive pairs
        with a sign change.

    Raises:
        ValueError: If signal has fewer than 2 samples.
    """
    if len(signal) < 2:
        raise ValueError("Signal must have at least 2 samples")
    x = [_to_float(v) for v in signal]
    crossings = sum(
        1 for i in range(1, len(x)) if (x[i - 1] >= 0) != (x[i] >= 0)
    )
    return crossings / (len(x) - 1)


# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------

def fft_shift(spectrum: list) -> list:
    """
    Shift zero-frequency component to the center of the spectrum.

    Moves the DC component from index 0 to the middle, matching the
    convention used by numpy.fft.fftshift.

    Args:
        spectrum: DFT output (list of complex or float values).

    Returns:
        Shifted list of the same length.

    Raises:
        ValueError: If spectrum is empty.
    """
    if len(spectrum) == 0:
        raise ValueError("Spectrum must not be empty")
    n = len(spectrum)
    mid = n // 2
    return list(spectrum[mid:]) + list(spectrum[:mid])


def next_power_of_two(n: int) -> int:
    """
    Return the smallest integer power of 2 that is >= n.

    Args:
        n: Input integer.

    Returns:
        The next (or equal) power of 2.

    Raises:
        ValueError: If n < 1.
    """
    n = int(n)
    if n < 1:
        raise ValueError(f"n must be >= 1, got {n}")
    return _next_pow2(n)


def db_to_linear(db: float) -> float:
    """
    Convert a decibel value to a linear amplitude ratio.

    linear = 10^(dB / 20)

    Args:
        db: Value in decibels.

    Returns:
        Linear amplitude ratio.
    """
    return 10.0 ** (_to_float(db) / 20.0)


def linear_to_db(linear: float) -> float:
    """
    Convert a linear amplitude ratio to decibels.

    dB = 20 * log10(linear)

    Args:
        linear: Positive linear amplitude ratio.

    Returns:
        Value in decibels.

    Raises:
        ValueError: If linear <= 0.
    """
    v = _to_float(linear)
    if v <= 0.0:
        raise ValueError(f"linear value must be > 0, got {v}")
    return 20.0 * math.log10(v)


def resample(signal: list, target_length: int) -> list:
    """
    Resample a signal to a new length using linear interpolation.

    Args:
        signal:        Input signal.
        target_length: Desired output length.

    Returns:
        List of float resampled values of length target_length.

    Raises:
        ValueError: If signal has fewer than 2 samples or target_length < 1.
    """
    if len(signal) < 2:
        raise ValueError("Signal must have at least 2 samples for resampling")
    target_length = int(target_length)
    if target_length < 1:
        raise ValueError(f"target_length must be >= 1, got {target_length}")
    x = [_to_float(v) for v in signal]
    n_in = len(x)
    out = []
    for i in range(target_length):
        pos = i * (n_in - 1) / (target_length - 1) if target_length > 1 else 0.0
        lo = int(pos)
        hi = min(lo + 1, n_in - 1)
        frac = pos - lo
        out.append(x[lo] * (1.0 - frac) + x[hi] * frac)
    return out


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_dsp_functions(runtime: Runtime) -> None:
    """Register all DSP functions with the NexusLang runtime."""
    # FFT / Spectral
    runtime.register_function("fft", fft)
    runtime.register_function("ifft", ifft)
    runtime.register_function("rfft", rfft)
    runtime.register_function("irfft", irfft)
    runtime.register_function("fft_magnitude", fft_magnitude)
    runtime.register_function("fft_phase", fft_phase)
    runtime.register_function("fft_power", fft_power)
    runtime.register_function("fft_freqs", fft_freqs)
    # Windowing
    runtime.register_function("window_rectangular", window_rectangular)
    runtime.register_function("window_hann", window_hann)
    runtime.register_function("window_hamming", window_hamming)
    runtime.register_function("window_blackman", window_blackman)
    runtime.register_function("window_bartlett", window_bartlett)
    runtime.register_function("apply_window", apply_window)
    # Filtering
    runtime.register_function("convolve", convolve)
    runtime.register_function("fft_convolve", fft_convolve)
    runtime.register_function("correlate", correlate)
    runtime.register_function("fir_filter", fir_filter)
    runtime.register_function("moving_average", moving_average)
    # Analysis
    runtime.register_function("stft", stft)
    runtime.register_function("dominant_frequency", dominant_frequency)
    runtime.register_function("spectral_centroid", spectral_centroid)
    runtime.register_function("snr", snr)
    runtime.register_function("zero_crossing_rate", zero_crossing_rate)
    # Utilities
    runtime.register_function("fft_shift", fft_shift)
    runtime.register_function("next_power_of_two", next_power_of_two)
    runtime.register_function("db_to_linear", db_to_linear)
    runtime.register_function("linear_to_db", linear_to_db)
    runtime.register_function("resample", resample)
