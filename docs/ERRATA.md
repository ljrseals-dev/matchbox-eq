# MatchboxEQ technical-paper errata

The original cross-domain paper is retained as an historical document. This file records the current implementation contract where it differs from earlier paper text.

## Envelope

- The current C++ and Python implementations use a 20.8 ms envelope time constant.
- The coefficient is derived from the active sample rate: `1 - exp(-1 / (0.0208 × sample_rate))`.
- At 48 kHz the coefficient is approximately 0.001, rather than the earlier 0.04 value.

## Quiet-signal compensation gate

The current production C++ and Python implementations share these calibrated values:

| Constant | Value |
|---|---:|
| `k_target` | `0.015f` |
| `k_floor` | `0.007f` |
| `k_ramp` | `0.008f` |

Non-resonance compensation remains disengaged below the floor and blends smoothly toward its bounded target across the ramp. The calibration is intended to limit high-frequency noise-floor lift on quiet room-tone and refrigerator recordings while preserving adaptive response on program material. C++ compile-time regression assertions and Python unit tests guard the shared values.

## Frequency and parameter handling

- The 50 analysis frequencies span 40 Hz to `min(18 kHz, 0.45 × sample_rate)`.
- C++ sanitizes invalid or sub-8-kHz sample rates to 48 kHz; the Python reference rejects invalid or sub-8-kHz rates.
- Parameter bounds and non-finite sample handling are implemented in both references.
- APVTS parameter IDs are case-sensitive camelCase identifiers; `scripts/check_apvts_ids.py` checks processor declarations against editor attachments.

## Validation

The paper is not a substitute for current regression evidence. Use the C++ and Python tests, checked-in WAV fixture measurements, and DAW listening checks when assessing current behavior.
