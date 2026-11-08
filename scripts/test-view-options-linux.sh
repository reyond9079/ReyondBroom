#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")/.." && pwd)
runtime=${1:?Packaged Linux runtime directory}
qc_image=${2:-"$src/csaddon.dat"}
if [ -z "${DISPLAY:-}" ]; then exec xvfb-run -a -s '-screen 0 1280x800x24' bash "$0" "$runtime" "$qc_image"; fi
test=$(mktemp -d "${TMPDIR:-/tmp}/reyond-view-options132-XXXXXX")
cp -a --reflink=auto "$runtime/." "$test/"
cp "$qc_image" "$test/ftebroom/csaddon.dat"
cp "$src/tests/view-options132.cfg" "$test/ftebroom/"
cd "$test"
LIBGL_ALWAYS_SOFTWARE=1 timeout 40s ./fteqw-gl64 -nohome -noupdates -window -width 1280 -height 800 -condebug +vid_width 1280 +vid_height 800 +exec view-options132.cfg > console.txt 2>&1
if rg '_FAIL|CSQC_Abort|runaway loop error|mod by zero|Outgoing message overflow' console.txt; then exit 1; fi
for name in brush_and_patch_visible_and_pickable hide_brushes_preserves_patch_render_and_pick hide_patches_preserves_brush_render_and_pick both_visibility_switches_hide_their_geometry selected_brush_remains_visible_when_brushes_hidden selected_patch_remains_visible_when_patches_hidden visibility_switches_rebuild_render_and_pick_access selected_hidden_class_keeps_point_and_label_entry deselected_hidden_class_removes_point_and_label_entry entity_classname_toggle_keeps_bounds group_names_and_bounds_independent_of_entity_classnames entity_bounds_toggle_keeps_classnames group_toggle_hides_both_group_names_and_bounds one_hidden_face_keeps_brush_and_five_material_faces hidden_face_grid_fill_and_occlusion_removed_edges_retained hidden_face_blocks_pick_visible_face_remains_pickable native_grid_builder_reapplies_hidden_alpha_without_hiding_edges grid_step_changes_keep_hidden_fill_transparent selected_whole_brush_restores_all_face_fill_and_grid individually_selected_hidden_face_is_rendered selecting_another_face_does_not_unhide_tagged_face different_hidden_tags_on_all_faces_hide_whole_brush selected_brush_overrides_all_hidden_face_tags unchanged_filter_and_selection_do_not_rebuild_geometry_each_frame; do
  rg -q "FTEBROOM_VIEWOPTS_PASS $name" console.txt
done
for name in additive_face_selection_invalidates_visibility_cache face_toggle_invalidates_visibility_cache bulk_additive_selection_keeps_hash_valid_and_updates_epoch repeated_face_paint_sample_keeps_selection_epoch_stable repeated_face_deselect_keeps_selection_epoch_stable; do
  rg -q "FTEBROOM_VIEWOPTS_PASS $name" console.txt
done
echo "PASS: $test"
