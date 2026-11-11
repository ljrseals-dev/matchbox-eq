#!/bin/bash
set -e

echo "=== MatchboxEQ Experimental Build ==="

# 1. Fetch JUCE if missing
if [ ! -d "JUCE" ]; then
    echo "Cloning JUCE framework..."
    git clone --depth 1 https://github.com/juce-framework/JUCE.git
fi

# 2. Configure
echo "Configuring CMake (Universal Binary arm64 + x86_64)..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build
echo "Building..."
cmake --build build --config Release -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

# 4. Report
echo ""
echo "=== Build Complete ==="
echo "VST3:       build/MatchboxEQ_artefacts/Release/VST3/MatchboxEQ.vst3"
echo "AU:         build/MatchboxEQ_artefacts/Release/AU/MatchboxEQ.component"
echo "Standalone: build/MatchboxEQ_artefacts/Release/Standalone/MatchboxEQ.app"
echo ""
echo "Install VST3:"
echo "  cp -R build/MatchboxEQ_artefacts/Release/VST3/MatchboxEQ.vst3 ~/Library/Audio/Plug-Ins/VST3/"
echo "Install AU:"
echo "  cp -R build/MatchboxEQ_artefacts/Release/AU/MatchboxEQ.component ~/Library/Audio/Plug-Ins/Components/"
