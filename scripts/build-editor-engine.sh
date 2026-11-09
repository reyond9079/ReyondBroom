#!/usr/bin/env bash
# Rebuild the pinned engine with the complete editor patch stack.
set -euo pipefail
src=$(cd "$(dirname "$0")/.." && pwd)
platform=${1:?Pass Linux-x64, Linux-SDL2-x64 or Windows-x64}
engine=${2:?Pass a dedicated engine source/build directory}
case "$platform" in
  Linux-x64|Linux-SDL2-x64|Windows-x64) ;;
  *) printf 'Unsupported engine platform: %s\n' "$platform" >&2; exit 2 ;;
esac
revision=f937b9d88f71fc4429db5fe56c6a98d922711b2e
if [ ! -d "$engine/.git" ]; then
  git -c core.autocrlf=false clone --filter=blob:none --no-checkout https://github.com/fte-team/fteqw.git "$engine"
  git -c core.autocrlf=false -C "$engine" checkout --detach "$revision"
fi
test "$(git -C "$engine" rev-parse HEAD)" = "$revision"
python_bin=python3
if [ "$platform" = Windows-x64 ]; then python_bin=python; fi
"$python_bin" "$src/scripts/apply-engine-patches.py" "$engine"
"$python_bin" "$src/tests/check-editor-resource-stack.py" "$engine"
"$python_bin" "$src/tests/check-windows-mouse-activation.py" "$engine"
if [ "$platform" = Linux-SDL2-x64 ]; then
  bash "$src/scripts/build-sdl2-engine.sh" "$engine"
else
  target=linux64
  build_make=make
  if [ "$platform" = Windows-x64 ]; then
    target=win64
    build_make=mingw32-make
  fi
  "$build_make" -B -C "$engine/engine" -j4 CPUOPTIMIZATIONS=-O3 DEPCC= DEPCXX= BASE_DIR=. RELEASE_DIR=release-editor FTE_TARGET="$target" FTE_CONFIG_EXTRA="-DNO_SPEEX -DNO_OPUS -DNO_VORBISFILE -DNO_OGG ${RB_ENGINE_EXTRA_FLAGS:-}" LINK_FREETYPE=0 gl-rel
fi
