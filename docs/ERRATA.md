# MatchboxEQ technical-paper errata

The included cross-domain paper is preserved as the original authored document. The following implementation corrections supersede the corresponding historical descriptions in the paper.

## DSP envelope

- The current C++ envelope time constant is 20.8 ms.
- The smoothing coefficient is derived from the active sample rate: 1 - exp(-1 / (0.0208 × sample_rate)).
- At 48 kHz this is approximately 0.001, not the earlier 0.04 value described in Section 3.3.

## Quiet-signal compensation gate

The current production C++ and Python reference values are:

| Constant | Value |
|---|---:|
| k_target | 0.015 |
| k_floor | 0.003 |
| k_ramp | 0.010 |

The earlier Python values k_floor = 0.012 and k_ramp = 0.008 are obsolete.

## Validation

The paper's claims should not be treated as evidence of release readiness. Use the repository's current C++ tests, Python tests, checked-in WAV sample measurements, DAW validation, and listening tests for current behavior.
