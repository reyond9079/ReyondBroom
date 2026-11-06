#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
bundle=$(cd "${1:?Pass a built runtime directory}" && pwd)
qa=${2:?Pass a QA directory}
mkdir -p "$qa"
qa=$(cd "$qa" && pwd)
cp "$repo/tests/fixtures/CompilationProfiles-cpu-count.cfg" "$bundle/ftebroom/CompilationProfiles-regression.cfg"
cp "$repo/tests/trenchbroom-native.cfg" "$repo/tests/trenchbroom-restore.cfg" "$repo/tests/run.cfg" "$bundle/ftebroom/"
cd "$bundle"
for case in run trenchbroom-native trenchbroom-restore; do
  LIBGL_ALWAYS_SOFTWARE=1 timeout 65s ./fteqw-gl64 -ftebroom-child-processes -nohome -noupdates -nosound -window -width 1280 -height 800 -basedir "$bundle" +exec "$case.cfg" > "$qa/$case.log" 2>&1
  if rg 'FTEBROOM_RUN_FAIL|CSQC_Abort|Assertion .*failed' "$qa/$case.log"; then exit 1; fi
done
for check in customer_five_profiles_imported customer_seventy_tasks_preserved customer_profiles_saved; do rg "FTEBROOM_RUN_PASS $check" "$qa/trenchbroom-native.log"; done
for check in customer_profiles_restored customer_expression_expanded_after_restore customer_single_core_uses_one_thread; do rg "FTEBROOM_RUN_PASS $check" "$qa/trenchbroom-restore.log"; done
test "$(rg -c 'FTEBROOM_RUN_PASS ' "$qa/run.log")" = 15
cp "$bundle/ftebroom/trenchbroom-import.png" "$qa/"
