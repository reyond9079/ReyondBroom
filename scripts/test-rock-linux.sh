#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")/.." && pwd)
deps=${1:?Dependency directory}
if [ "${2:-}" != inside ]; then exec xvfb-run -a -s '-screen 0 1440x1000x24' bash "$0" "$deps" inside; fi
test=$(mktemp -d "$deps/rock45-XXXXXX")
cp -a "$src/runtime" "$test/ftebroom"
cp "$src/runtime/default.fmf" "$src/runtime/installed.lst" "$test/"
cp "$src/csaddon.dat" "$test/ftebroom/"
cp "$deps/fteqw/engine/release-linux/fteqw-gl64" "$deps/fteplug_ftebroom_amd64.so" "$test/"
cp "$src/tests/rock45.cfg" "$test/ftebroom/"
cd "$test"
LIBGL_ALWAYS_SOFTWARE=1 timeout 90s ./fteqw-gl64 -nohome -noupdates -window -condebug +exec rock45.cfg > console.txt 2>&1
if grep -E '_FAIL|CSQC_Abort|runaway loop error|mod by zero|Outgoing message overflow' console.txt; then exit 1; fi
for name in rock45_solid_valid rock45_single_undo rock45_seed_repeat rock45_detail_convex_partition rock45_hollow_shell rock45_group_atomic rock45_cancel_restores_source sculpt45_raise_neighbours sculpt45_shared_vertices_and_back_fixed sculpt45_single_undo sculpt45_smooth sculpt45_prevent_collapse sculpt45_tool_change_cancels array45_centered_offsets array45_same_side_offsets array45_cross_lines array45_multiple_rings array45_random_disk array45_facing_preserves_center array45_ring_geometry array45_single_undo array45_budget_atomic; do
 grep -q "TRANSFORM_PASS $name" console.txt
done
grep -q 'FTEBROOM_GEOM_COMPLETE' console.txt
for name in sculpt46_raise_face_center_pointer sculpt46_lower_face_center_pointer sculpt46_pointer_strokes_undo; do
 grep -q "TRANSFORM_PASS $name" console.txt
done
echo "PASS: $test"
