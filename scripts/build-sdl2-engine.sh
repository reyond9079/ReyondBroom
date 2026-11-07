#!/usr/bin/env bash
# Rebuild SDL2 so window-close events use the editor's document/compilation guard.
set -euo pipefail
src=$(cd "$(dirname "$0")/.." && pwd)
engine=${1:?Pass a dedicated engine source/build directory}
revision=f937b9d88f71fc4429db5fe56c6a98d922711b2e
if [ ! -d "$engine/.git" ]; then
  git clone --filter=blob:none --no-checkout https://github.com/fte-team/fteqw.git "$engine"
  git -C "$engine" checkout --detach "$revision"
fi
test "$(git -C "$engine" rev-parse HEAD)" = "$revision"
python3 "$src/scripts/apply-engine-patches.py" "$engine"
make -C "$engine/engine" -j4 CPUOPTIMIZATIONS=-O2 FTE_TARGET=SDL SDLCONFIG="${SDLCONFIG:-sdl2-config}" FTE_FULLTARGET=sdl2_64 RELEASE_DIR=release-sdl2 DEPCC= DEPCXX= BASE_DIR=. FTE_CONFIG_EXTRA="-DNO_SPEEX -DNO_OPUS -DNO_VORBISFILE -DNO_OGG ${RB_ENGINE_EXTRA_FLAGS:-}" LINK_FREETYPE=0 gl-rel
