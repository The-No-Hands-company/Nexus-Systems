"""Smoke test for the dsp stdlib module."""
import sys
import math

sys.path.insert(0, "src")

import nexuslang.stdlib.dsp as dsp_mod

fft = dsp_mod.fft
ifft = dsp_mod.ifft
rfft = dsp_mod.rfft
irfft = dsp_mod.irfft
fft_magnitude = dsp_mod.fft_magnitude
fft_phase = dsp_mod.fft_phase
fft_power = dsp_mod.fft_power
fft_freqs = dsp_mod.fft_freqs
window_hann = dsp_mod.window_hann
window_hamming = dsp_mod.window_hamming
window_blackman = dsp_mod.window_blackman
window_bartlett = dsp_mod.window_bartlett
window_rectangular = dsp_mod.window_rectangular
apply_window = dsp_mod.apply_window
convolve = dsp_mod.convolve
fft_convolve = dsp_mod.fft_convolve
correlate = dsp_mod.correlate
fir_filter = dsp_mod.fir_filter
moving_average = dsp_mod.moving_average
stft = dsp_mod.stft
dominant_frequency = dsp_mod.dominant_frequency
spectral_centroid = dsp_mod.spectral_centroid
snr = dsp_mod.snr
zero_crossing_rate = dsp_mod.zero_crossing_rate
fft_shift = dsp_mod.fft_shift
next_power_of_two = dsp_mod.next_power_of_two
db_to_linear = dsp_mod.db_to_linear
linear_to_db = dsp_mod.linear_to_db
resample = dsp_mod.resample

# FFT round-trip
sig = [math.sin(2 * math.pi * k / 8) for k in range(8)]
X = fft(sig)
x2 = ifft(X)
err = max(abs(x2[k].real - sig[k]) for k in range(8))
print(f"FFT round-trip error : {err:.2e}  (expect < 1e-14)")
assert err < 1e-12, f"FFT round-trip too large: {err}"

# rfft length
rs = rfft(sig)
print(f"rfft length          : {len(rs)}  (expect 5 = 8//2+1)")
assert len(rs) == 5

# irfft round-trip
sig2 = irfft(rs, len(sig))
err2 = max(abs(sig2[k] - sig[k]) for k in range(len(sig)))
print(f"irfft round-trip err : {err2:.2e}  (expect < 1e-12)")
assert err2 < 1e-10

# fft_freqs
freqs = fft_freqs(8, 8.0)
print(f"fft_freqs(8,8)       : {[round(f,1) for f in freqs]}")
assert freqs[0] == 0.0 and abs(freqs[1] - 1.0) < 1e-9

# fft_magnitude / phase / power
mags = fft_magnitude(X)
peak = mags.index(max(mags))
print(f"FFT peak bin         : {peak}  (expect 1)")
assert peak == 1

phases = fft_phase(X)
print(f"fft_phase length     : {len(phases)} (expect 8)")
assert len(phases) == 8

pows = fft_power(X)
assert all(p >= 0 for p in pows)
print("fft_power            : all non-negative (ok)")

# Windowing
hann4 = window_hann(4)
print(f"window_hann(4)       : {[round(v,4) for v in hann4]}")
assert abs(hann4[0]) < 1e-12 and abs(hann4[3]) < 1e-12

ham4 = window_hamming(4)
print(f"window_hamming(4)    : {[round(v,4) for v in ham4]}")
assert abs(ham4[0] - 0.08) < 1e-9

blk4 = window_blackman(4)
print(f"window_blackman(4)   : {[round(v,4) for v in blk4]}")

bart4 = window_bartlett(4)
print(f"window_bartlett(4)   : {[round(v,4) for v in bart4]}")
assert bart4[0] == 0.0 or abs(bart4[0]) < 1e-12

rect4 = window_rectangular(4)
assert rect4 == [1.0, 1.0, 1.0, 1.0]
print("window_rectangular(4): [1.0, 1.0, 1.0, 1.0] (ok)")

aw = apply_window([1, 2, 3, 4], rect4)
assert aw == [1.0, 2.0, 3.0, 4.0]
print("apply_window rect    : identity (ok)")

# Convolution
c = convolve([1, 2, 3], [1, 1])
print(f"convolve [1,2,3]*[1,1]: {c}  (expect [1,3,5,3])")
assert c == [1.0, 3.0, 5.0, 3.0], f"got {c}"

c2 = fft_convolve([1, 2, 3], [1, 1])
err3 = max(abs(c[i] - c2[i]) for i in range(len(c)))
print(f"fft_convolve error   : {err3:.2e}  (expect < 1e-12)")
assert err3 < 1e-10

r = correlate([1, 2, 3], [1, 0])
print(f"correlate            : {[round(v,4) for v in r]}")

smoothed = fir_filter([1, 2, 3, 4, 5], [0.5, 0.5])
print(f"fir_filter           : {smoothed}")
assert abs(smoothed[1] - 1.5) < 1e-12

ma = moving_average([0, 1, 2, 3, 4, 5], 3)
print(f"moving_average(3)    : {[round(v,4) for v in ma]}")
assert abs(ma[5] - (3/3 + 4/3 + 5/3) / 1) < 0.01

# Analysis
fs = 100.0
sinusoid = [math.sin(2 * math.pi * 10 * k / fs) for k in range(256)]
df = dominant_frequency(sinusoid, fs)
print(f"dominant_freq 10Hz   : {round(df,2)} Hz  (expect ~10)")
assert abs(df - 10.0) < 0.5

mags_s = fft_magnitude(fft(sinusoid))
fr_s = fft_freqs(len(sinusoid), fs)
sc = spectral_centroid(mags_s[:len(fr_s)], fr_s[:len(mags_s)])
print(f"spectral_centroid    : {round(sc,1)} Hz")

clean = [math.sin(2 * math.pi * k / 16) for k in range(64)]
noisy = [0.01 * math.sin(3.7 * k) for k in range(64)]
snr_val = snr(clean, noisy)
print(f"SNR                  : {round(snr_val,1)} dB  (expect > 30)")
assert snr_val > 30

zcr = zero_crossing_rate([1, -1, 1, -1, 1, -1, 1, -1])
print(f"ZCR square wave      : {zcr}  (expect 1.0)")
assert zcr == 1.0

# STFT
frames = stft(sinusoid, frame_size=32, hop_size=16)
print(f"stft frames          : {len(frames)}, each len {len(frames[0])}")
assert len(frames) > 0 and len(frames[0]) == 32

# Utilities
shifted = fft_shift([0, 1, 2, 3, 4, 5, 6, 7])
print(f"fft_shift [0..7]     : {shifted}")
assert shifted == [4, 5, 6, 7, 0, 1, 2, 3]

p = next_power_of_two(5)
print(f"next_pow2(5)         : {p}  (expect 8)")
assert p == 8

lin = db_to_linear(20)
print(f"db_to_linear(20)     : {round(lin,4)}  (expect 10.0)")
assert abs(lin - 10.0) < 1e-9

db = linear_to_db(10)
print(f"linear_to_db(10)     : {round(db,4)}  (expect 20.0)")
assert abs(db - 20.0) < 1e-9

rs2 = resample([0, 1, 2, 3], 7)
print(f"resample [0,1,2,3]x7 : {[round(v,3) for v in rs2]}")
assert abs(rs2[0] - 0.0) < 1e-9 and abs(rs2[-1] - 3.0) < 1e-9

print()
print("ALL OK")
