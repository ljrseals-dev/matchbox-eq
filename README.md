# MatchboxEQ

**MatchboxEQ** is a 50-band adaptive spectral equalizer with resonance-focused attenuation, level-aware compensation, mid/side processing, and a custom JUCE interface.

The project contains a framework-free C++17 DSP core, a JUCE audio plug-in, an offline Python reference implementation, regression tests, and checked-in audio fixtures.

## Features

- 50 logarithmically spaced analysis bands, bounded by 18 kHz and 45% of the active sample rate.
- Configurable grouping (1–12 groups) and deterministic top-N resonance attenuation (0–50 cuts).
- Quiet-signal compensation with a smooth engagement gate, calibrated to avoid lifting room tone and refrigerator noise prematurely.
- Independent mid/side filter histories and continuous filter-state advancement.
- Bit-depth and genre tonal profiles, unison weighting, output gain, and dry/wet mix.
- Mono and stereo host layouts.
- Custom `MatchboxLookAndFeel` rotary controls and combo boxes.
- C++17 DSP core that can be built and tested without JUCE.
- Python reference for offline analysis and sample regression.

## Quiet-signal gate calibration

The production C++ core and Python reference use the same constants:

| Constant | Value | Purpose |
|---|---:|---|
| `k_target` | `0.015f` | Target group envelope used by compensation |
| `k_floor` | `0.007f` | Level below which compensation remains disengaged |
| `k_ramp` | `0.008f` | Smooth transition width above the floor |

The gate is applied to non-resonance compensation. Below the floor its contribution is unity; above the floor it blends progressively toward the bounded compensation value. This calibration is intended to reduce high-frequency noise-floor lift on quiet room-tone and refrigerator material while retaining adaptive behavior on musical content.

The values are defined once in the C++ class contract and mirrored by `MatchboxEQ.GATE_TARGET`, `GATE_FLOOR`, and `GATE_RAMP` in the Python reference. Regression tests assert the shared calibration.

## Parameter contract

The JUCE AudioProcessorValueTreeState (APVTS) IDs are case-sensitive and intentionally use camelCase.

| APVTS ID | Range / choices | Default | Description |
|---|---:|---:|---|
| `askedBands` | 1–12 | 5 | Number of analysis groups |
| `demonCount` | 0–50 | 8 | Number of resonance-focused cuts |
| `bitDepth` | 16 / 20 / 24 | 24 | High-frequency roll-off profile |
| `genreId` | 0–3 | 0 | Neo-Classical / Cinematic Pop / Jazz-Hip-Hop / Flat |
| `unisonWeight` | 0–1 | 0.6 | Harmonic-interval weighting |
| `outputGain` | −24…+24 dB | 0 dB | Output trim |
| `mix` | 0–1 | 1 | Dry/wet blend |

The editor attachments use these exact IDs. `scripts/check_apvts_ids.py` checks that the processor declarations and editor attachments remain in sync.

## Repository layout

- `Source/MatchboxEQ.h` — framework-free real-time DSP core.
- `Source/PluginProcessor.h/.cpp` — JUCE processor, APVTS state, bus handling, and audio callback.
- `Source/PluginEditor.h/.cpp` — custom rotary and combo-box look-and-feel and editor layout.
- `Source/Pure_python.py` — offline block-oriented reference.
- `tests/` — C++ and Python regression tests.
- `scripts/validate_samples.py` — checked-in WAV fixture analysis and group-energy report.
- `scripts/check_apvts_ids.py` — processor/editor parameter-ID consistency check.
- `docs/ERRATA.md` — implementation corrections to the original technical paper.
- `samples/` — dry and processed WAV fixtures.

## Build and test the framework-free DSP core

Requirements: C++17 compiler, CMake 3.22+, Python 3.9+, NumPy, and SciPy.

### Generic CMake (Linux, Windows, and macOS)

```sh
cmake -S . -B build \
  -DMATCHBOX_BUILD_PLUGIN=OFF \
  -DMATCHBOX_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Python reference and fixture validation

```sh
python -m pip install numpy scipy
python -m unittest discover -s tests -p 'test_*.py' -v
python scripts/check_apvts_ids.py
python scripts/validate_samples.py
```

The sample validator reports dry/wet RMS, peak levels, and measured group energies for the included refrigerator and reference recordings. It rejects non-finite results and emits strict JSON. Measurements are reproducible diagnostics; listening remains part of audio acceptance.

## Build the JUCE plug-in

JUCE is deliberately not vendored and is not fetched during CMake configuration. Put the approved JUCE checkout at `JUCE/` and record its exact revision in the build/release record.

### Generic CMake plug-in build

```sh
cmake -S . -B build-plugin \
  -DMATCHBOX_BUILD_PLUGIN=ON \
  -DMATCHBOX_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-plugin --config Release --parallel
ctest --test-dir build-plugin -C Release --output-on-failure
```

The configured formats are VST3 and Standalone. AU is included on macOS.

### macOS universal Xcode build

On macOS with Xcode, CMake, and JUCE present at `JUCE/`:

```sh
cmake -S . -B build-macos-universal -G Xcode \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DMATCHBOX_BUILD_PLUGIN=ON \
  -DMATCHBOX_BUILD_TESTS=ON
cmake --build build-macos-universal --config Release --parallel
ctest --test-dir build-macos-universal -C Release --output-on-failure
```

The equivalent helper is `scripts/build_mac.sh`. The general plug-in helper is `scripts/build_and_ship.sh`. Both require the pre-existing JUCE checkout and run CTest; neither installs plug-ins into system directories.

### Manual VST3 installation locations

- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Windows: `C:\Program Files\Common Files\VST3\`
- Linux: commonly `~/.vst3/`

## DSP and host behavior

- The envelope time constant is 20.8 ms and its coefficient is derived from the active sample rate.
- Analysis frequencies stay below Nyquist with a 0.45 × sample-rate ceiling.
- Non-finite samples are sanitized before they can contaminate filter state or the dry/wet output.
- Input amplitudes are bounded before DSP processing.
- Filter histories continue advancing when a band is at unity gain, avoiding stale-state transitions.
- Group-energy diagnostics are intended for same-thread/offline inspection; they are not safe for concurrent UI reads.

The Python implementation is an offline block-oriented reference. It shares the gate constants, frequency ceiling, parameter bounds, and envelope time constant with C++, but calculates adaptive gains from each supplied block. The real-time C++ implementation updates gains sample-by-sample, so arbitrary block sizes are not expected to be bit-identical.

## Technical paper

The original cross-domain paper is retained at `docs/MatchboxEQ_Cross_Domain_Paper_Lewis_Seals.pdf`. Its historical DSP values are superseded by the implementation contract documented in `docs/ERRATA.md`.

## Validation

The automated workflow builds the framework-free C++ core, runs C++ and Python regression tests, checks APVTS/editor parameter parity, and validates the checked-in WAV fixtures. A JUCE plug-in build additionally requires the approved JUCE checkout. Plug-in host loading and listening checks should be performed in a DAW with digital silence, quiet room-tone/refrigerator material, and normal music before a release is declared.
