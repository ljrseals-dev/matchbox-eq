# MatchboxEQ

A 50-band adaptive spectral equalizer with resonance-hunting cuts, level-aware compensation, and a custom JUCE interface.

## Current implementation

- Framework-free C++17 DSP core in Source/MatchboxEQ.h.
- JUCE audio processor with APVTS state and mono/stereo bus support.
- Custom native editor in Source/PluginEditor.cpp.
- Offline Python reference in Source/Pure_python.py.
- C++ regression tests, Python reference tests, and checked-in WAV sample validation.

The VU graphic and four labeled panel shapes in the editor are decorative. They are not live metering or interactive transport controls.

## DSP safeguards and corrections

1. **Quiet-signal compensation gate.** Non-resonance compensation stays at unity below the group-energy floor and blends toward the calculated compensation across a smooth ramp. The production constants are k_target = 0.015, k_floor = 0.003, and k_ramp = 0.010.
2. **Continuous filter history.** Mid and side bell filters continue advancing their state at unity gain. This prevents stale state from producing a transient when adaptive gain becomes active again.
3. **Sample-rate-safe analysis.** The 50 analysis frequencies end at min(18 kHz, 0.45 × sample rate), keeping every band below Nyquist. Sample rates below 8 kHz or non-finite sample-rate inputs fall back to 48 kHz in the C++ core; the Python reference rejects them.
4. **Defensive parameter and sample handling.** Group count, demon count, genre, and unison weight are bounded. Non-finite input samples are replaced with silence so invalid values cannot poison filter state.
5. **Mono/stereo host safety.** The processor accepts matching mono or stereo input/output layouts and never assumes a second channel exists.
6. **Sample-rate-scaled envelope.** The envelope time constant is 20.8 ms, keeping its response consistent across supported sample rates.
7. **Independent DSP build and tests.** The core and its regression tests build without JUCE. The plugin is built when a JUCE checkout is supplied.

MatchboxEQ::getLastGroupCount() and getLastGroupEnergy() expose the most recently measured group energies for offline validation. These diagnostics are written by the processing thread and are **not safe to read concurrently** from a UI or other thread.

## Build and test the DSP core

A C++17 compiler and CMake 3.22 or newer are sufficient:

```sh
cmake -S . -B build -DMATCHBOX_BUILD_PLUGIN=OFF -DMATCHBOX_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the Python reference tests and checked-in sample analysis with Python 3.9+:

```sh
python -m pip install numpy scipy
python -m unittest discover -s tests -p 'test_*.py' -v
python scripts/validate_samples.py
```

The sample validator reports dry/wet RMS, peak level, and measured group_energy values for the refrigerator and reference dry WAV files. It fails on non-finite DSP output; the reported RMS ratios are measurements, not a substitute for listening tests.

## Build the JUCE plug-in

The repository does not vendor JUCE or fetch an unpinned dependency during CMake configuration. Place the exact JUCE revision you have approved in JUCE/, and record that revision in your build/release notes.

```sh
cmake -S . -B build -DMATCHBOX_BUILD_PLUGIN=ON -DMATCHBOX_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The configured formats are VST3, AU, and Standalone. AU is available on macOS. scripts/build_mac.sh configures a universal arm64;x86_64 build; scripts/build_and_ship.sh is the general JUCE build helper. Both scripts require JUCE to already exist in JUCE/ and run CTest after building. Neither script installs a plug-in into system directories.

Install the built VST3 manually:

- macOS: ~/Library/Audio/Plug-Ins/VST3/
- Windows: C:\Program Files\Common Files\VST3\
- Linux: commonly ~/.vst3/

## Parameters

| APVTS ID | Range / choices | Default | Description |
|---|---:|---:|---|
| askedBands | 1–12 | 5 | Number of conceptual groups |
| demonCount | 0–50 | 8 | Number of resonance-hunting cuts; zero disables them |
| bitDepth | 16 / 20 / 24 | 24 | High-frequency roll-off profile |
| genreId | 0–3 | 0 | Neo-Classical / Cinematic Pop / Jazz-Hip-Hop / Flat |
| unisonWeight | 0–1 | 0.6 | Harmonic-interval weighting |
| outputGain | −24…+24 dB | 0 dB | Master output gain |
| mix | 0–1 | 1 | Dry/wet blend |

## Reference implementation note

The Python implementation is an offline, block-oriented reference. It uses the same gate constants, frequency ceiling, parameter bounds, and envelope time constant as C++, but calculates one set of adaptive gains from each supplied block. The real-time C++ processor updates its adaptive gains sample-by-sample, so the two implementations are not expected to be bit-identical for arbitrary block sizes.

## Technical paper

docs/MatchboxEQ_Cross_Domain_Paper_Lewis_Seals.pdf is retained as the original paper. Its earlier envelope coefficient and gate values are superseded by the implementation described above. See docs/ERRATA.md before citing its DSP details.

## Validation status

A successful DSP-core test run does not establish that the JUCE plug-in builds or that the effect is ready for release. Release validation still requires a build against the recorded JUCE revision, DAW loading, listening tests on digital silence, quiet refrigerator/room-tone material and normal music, plus review of the sample-validator measurements.
