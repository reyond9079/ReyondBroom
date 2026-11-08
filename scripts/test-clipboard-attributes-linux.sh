#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")/.." && pwd)
runtime=${1:?Packaged Linux runtime directory}
qc_image=${2:-"$src/csaddon.dat"}
if [ -z "${DISPLAY:-}" ]; then exec xvfb-run -a -s '-screen 0 1280x800x24' bash "$0" "$runtime" "$qc_image"; fi
test=$(mktemp -d "${TMPDIR:-/tmp}/reyond-clipboard-attributes132-XXXXXX")
cp -a --reflink=auto "$runtime/." "$test/"
cp "$qc_image" "$test/ftebroom/csaddon.dat"
cp "$src/tests/clipboard-attributes132.cfg" "$test/ftebroom/"
cd "$test"
LIBGL_ALWAYS_SOFTWARE=1 timeout 50s ./fteqw-gl64 -nohome -noupdates -window -width 1280 -height 800 -condebug +vid_width 1280 +vid_height 800 +exec clipboard-attributes132.cfg > console.txt 2>&1
if rg '_FAIL|CSQC_Abort|runaway loop error|mod by zero|Outgoing message overflow' console.txt; then exit 1; fi
for name in face_fragment_roundtrip_for_brush_paste whole_brush_receives_all_attributes_preserving_planes_selection brush_paste_single_undo_restores_full_before brush_paste_redo_restores_full_after face_fragment_roundtrip_for_face_paste selected_faces_only_receive_all_attributes face_paste_single_undo_restores_full_before face_paste_redo_restores_full_after no_selection_fails_without_geometry_or_history_changes external_face_without_optional_attributes_parses external_face_defaults_to_solid_contents external_face_paste_keeps_brush_solid; do
  rg -q "FTEBROOM_CLIPATTRS_PASS $name" console.txt
done
echo "PASS: $test"
