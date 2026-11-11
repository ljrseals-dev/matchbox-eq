# MatchboxEQ — Shipping-Ready VST3 / AU

50-band adaptive spectral EQ with “demon” resonance hunters.  
All critical DSP bugs fixed. Custom LookAndFeel. Universal Binary ready.

## Project layout

```
MatchboxEQ_VST_Corrected/
├── CMakeLists.txt
├── README.md
├── scripts/build_mac.sh
├── Source/
│   ├── MatchboxEQ.h          ← cleaned DSP (no dead mod_phase)
│   ├── PluginProcessor.h/.cpp
│   ├── PluginEditor.h/.cpp   ← custom MatchboxLookAndFeel
│   └── Pure_python.py        ← reference implementation
└── docs/
    └── MatchboxEQ_Cross_Domain_Paper_Lewis_Seals.pdf
```

## Critical fixes (final)

| Issue | Status |
|-------|--------|
| Mid/side shared filter state | Fixed – independent arrays |
| `asked_bands` overflow / NaN | Fixed – clamped [1, 12] |
| Envelope α = 0.04 (distortion) | Fixed – α = 0.001 (~20 ms) |
| Dead `mod_phase` code | Removed |
| Soft-clip gain | Unity compensated |
| Portable `M_PI` | Present |
| UI | Custom LookAndFeel (no external assets) |

> **Note for the paper:** Section 3.3 still mentions the original α = 0.04.  
> The production code uses 0.001. Update the paper text before formal publication.

## Build (macOS Universal)

```bash
cd MatchboxEQ_VST_Corrected
./scripts/build_mac.sh
```

Or manually:

```bash
git clone --depth 1 https://github.com/juce-framework/JUCE.git
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Install:

```bash
cp -R build/MatchboxEQ_artefacts/Release/VST3/MatchboxEQ.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/
```

## Parameters

| ID | Range | Default | Description |
|----|-------|---------|-------------|
| askedBands | 1–12 | 5 | Conceptual groups |
| demonCount | 1–50 | 8 | Resonance hunters |
| bitDepth | 16/20/24 | 24 | HF roll-off profile |
| genreId | 0–3 | 0 | Genre tilt |
| unisonWeight | 0–1 | 0.6 | Harmonic weighting |
| outputGain | –24…+24 dB | 0 | Master |
| mix | 0–1 | 1 | Dry/Wet |

## Academic paper

Full ~12 800-word paper with charts is in:

`docs/MatchboxEQ_Cross_Domain_Paper_Lewis_Seals.pdf`

Author: Lewis James Seals (theRealla)

## Philosophy

Ask for 4 bands → receive a 50-band matchbox.  
The demons hunt the loudest resonances.  
The rest is a gentle, level-aware spectral tilt.

Build it. Load it. Ask how many denons it wants.

## Changelog (noise-floor fix)

- **Gate non-demon compensation below ~−50 dBFS** (`k_floor = 0.012`).
  Empty / near-empty groups are no longer boosted into the noise floor.
  This removes the ~+10 dB HF hiss that appeared on quiet material
  (home fridge, room tone, soft pads). Demon cuts are unchanged.
  Music-level material continues to receive the original balancing behaviour.
