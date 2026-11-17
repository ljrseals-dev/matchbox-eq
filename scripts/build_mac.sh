#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build-macos-universal"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: build_mac.sh must run on macOS" >&2
    exit 2
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "error: CMake 3.22 or newer is required" >&2
    exit 2
fi

if [[ ! -f "${ROOT}/JUCE/CMakeLists.txt" ]]; then
    echo "error: JUCE is missing at ${ROOT}/JUCE" >&2
    echo "Place the approved, pinned JUCE checkout there before building the plug-in." >&2
    exit 2
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Xcode \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DMATCHBOX_BUILD_PLUGIN=ON \
    -DMATCHBOX_BUILD_TESTS=ON

cmake --build "${BUILD_DIR}" --config Release --parallel
ctest --test-dir "${BUILD_DIR}" -C Release --output-on-failure

echo
echo "Universal macOS build and DSP tests completed."
echo "VST3: build-macos-universal/MatchboxEQ_artefacts/Release/VST3/MatchboxEQ.vst3"
echo "AU:   build-macos-universal/MatchboxEQ_artefacts/Release/AU/MatchboxEQ.component"
