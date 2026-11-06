#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")/.." && pwd)
deps=${1:?Dependency directory}
if [ "${2:-}" != inside ]; then exec xvfb-run -a -s '-screen 0 1440x1000x24' bash "$0" "$deps" inside; fi
test=$(mktemp -d "$deps/setup61-XXXXXX")
cp -a "$src/runtime" "$test/ftebroom"
cp "$src/runtime/default.fmf" "$src/runtime/installed.lst" "$test/"
cp "$src/csaddon.dat" "$test/ftebroom/"
cp "$deps/fteqw/engine/release-linux/fteqw-gl64" "$deps/fteplug_ftebroom_amd64.so" "$test/"
cp "$src/tests/setup61.cfg" "$test/ftebroom/"
cd "$test"
openbox > wm.log 2>&1 &
wm=$!
trap 'kill "$wm" 2>/dev/null || true' EXIT
sleep 1
LIBGL_ALWAYS_SOFTWARE=1 timeout 90s ./fteqw-gl64 -nohome -noupdates -window -width 1280 -height 800 -condebug +vid_width 1280 +vid_height 800 +exec setup61.cfg > console.txt 2>&1
if grep -E '_FAIL|CSQC_Abort|runaway loop error|mod by zero|Outgoing message overflow' console.txt; then exit 1; fi
for name in setup61_game_choice setup_recommended_libre_default_without_mount setup61_libre_choice setup61_own_choice setup_recommended_manual_choice_retained setup_recommended_existing_mod_retained setup61_wizard_close setup61_resources_open setup61_resources_stays_closed; do
 grep -q "DESKTOP_PASS $name" console.txt
done
echo "PASS: $test"
