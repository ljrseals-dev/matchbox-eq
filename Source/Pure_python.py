#!/usr/bin/env python3
"""
MatchboxEQ – Corrected pure-Python reference implementation
Fixes applied:
  - Independent mid/side filter states
  - asked_bands clamped to [1, 12]
  - Envelope follower slowed (alpha = 0.001 ≈ 20 ms)
  - Proper scipy.signal.lfilter state handling
  - Coefficient caching for bell filters
  - Soft-clip unity-gain compensation
"""

import numpy as np
from scipy.signal import lfilter

class MatchboxEQ:
    def __init__(self, sample_rate=48000):
        self.sr = sample_rate
        self.init_done = False
        self.mod_phase = 0.0
        self.mod_rate = 0.14
        self.energy_alpha = 0.001          # FIX: was 0.04 → distortion

        self.freqs = None
        self.bp_b = None
        self.bp_a = None
        self.bp_zi = None
        self.bell_zi_mid = None
        self.bell_zi_side = None
        self.band_energy = None

        # Coefficient cache
        self._bell_cache_mid = [None] * 50
        self._bell_cache_side = [None] * 50
        self._last_gain_mid = np.ones(50)
        self._last_gain_side = np.ones(50)

    def _init(self):
        low, high = 40.0, 18000.0
        ratio = (high / low) ** (1.0 / 49.0)
        self.freqs = low * ratio ** np.arange(50)

        self.bp_zi = np.zeros((50, 2))
        self.bell_zi_mid = np.zeros((50, 2))
        self.bell_zi_side = np.zeros((50, 2))
        self.band_energy = np.zeros(50)

        self.bp_b = np.zeros((50, 3))
        self.bp_a = np.zeros((50, 3))
        for i in range(50):
            w0 = 2 * np.pi * self.freqs[i] / self.sr
            alpha = np.sin(w0) / (2 * 0.707)
            cw = np.cos(w0)
            b = np.array([alpha, 0.0, -alpha])
            a = np.array([1 + alpha, -2 * cw, 1 - alpha])
            self.bp_b[i] = b / a[0]
            self.bp_a[i] = a / a[0]
        self.init_done = True

    def _bell_coeffs(self, i, gain, is_side):
        cache = self._bell_cache_side if is_side else self._bell_cache_mid
        last  = self._last_gain_side if is_side else self._last_gain_mid
        if cache[i] is not None and abs(gain - last[i]) < 1e-4:
            return cache[i]

        freq = self.freqs[i]
        w0 = 2 * np.pi * freq / self.sr
        db = 20 * np.log10(max(gain, 1e-12))
        q = 4.5 - db * 0.25 if db > 0 else 4.5 + db * 0.25
        q = np.clip(q, 1.0, 8.0)
        alpha = np.sin(w0) / (2 * q)
        cw = np.cos(w0)
        b = np.array([1 + alpha * gain, -2 * cw, 1 - alpha * gain])
        a = np.array([1 + alpha / gain, -2 * cw, 1 - alpha / gain])
        coeffs = (b / a[0], a / a[0])
        cache[i] = coeffs
        last[i] = gain
        return coeffs

    def _follow_envelope(self, i, rectified):
        a1 = self.energy_alpha
        zi = [self.band_energy[i] * (1.0 - a1)]
        out, _ = lfilter([a1], [1.0, -(1.0 - a1)], rectified, zi=zi)
        self.band_energy[i] = out[-1]
        return out

    @staticmethod
    def _clamp(v, lo, hi):
        return max(lo, min(hi, v))

    @staticmethod
    def _bitrate_attenuation(freq, bit_depth):
        cutoff, slope = 22050.0, 0.0
        if bit_depth <= 16.0:
            cutoff, slope = 16000.0, -6.0
        elif bit_depth <= 20.0:
            cutoff, slope = 20000.0, -3.0
        if freq < cutoff:
            return 1.0
        return 10.0 ** (slope * np.log2(freq / cutoff) / 20.0)

    @staticmethod
    def _genre_offset(freq, genre_id):
        profiles = [
            [+2.0, -1.5, +1.0],
            [+3.0, -2.0, +2.5],
            [+1.5, +0.5, +1.5],
            [0.0, 0.0, 0.0],
        ]
        region = 2 if freq > 3000 else (1 if freq > 300 else 0)
        return profiles[int(genre_id) % 4][region]

    @staticmethod
    def _unison_weight_factor(freq, weight):
        ratio = freq / 440.0
        closest = np.round(ratio)
        detune = abs(ratio - closest)
        factor = 1.0 - 0.5 * np.exp(-10.0 * detune)
        return factor * weight + (1.0 - weight)

    def process(self, audio, asked_bands=5.0, demon_count=8,
                bit_depth=24.0, genre_id=0, unison_weight=0.6):
        if not self.init_done:
            self._init()
        if audio.ndim != 2 or audio.shape[1] != 2:
            raise ValueError("Audio must be stereo (samples × 2)")

        n = audio.shape[0]
        # FIX: clamp groups
        groups = max(1, min(12, int(round(asked_bands))))

        self.mod_phase = (self.mod_phase
                          + 2 * np.pi * self.mod_rate * n / self.sr) % (2 * np.pi)

        left  = audio[:, 0].copy()
        right = audio[:, 1].copy()
        mono  = (left + right) * 0.5

        # Analysis
        for i in range(50):
            filtered, self.bp_zi[i] = lfilter(
                self.bp_b[i], self.bp_a[i], mono, zi=self.bp_zi[i])
            self._follow_envelope(i, np.abs(filtered))

        # Grouping
        bands_per_group = 50 // groups
        remainder = 50 % groups
        group_energy = np.zeros(groups)
        group_first  = np.zeros(groups, dtype=int)
        idx = 0
        for g in range(groups):
            size = bands_per_group + (1 if g < remainder else 0)
            group_first[g] = idx
            group_energy[g] = np.mean(self.band_energy[idx:idx + size])
            idx += size

        # Demon selection
        order = np.argsort(self.band_energy)[::-1]
        selected = np.zeros(50, dtype=bool)
        selected[order[:min(50, max(0, int(demon_count)))]] = True

        # Gains
        gains = np.zeros(50)
        for i in range(50):
            freq = self.freqs[i]
            base = (self._bitrate_attenuation(freq, bit_depth)
                    * (10 ** (self._genre_offset(freq, genre_id) / 20))
                    * self._unison_weight_factor(freq, unison_weight))
            g = np.searchsorted(group_first, i, side="right") - 1
            if selected[i]:
                rel = self.band_energy[i] / (group_energy[g] + 1e-6)
                cut = 0.3 + rel * 0.5
                base *= self._clamp(cut, 0.2, 0.9)
            else:
                comp = self._clamp(0.015 / (group_energy[g] + 1e-6), 0.7, 1.4)
                base *= comp
            gains[i] = self._clamp(base, 0.7079, 1.4125)

        # Mid / side processing with independent states
        mid  = (left + right) * 0.5
        side = (left - right) * 0.5
        out_mid  = mid.copy()
        out_side = side.copy()

        for i in range(50):
            if abs(gains[i] - 1.0) > 0.001:
                b, a = self._bell_coeffs(i, gains[i], False)
                out_mid, self.bell_zi_mid[i] = lfilter(b, a, out_mid, zi=self.bell_zi_mid[i])
            sg = self._clamp(gains[i] * 1.15, 0.7079, 1.4125)
            if abs(sg - 1.0) > 0.001:
                b, a = self._bell_coeffs(i, sg, True)
                out_side, self.bell_zi_side[i] = lfilter(b, a, out_side, zi=self.bell_zi_side[i])

        left_out  = out_mid + out_side
        right_out = out_mid - out_side

        # Soft clip (unity compensated)
        drive_comp = 0.85 / 1.1045
        left_out  = np.tanh(left_out  * 1.3) * drive_comp
        right_out = np.tanh(right_out * 1.3) * drive_comp

        return np.column_stack((left_out, right_out))
