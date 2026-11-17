#!/usr/bin/env python3
"""Offline Python reference for MatchboxEQ.

The C++ processor updates adaptive gains sample-by-sample. This reference
processes each supplied block with the block's final envelope estimate, so it
is intended for analysis and regression checks rather than bit-exact parity.
"""
import math

import numpy as np
from scipy.signal import lfilter


class MatchboxEQ:
    GATE_TARGET = 0.015
    GATE_FLOOR = 0.007
    GATE_RAMP = 0.008
    ENERGY_TIME_CONSTANT_SECONDS = 0.0208

    def __init__(self, sample_rate=48000):
        try:
            sample_rate = float(sample_rate)
        except (TypeError, ValueError):
            raise ValueError("sample_rate must be a finite value >= 8000 Hz")

        if not math.isfinite(sample_rate) or sample_rate < 8000.0:
            raise ValueError("sample_rate must be a finite value >= 8000 Hz")

        self.sr = sample_rate
        self.init_done = False
        self.energy_alpha = 1.0 - math.exp(
            -1.0 / (self.ENERGY_TIME_CONSTANT_SECONDS * self.sr)
        )
        self.freqs = None
        self.bp_b = self.bp_a = None
        self.bp_zi = self.bell_zi_mid = self.bell_zi_side = None
        self.band_energy = None
        self.last_group_energy = np.zeros(12, dtype=np.float64)
        self.last_group_count = 0
        self._bell_cache_mid = [None] * 50
        self._bell_cache_side = [None] * 50
        self._last_gain_mid = np.ones(50)
        self._last_gain_side = np.ones(50)

    def reset(self):
        """Clear filter/envelope history without changing configuration."""
        if not self.init_done:
            return
        self.bp_zi.fill(0.0)
        self.bell_zi_mid.fill(0.0)
        self.bell_zi_side.fill(0.0)
        self.band_energy.fill(0.0)
        self.last_group_energy.fill(0.0)
        self.last_group_count = 0

    def _init(self):
        low = 40.0
        high = min(18000.0, 0.45 * self.sr)
        ratio = (high / low) ** (1.0 / 49.0)
        self.freqs = low * ratio ** np.arange(50)
        self.bp_zi = np.zeros((50, 2), dtype=np.float64)
        self.bell_zi_mid = np.zeros((50, 2), dtype=np.float64)
        self.bell_zi_side = np.zeros((50, 2), dtype=np.float64)
        self.band_energy = np.zeros(50, dtype=np.float64)
        self.bp_b = np.zeros((50, 3), dtype=np.float64)
        self.bp_a = np.zeros((50, 3), dtype=np.float64)

        for i in range(50):
            w0 = 2.0 * np.pi * self.freqs[i] / self.sr
            alpha = np.sin(w0) / (2.0 * 0.707)
            cosine = np.cos(w0)
            b = np.array([alpha, 0.0, -alpha])
            a = np.array([1.0 + alpha, -2.0 * cosine, 1.0 - alpha])
            self.bp_b[i] = b / a[0]
            self.bp_a[i] = a / a[0]

        self.init_done = True

    def _bell_coeffs(self, i, gain, is_side):
        cache = self._bell_cache_side if is_side else self._bell_cache_mid
        last = self._last_gain_side if is_side else self._last_gain_mid

        if cache[i] is not None and abs(gain - last[i]) < 1e-4:
            return cache[i]

        w0 = 2.0 * np.pi * self.freqs[i] / self.sr
        db = 20.0 * np.log10(max(float(gain), 1e-12))
        q = np.clip(4.5 - db * 0.25 if db > 0.0 else 4.5 + db * 0.25, 1.0, 8.0)
        alpha = np.sin(w0) / (2.0 * q)
        cosine = np.cos(w0)

        b = np.array([
            1.0 + alpha * gain,
            -2.0 * cosine,
            1.0 - alpha * gain,
        ])
        a = np.array([
            1.0 + alpha / gain,
            -2.0 * cosine,
            1.0 - alpha / gain,
        ])
        coeffs = (b / a[0], a / a[0])
        cache[i] = coeffs
        last[i] = gain
        return coeffs

    @staticmethod
    def _finite_float(value, fallback):
        try:
            value = float(value)
        except (TypeError, ValueError):
            return fallback
        return value if math.isfinite(value) else fallback

    def process(
        self,
        audio,
        asked_bands=5.0,
        demon_count=8,
        bit_depth=24.0,
        genre_id=0,
        unison_weight=0.6,
    ):
        audio = np.asarray(audio, dtype=np.float64)
        if audio.ndim != 2 or audio.shape[1] != 2:
            raise ValueError("audio must have shape (frames, 2) for stereo")

        if audio.shape[0] == 0:
            return np.empty((0, 2), dtype=np.float64)

        if not self.init_done:
            self._init()

        # Match the C++ defensive input handling; NaN/Inf must not poison state.
        audio = np.where(np.isfinite(audio), audio, 0.0)
        np.clip(audio, -64.0, 64.0, out=audio)
        asked = np.clip(self._finite_float(asked_bands, 5.0), 1.0, 12.0)
        groups = max(1, min(12, int(math.floor(asked + 0.5))))

        try:
            demons = int(demon_count)
        except (TypeError, ValueError, OverflowError):
            demons = 8
        demons = max(0, min(50, demons))

        bit_value = self._finite_float(bit_depth, 24.0)
        if bit_value <= 16.0:
            bit_value = 16.0
        elif bit_value <= 20.0:
            bit_value = 20.0
        else:
            bit_value = 24.0

        try:
            genre = int(genre_id)
        except (TypeError, ValueError, OverflowError):
            genre = 0
        genre = max(0, min(3, genre))

        unison = np.clip(self._finite_float(unison_weight, 0.6), 0.0, 1.0)

        left = audio[:, 0].copy()
        right = audio[:, 1].copy()
        mono = (left + right) * 0.5

        for i in range(50):
            filtered, self.bp_zi[i] = lfilter(
                self.bp_b[i], self.bp_a[i], mono, zi=self.bp_zi[i]
            )
            a1 = self.energy_alpha
            zi = [self.band_energy[i] * (1.0 - a1)]
            envelope, _ = lfilter(
                [a1], [1.0, -(1.0 - a1)], np.abs(filtered), zi=zi
            )
            self.band_energy[i] = envelope[-1]

        bands_per_group, remainder = divmod(50, groups)
        group_energy = np.zeros(groups, dtype=np.float64)
        group_first = np.zeros(groups, dtype=int)
        index = 0

        for group in range(groups):
            size = bands_per_group + (1 if group < remainder else 0)
            group_first[group] = index
            group_energy[group] = np.mean(self.band_energy[index:index + size])
            index += size

        self.last_group_energy.fill(0.0)
        self.last_group_energy[:groups] = group_energy
        self.last_group_count = groups

        order = np.argsort(-self.band_energy, kind="stable")
        selected = np.zeros(50, dtype=bool)
        selected[order[:demons]] = True

        gains = np.zeros(50, dtype=np.float64)
        profiles = (
            (2.0, -1.5, 1.0),
            (3.0, -2.0, 2.5),
            (1.5, 0.5, 1.5),
            (0.0, 0.0, 0.0),
        )

        for i in range(50):
            freq = self.freqs[i]
            cutoff, slope = 22050.0, 0.0
            if bit_value == 16.0:
                cutoff, slope = 16000.0, -6.0
            elif bit_value == 20.0:
                cutoff, slope = 20000.0, -3.0

            bit_attenuation = (
                1.0 if freq < cutoff
                else 10.0 ** (slope * np.log2(freq / cutoff) / 20.0)
            )

            region = 2 if freq > 3000.0 else (1 if freq > 300.0 else 0)
            genre_attenuation = 10.0 ** (profiles[genre][region] / 20.0)

            ratio = freq / 440.0
            closest = np.floor(ratio + 0.5)
            detune = abs(ratio - closest)
            unison_factor = (
                (1.0 - 0.5 * np.exp(-10.0 * detune)) * unison
                + (1.0 - unison)
            )
            base = bit_attenuation * genre_attenuation * unison_factor

            group = np.searchsorted(group_first, i, side="right") - 1
            if selected[i]:
                relative_energy = self.band_energy[i] / (group_energy[group] + 1e-6)
                base *= np.clip(0.3 + relative_energy * 0.5, 0.2, 0.9)
            else:
                raw_comp = np.clip(
                    self.GATE_TARGET / (group_energy[group] + 1e-6),
                    0.7,
                    1.4,
                )
                gate = np.clip(
                    (group_energy[group] - self.GATE_FLOOR) / self.GATE_RAMP,
                    0.0,
                    1.0,
                )
                base *= 1.0 + gate * (raw_comp - 1.0)

            gains[i] = np.clip(base, 0.7079, 1.4125)

        mid = (left + right) * 0.5
        side = (left - right) * 0.5
        out_mid = mid.copy()
        out_side = side.copy()

        for i in range(50):
            # Process unity-gain sections too, keeping filter history current.
            b, a = self._bell_coeffs(i, gains[i], False)
            out_mid, self.bell_zi_mid[i] = lfilter(
                b, a, out_mid, zi=self.bell_zi_mid[i]
            )

            side_gain = np.clip(gains[i] * 1.15, 0.7079, 1.4125)
            b, a = self._bell_coeffs(i, side_gain, True)
            out_side, self.bell_zi_side[i] = lfilter(
                b, a, out_side, zi=self.bell_zi_side[i]
            )

        drive_comp = 0.85 / 1.1045
        left_out = np.tanh((out_mid + out_side) * 1.3) * drive_comp
        right_out = np.tanh((out_mid - out_side) * 1.3) * drive_comp
        output = np.column_stack((left_out, right_out))
        return np.where(np.isfinite(output), output, 0.0)
