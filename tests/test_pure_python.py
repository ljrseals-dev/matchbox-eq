import unittest

import numpy as np

from Source.Pure_python import MatchboxEQ


class MatchboxEQReferenceTests(unittest.TestCase):
    def test_gate_constants_match_cpp_contract(self):
        self.assertEqual(MatchboxEQ.GATE_TARGET, 0.015)
        self.assertEqual(MatchboxEQ.GATE_FLOOR, 0.007)
        self.assertEqual(MatchboxEQ.GATE_RAMP, 0.008)

    def test_digital_silence_stays_silent(self):
        eq = MatchboxEQ()
        audio = np.zeros((4096, 2), dtype=np.float64)
        output = eq.process(audio)
        self.assertTrue(np.all(np.isfinite(output)))
        self.assertTrue(np.array_equal(output, audio))
        self.assertEqual(eq.last_group_count, 5)

    def test_empty_block_is_safe(self):
        eq = MatchboxEQ()
        output = eq.process(np.empty((0, 2)))
        self.assertEqual(output.shape, (0, 2))
        self.assertFalse(eq.init_done)

    def test_rejects_invalid_sample_rate(self):
        for rate in (0, -1, float("nan"), float("inf")):
            with self.subTest(rate=rate):
                with self.assertRaises(ValueError):
                    MatchboxEQ(rate)

    def test_analysis_frequencies_stay_below_nyquist(self):
        for rate in (8000, 32000, 44100, 48000, 96000, 192000):
            with self.subTest(rate=rate):
                eq = MatchboxEQ(rate)
                eq.process(np.zeros((32, 2)))
                self.assertLessEqual(float(eq.freqs[-1]), rate * 0.45 + 1e-9)

    def test_non_finite_samples_and_controls_do_not_poison_state(self):
        eq = MatchboxEQ()
        audio = np.zeros((512, 2), dtype=np.float64)
        audio[3, 0] = np.nan
        audio[7, 1] = np.inf
        audio[11, 0] = -np.inf
        audio[13, :] = np.finfo(np.float64).max

        output = eq.process(
            audio,
            asked_bands=float("nan"),
            demon_count=-100,
            bit_depth=float("nan"),
            genre_id=999,
            unison_weight=float("nan"),
        )
        self.assertTrue(np.all(np.isfinite(output)))
        self.assertTrue(np.all(np.isfinite(eq.last_group_energy)))
        self.assertEqual(eq.last_group_count, 5)

    def test_quiet_signal_does_not_explode(self):
        eq = MatchboxEQ()
        time = np.arange(12000, dtype=np.float64)
        signal = 0.0002 * np.sin(0.071 * time) + 0.0001 * np.sin(0.193 * time)
        audio = np.column_stack((signal, signal * 0.9))
        output = eq.process(audio)

        input_peak = float(np.max(np.abs(audio)))
        output_peak = float(np.max(np.abs(output)))
        self.assertTrue(np.all(np.isfinite(output)))
        self.assertLessEqual(output_peak, input_peak * 2.5 + 1e-7)
        self.assertEqual(eq.last_group_energy.shape, (12,))
        self.assertEqual(eq.last_group_count, 5)

    def test_reset_reproduces_fresh_instance(self):
        reused = MatchboxEQ()
        fresh = MatchboxEQ()
        warm = np.column_stack((
            0.3 * np.sin(0.17 * np.arange(2048)),
            0.2 * np.cos(0.11 * np.arange(2048)),
        ))
        test_signal = np.column_stack((
            0.1 * np.sin(0.07 * np.arange(2048)),
            0.1 * np.cos(0.09 * np.arange(2048)),
        ))

        reused.process(warm)
        reused.reset()
        actual = reused.process(test_signal)
        expected = fresh.process(test_signal)
        np.testing.assert_allclose(actual, expected, rtol=0.0, atol=1e-10)


if __name__ == "__main__":
    unittest.main()
