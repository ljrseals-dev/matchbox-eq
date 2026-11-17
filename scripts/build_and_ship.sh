#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"

if ! command -v cmake >/dev/null 2>&1; then
    echo "error: CMake 3.22 or newer is required" >&2
    exit 2
fi

if [[ ! -f "${ROOT}/JUCE/CMakeLists.txt" ]]; then
    echo "error: JUCE is missing at ${ROOT}/JUCE" >&2
    echo "Place the approved, pinned JUCE checkout there before building the plug-in." >&2
    exit 2
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DMATCHBOX_BUILD_PLUGIN=ON \
    -DMATCHBOX_BUILD_TESTS=ON

cmake --build "${BUILD_DIR}" --config Release --parallel
ctest --test-dir "${BUILD_DIR}" -C Release --output-on-failure

echo
echo "Build and DSP tests completed."
echo "VST3 output: ${BUILD_DIR}/MatchboxEQ_artefacts/Release/VST3/MatchboxEQ.vst3"
if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "AU output:   ${BUILD_DIR}/MatchboxEQ_artefacts/Release/AU/MatchboxEQ.component"
fi
echo "Standalone:  ${BUILD_DIR}/MatchboxEQ_artefacts/Release/Standalone/MatchboxEQ.app"
echo "No system plug-in directories were modified."
