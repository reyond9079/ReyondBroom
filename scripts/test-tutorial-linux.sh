#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")/.." && pwd)
runtime=${1:?Packaged Linux runtime directory}
qc_image=${2:-"$src/csaddon.dat"}
if [ -z "${DISPLAY:-}" ]; then exec xvfb-run -a -s '-screen 0 1280x800x24' bash "$0" "$runtime" "$qc_image"; fi
test=$(mktemp -d "${TMPDIR:-/tmp}/reyond-tutorial132-XXXXXX")
cp -a --reflink=auto "$runtime/." "$test/"
cp "$qc_image" "$test/ftebroom/csaddon.dat"
cp "$src/tests/tutorial132.cfg" "$test/ftebroom/"
cd "$test"
LIBGL_ALWAYS_SOFTWARE=1 timeout 55s ./fteqw-gl64 -nohome -noupdates -window -width 1280 -height 800 -condebug +vid_width 1280 +vid_height 800 +exec tutorial132.cfg > console.txt 2>&1
if rg '_FAIL|CSQC_Abort|runaway loop error|mod by zero|Outgoing message overflow' console.txt; then exit 1; fi
for name in outside_lesson_unchanged creation_demo_changes_scene creation_demo_removed_without_redo restore_is_idempotent modified_demo_restores_geometry_material_and_selection grid_stage_has_only_reference_brush tools_stage_starts_empty; do
  rg -q "FTEBROOM_TUTORIAL_PASS $name" console.txt
done
echo "PASS: $test"
