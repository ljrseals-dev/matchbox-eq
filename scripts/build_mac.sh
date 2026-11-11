#!/bin/bash
set -e

echo "=== MatchboxEQ Build Pipeline ==="

if [ ! -d "JUCE" ]; then
    echo "Cloning JUCE framework..."
    git clone --depth 1 https://github.com/juce-framework/JUCE.git
fi

echo "Configuring CMake (Universal Binary)..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

echo "Building..."
cmake --build build --config Release -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=== Build Complete ==="
echo "VST3:       build/MatchboxEQ_artefacts/Release/VST3/MatchboxEQ.vst3"
echo "AU:         build/MatchboxEQ_artefacts/Release/AU/MatchboxEQ.component"
echo "Standalone: build/MatchboxEQ_artefacts/Release/Standalone/MatchboxEQ.app"
echo ""
echo "Install VST3:  cp -R build/MatchboxEQ_artefacts/Release/VST3/MatchboxEQ.vst3 ~/Library/Audio/Plug-Ins/VST3/"
echo "Install AU:    cp -R build/MatchboxEQ_artefacts/Release/AU/MatchboxEQ.component ~/Library/Audio/Plug-Ins/Components/"
