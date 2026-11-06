#!/usr/bin/env bash
# Runs against a built runtime with this branch's native plugin installed.
# Uses only generated resources under the supplied dedicated QA directory.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
bundle=$(cd "${1:?Pass built Linux runtime directory}" && pwd)
qa=${2:?Pass a dedicated empty QA directory}
mkdir -p "$qa"
qa=$(cd "$qa" && pwd)
python3 "$repo/scripts/audio-editor-fixtures.py" "$qa"
cp "$repo/tests/audio-editor.cfg" "$bundle/ftebroom/audio-editor.cfg"
printf 'set ftebroom_test_import_root "%s"\nexec audio-editor.cfg\n' "$qa" > "$bundle/ftebroom/audio-editor-run.cfg"
cd "$bundle"
SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 timeout 90s ./fteqw-gl64 -ftebroom-child-processes -nohome -noupdates -nosound -window -width 1920 -height 1080 -basedir "$bundle" +exec ftebroom.cfg +exec audio-editor-run.cfg > "$qa/native.log" 2>&1
if rg 'DESKTOP_FAIL|CSQC_Abort|Assertion .*failed' "$qa/native.log"; then exit 1; fi
for check in async_import all_approved_bindings_resolve mono_import_metadata three_real_sources_tracks dry_mix_ready ctrl_b_consumed ctrl_b_audio_split independent_undo text_focus_blocks_shortcut outside_focus_passes_to_map timeline_mouse_blocks_map save_project_preserves_source project_roundtrip range_export_48k_mono source_update_real_audio immutable_source_snapshot source_update_retains_edit_decisions source_backup updated_project_reopens_original failed_load_retains_project external_change_guard roots_guard pak_update_preserves_neighbors export_cannot_overwrite_source engine_event_routes_to_audio capture_started_after_countdown recording_creates_clip_source recording_file_persisted; do
  rg "DESKTOP_PASS audio4_$check" "$qa/native.log"
done
cp "$bundle"/ftebroom/audio4-*.png "$qa/"
