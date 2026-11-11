#!/usr/bin/env python3
"""MatchboxEQ – corrected pure-Python reference (shipping)."""
import numpy as np
from scipy.signal import lfilter

class MatchboxEQ:
    def __init__(self, sample_rate=48000):
        self.sr = sample_rate
        self.init_done = False
        self.energy_alpha = 0.001
        self.freqs = None
        self.bp_b = self.bp_a = None
        self.bp_zi = self.bell_zi_mid = self.bell_zi_side = None
        self.band_energy = None
        self._bell_cache_mid = [None]*50
        self._bell_cache_side = [None]*50
        self._last_gain_mid = np.ones(50)
        self._last_gain_side = np.ones(50)

    def _init(self):
        low, high = 40.0, 18000.0
        ratio = (high/low)**(1.0/49.0)
        self.freqs = low * ratio ** np.arange(50)
        self.bp_zi = np.zeros((50, 2))
        self.bell_zi_mid = np.zeros((50, 2))
        self.bell_zi_side = np.zeros((50, 2))
        self.band_energy = np.zeros(50)
        self.bp_b = np.zeros((50, 3))
        self.bp_a = np.zeros((50, 3))
        for i in range(50):
            w0 = 2*np.pi*self.freqs[i]/self.sr
            alpha = np.sin(w0)/(2*0.707)
            cw = np.cos(w0)
            b = np.array([alpha, 0.0, -alpha])
            a = np.array([1+alpha, -2*cw, 1-alpha])
            self.bp_b[i] = b/a[0]
            self.bp_a[i] = a/a[0]
        self.init_done = True

    def _bell_coeffs(self, i, gain, is_side):
        cache = self._bell_cache_side if is_side else self._bell_cache_mid
        last = self._last_gain_side if is_side else self._last_gain_mid
        if cache[i] is not None and abs(gain-last[i]) < 1e-4:
            return cache[i]
        freq = self.freqs[i]
        w0 = 2*np.pi*freq/self.sr
        db = 20*np.log10(max(gain, 1e-12))
        q = np.clip(4.5 - db*0.25 if db > 0 else 4.5 + db*0.25, 1.0, 8.0)
        alpha = np.sin(w0)/(2*q)
        cw = np.cos(w0)
        b = np.array([1+alpha*gain, -2*cw, 1-alpha*gain])
        a = np.array([1+alpha/gain, -2*cw, 1-alpha/gain])
        coeffs = (b/a[0], a/a[0])
        cache[i] = coeffs
        last[i] = gain
        return coeffs

    def process(self, audio, asked_bands=5.0, demon_count=8,
                bit_depth=24.0, genre_id=0, unison_weight=0.6):
        if not self.init_done: self._init()
        if audio.ndim != 2 or audio.shape[1] != 2:
            raise ValueError("stereo required")
        n = audio.shape[0]
        groups = max(1, min(12, int(round(asked_bands))))
        left, right = audio[:,0].copy(), audio[:,1].copy()
        mono = (left+right)*0.5

        for i in range(50):
            filtered, self.bp_zi[i] = lfilter(self.bp_b[i], self.bp_a[i], mono, zi=self.bp_zi[i])
            a1 = self.energy_alpha
            zi = [self.band_energy[i]*(1-a1)]
            out, _ = lfilter([a1], [1.0, -(1-a1)], np.abs(filtered), zi=zi)
            self.band_energy[i] = out[-1]

        bpg, rem = 50//groups, 50%groups
        group_energy = np.zeros(groups)
        group_first = np.zeros(groups, dtype=int)
        idx = 0
        for g in range(groups):
            size = bpg + (1 if g < rem else 0)
            group_first[g] = idx
            group_energy[g] = np.mean(self.band_energy[idx:idx+size])
            idx += size

        order = np.argsort(self.band_energy)[::-1]
        selected = np.zeros(50, dtype=bool)
        selected[order[:min(50, max(0, int(demon_count)))]] = True

        gains = np.zeros(50)
        for i in range(50):
            freq = self.freqs[i]
            # intelligence
            cutoff, slope = 22050.0, 0.0
            if bit_depth <= 16: cutoff, slope = 16000.0, -6.0
            elif bit_depth <= 20: cutoff, slope = 20000.0, -3.0
            bit_a = 1.0 if freq < cutoff else 10**(slope*np.log2(freq/cutoff)/20)
            profiles = [[2,-1.5,1],[3,-2,2.5],[1.5,0.5,1.5],[0,0,0]]
            region = 2 if freq > 3000 else (1 if freq > 300 else 0)
            genre_a = 10**(profiles[int(genre_id)%4][region]/20)
            ratio = freq/440.0
            detune = abs(ratio - np.round(ratio))
            uni = (1-0.5*np.exp(-10*detune))*unison_weight + (1-unison_weight)
            base = bit_a * genre_a * uni
            g = np.searchsorted(group_first, i, side='right')-1
            if selected[i]:
                rel = self.band_energy[i]/(group_energy[g]+1e-6)
                base *= np.clip(0.3+rel*0.5, 0.2, 0.9)
            else:
                # Gate compensation so quiet/empty groups are not boosted into noise
                k_target, k_floor, k_ramp = 0.015, 0.012, 0.008
                raw_comp = np.clip(k_target / (group_energy[g] + 1e-6), 0.7, 1.4)
                gate = np.clip((group_energy[g] - k_floor) / k_ramp, 0.0, 1.0)
                comp = 1.0 + gate * (raw_comp - 1.0)
                base *= comp
            gains[i] = np.clip(base, 0.7079, 1.4125)

        mid = (left+right)*0.5
        side = (left-right)*0.5
        out_mid, out_side = mid.copy(), side.copy()
        for i in range(50):
            if abs(gains[i]-1) > 0.001:
                b,a = self._bell_coeffs(i, gains[i], False)
                out_mid, self.bell_zi_mid[i] = lfilter(b, a, out_mid, zi=self.bell_zi_mid[i])
            sg = np.clip(gains[i]*1.15, 0.7079, 1.4125)
            if abs(sg-1) > 0.001:
                b,a = self._bell_coeffs(i, sg, True)
                out_side, self.bell_zi_side[i] = lfilter(b, a, out_side, zi=self.bell_zi_side[i])

        left_out = out_mid + out_side
        right_out = out_mid - out_side
        comp = 0.85 / 1.1045
        left_out = np.tanh(left_out*1.3)*comp
        right_out = np.tanh(right_out*1.3)*comp
        return np.column_stack((left_out, right_out))
