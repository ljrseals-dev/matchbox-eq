# MatchboxEQ — Corrected VST-Ready Edition

50-band adaptive spectral EQ with “demon” resonance hunters.  
All critical bugs from the original analysis have been fixed.

## What’s included

| File | Purpose |
|------|---------|
| `Source/MatchboxEQ.h` | Core DSP engine (C++) – drop-in for JUCE |
| `Source/PluginProcessor.h/.cpp` | JUCE AudioProcessor + APVTS |
| `Source/PluginEditor.h/.cpp` | Simple 800×400 GUI |
| `Source/Pure_python.py` | Corrected reference Python implementation |
| `CMakeLists.txt` | Universal Binary (arm64 + x86_64) build |

## Critical fixes applied

1. **Mid/side bell states are now independent**  
   Separate `bell_*_mid[]` and `bell_*_side[]` arrays → no more M/S crosstalk.

2. **`asked_bands` clamped to [1, 12]**  
   Prevents buffer overflow (C++) and NaN (Python).

3. **Envelope follower slowed to α = 0.001 (~20 ms)**  
   Eliminates rectifier-ripple distortion on bass bands (was α = 0.04 ≈ 0.5 ms).

4. **Per-sample frequency modulation removed**  
   Was inaudible but forced constant coefficient recalculation.

5. **Soft-clip is unity-gain compensated**  
   `tanh(x·1.3) · (0.85 / 1.1045)`.

6. **Portable `M_PI`** and proper coefficient caching in Python.

## Building the VST3 / AU (macOS Universal)

```bash
# 1. Clone JUCE next to this folder
git clone https://github.com/juce-framework/JUCE.git

# 2. Configure & build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 3. Result
# build/MatchboxEQ_artefacts/VST3/MatchboxEQ.vst3
# build/MatchboxEQ_artefacts/AU/MatchboxEQ.component
```

Copy the `.vst3` bundle to:
- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Windows: `C:\Program Files\Common Files\VST3\`

## Parameters

| ID | Range | Default | Description |
|----|-------|---------|-------------|
| `askedBands` | 1–12 | 5 | How many conceptual groups |
| `demonCount` | 1–50 | 8 | How many resonance-hunting demons |
| `bitDepth` | 16/20/24 | 24 | High-frequency roll-off profile |
| `genreId` | 0–3 | 0 | Neo-Classical / Cinematic Pop / Jazz-Hip-Hop / Flat |
| `unisonWeight` | 0–1 | 0.6 | Harmonic-interval weighting |
| `outputGain` | –24…+24 dB | 0 | Master output |
| `mix` | 0–1 | 1 | Dry/Wet |

## Philosophy

Ask for 4 bands → get a 50-band matchbox.  
The demons are adaptive cuts that ride the loudest resonances.  
Everything else is a gentle, level-aware spectral tilt.

Build it, load it, and ask it how many denons it wants.

## Academic Paper

A full technical paper (~12,800 words) is included:

**`docs/MatchboxEQ_Cross_Domain_Paper_Lewis_Seals.pdf`**

Title: *MatchboxEQ: Adaptive Spectral Processing with Agent-Based Resonance Suppression — Primary Audio Applications and Cross-Domain Extensions to Computational Photography and Biomedical / Medical-Technical Signal Engineering*

Author: Lewis James Seals (theRealla)

The paper covers the algorithm, evaluation, and extensions to photography and medical/tech engineering, with charts and spectrograms.
