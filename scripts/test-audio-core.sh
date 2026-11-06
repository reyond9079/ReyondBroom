#!/usr/bin/env bash
# Standalone graph/DSP checks. No FTE runtime or audio device is required.
set -euo pipefail
cd "$(dirname "$0")/.."
output=${1:-/tmp/reyond-audio-tests}
converter=${2:-/usr/bin/ffmpeg}
mkdir -p "$output"
g++ -std=c++17 -O2 -pthread tests/audio-project-unit.cpp -o "$output/audio-project"
"$output/audio-project"
g++ -std=c++17 -O2 -pthread tests/audio-process-unit.cpp -o "$output/audio-process"
"$output/audio-process" "$converter"
if [[ ${OSTYPE:-} == linux* ]]; then
  g++ -std=c++17 -O2 -pthread tests/audio-capture-unit.cpp -ldl -o "$output/audio-capture"
  "$output/audio-capture"
fi
