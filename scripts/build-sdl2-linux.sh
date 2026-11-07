#!/usr/bin/env bash
# Build after scripts/build-linux.sh has prepared the patched engine and plugin.
set -euo pipefail
src=$(cd "$(dirname "$0")/.." && pwd)
deps=${1:?Pass the prepared dependency/build directory}
# The generic SDL target uses SDL2 without the SDL2 target's makelibs-only static dependencies.
bash "$src/scripts/build-sdl2-engine.sh" "$deps/fteqw"
python3 "$src/scripts/build-plugins.py" --output "$deps/plugins"
python3 "$src/scripts/package-linux.py" --deps "$deps" --output "${2:-$src/out/linux-sdl2}" --version "${3:-0.5.0-alpha.83}" --sdl2
