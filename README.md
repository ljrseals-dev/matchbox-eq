# MatchboxEQ — Corrected VST-Ready Edition

50-band adaptive spectral EQ with “demon” resonance hunters.

The merged implementation includes the quiet-signal compensation gate, independent mid/side filter state, corrected envelope smoothing, and a custom JUCE C++ editor using `MatchboxLookAndFeel`.

## What’s included

| File | Purpose |
|------|---------|
| `Source/MatchboxEQ.h` | Core 50-band C++ DSP engine |
| `Source/PluginProcessor.h/.cpp` | JUCE `AudioProcessor`, APVTS parameters, and processing |
| `Source/PluginEditor.h/.cpp` | Custom 800×400 editor and `MatchboxLookAndFeel` |
| `Source/Pure_python.py` | Pure-Python reference implementation |
| `CMakeLists.txt` | JUCE CMake plugin project; VST3, AU, and Standalone formats |
| `scripts/build_mac.sh` | macOS build helper; clones JUCE when it is not present |
| `docs/MatchboxEQ_Cross_Domain_Paper_Lewis_Seals.pdf` | Technical paper |

## Critical DSP fixes

1. **Quiet-signal compensation gate**

   Non-demon compensation is blended smoothly from unity instead of boosting near-silent groups indiscriminately. This is intended to prevent quiet high-frequency material—such as room tone or appliance noise—from being lifted into an audible noise-floor swell, while retaining compensation as group energy rises.

   The production C++ constants are `k_target = 0.015f`, `k_floor = 0.003f`, and `k_ramp = 0.010f`. The gate is applied to non-demon compensation; demon resonance cuts are separate.

2. **Independent mid/side bell-filter state**

   Mid and side processing use separate state arrays, avoiding shared filter history between the two paths.

3. **Band-count bounds**

   `asked_bands` is rounded and clamped to [1, 12] in C++, preventing invalid group counts and out-of-range group indexing.

4. **Slower envelope follower**

   The C++ energy follower uses `alpha = 0.001f`, reducing the rapid envelope movement associated with the former `0.04` setting.

5. **Removed unused per-sample modulation state**

   The unused `mod_phase` path has been removed; it is not part of the current DSP path.

6. **Unity-compensated soft clipping**

   The output stage uses:

   ```cpp
   constexpr float drive_comp = 0.85f / 1.1045f;
   inL = std::tanh (inL * 1.3f) * drive_comp;
   inR = std::tanh (inR * 1.3f) * drive_comp;
   ```

7. **Custom native JUCE editor**

   `PluginEditor.cpp` implements `MatchboxLookAndFeel` in C++. Rotary controls and combo boxes are drawn in code, with no external image assets. The painted VU graphic and four labeled switch shapes are decorative; they are not live metering or interactive switches.

## Building VST3 / AU

JUCE is included as a sibling source directory at `JUCE/` and is added by `CMakeLists.txt`.

### macOS — Universal Binary (Apple Silicon + Intel)

The project sets `CMAKE_OSX_ARCHITECTURES` to `arm64;x86_64` on Apple platforms.

```bash
# From the repository root
git clone --depth 1 https://github.com/juce-framework/JUCE.git
cmake -S . -B build -G Xcode -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Alternatively, run the repository helper:

```bash
./scripts/build_mac.sh
```

### Windows / Linux

Install or clone a compatible JUCE checkout into `JUCE/`, then configure and build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The configured plugin formats are VST3, AU, and Standalone; AU is a macOS format.

### Install

Copy the built VST3 bundle to the system's VST3 plug-in directory:

- **macOS:** `~/Library/Audio/Plug-Ins/VST3/`
- **Windows:** `C:\\Program Files\\Common Files\\VST3\\`
- **Linux:** commonly `~/.vst3/` (or the host's configured VST3 directory)

The macOS build helper also prints the AU component path and install command.

## Parameters

The IDs below are the APVTS identifiers used by the processor and editor attachments.

| APVTS ID | Range / choices | Default | Description |
|----------|-----------------|---------|-------------|
| `askedBands` | 1–12 | 5 | Number of conceptual groups |
| `demonCount` | 1–50 | 8 | Number of resonance-hunting demons |
| `bitDepth` | 16 / 20 / 24 | 24 | High-frequency roll-off profile |
| `genreId` | 0–3 | 0 | Neo-Classical / Cinematic Pop / Jazz-Hip-Hop / Flat |
| `unisonWeight` | 0–1 | 0.6 | Harmonic-interval weighting |
| `outputGain` | −24…+24 dB | 0 dB | Master output gain |
| `mix` | 0–1 | 1 | Dry/wet blend |

## Quiet-signal gate calibration

The production C++ gate is defined in `Source/MatchboxEQ.h`:

```cpp
constexpr float k_target = 0.015f; // Absolute-mean energy target
constexpr float k_floor  = 0.003f; // Gate floor
constexpr float k_ramp   = 0.010f; // Smooth transition width
```

The gate is calculated from each group's measured energy. Below the floor, non-demon compensation remains at unity; across the ramp it blends toward the calculated compensation.

If you change these constants, validate the result with both quiet noise-floor material (for example, room tone) and ordinary musical passages. Raising the floor or widening the ramp reduces compensation on quieter material; lowering the floor allows compensation to engage sooner. Recheck that quiet musical detail is not being suppressed and that background noise is not being raised objectionably.

**Reference implementation parity note:** `Source/Pure_python.py` currently uses `k_floor = 0.012` and `k_ramp = 0.008`. Those values do not yet match the merged C++ production settings above. Treat the C++ implementation as authoritative for the current gate calibration until the Python reference is brought into parity.

## Philosophy

Ask for 4 bands → get a 50-band matchbox.

The demons are adaptive cuts that ride the strongest resonances. Everything else is a gentle, level-aware spectral tilt.

Build it, load it, and ask it how many denons it wants.

## Academic paper

The technical paper is included at:

`docs/MatchboxEQ_Cross_Domain_Paper_Lewis_Seals.pdf`

**Title:** *MatchboxEQ: Adaptive Spectral Processing with Agent-Based Resonance Suppression — Primary Audio Applications and Cross-Domain Extensions to Computational Photography and Biomedical / Medical-Technical Signal Engineering*

**Author:** Lewis James Seals (theRealla)

**Publication note:** The paper's Section 3.3 still describes the earlier envelope coefficient `alpha = 0.04`; the current C++ implementation uses `alpha = 0.001`. Update the paper before formal publication.
