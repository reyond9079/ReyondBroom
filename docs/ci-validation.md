# Affected validation and dependency caches

Pull requests and releases use `scripts/ci-plan.py`. The plan records changed
paths, their owning subsystems, and the transitive set of consumer checks in a
short-lived `validation-plan` artifact and the Actions summary. Renames include
both the removed and added paths. A missing comparison ref fails the plan rather
than silently skipping checks. Unclassified shared code selects conservative
integration coverage.

| Change | Default validation |
| --- | --- |
| Compilation profiles/core | EL/argv/import core tests; legacy/import/restart/migration; real export/compiler/copy/delete; working/source paths; mod/WAD/lighting; engine launch and profile UI |
| Shared resource mounting | Compilation plus Q2 resource lookup, audio archive opening and model/plant placement integration |
| Shared input | Profile UI, viewport/input, browser, onboarding, audio UI and placement integration |
| Rendering/grid | Grid shader oracle, viewport checks and rendered placement integration |
| Plant algorithms/presets | Generator/codec/LOD tests, Plant Editor, exports and placement |
| Plant UI | Plant Editor/brush/export and placement integration; no botanical corpus |
| Audio | DSP, audio window/archive tests; rebuild the converter only when its build inputs differ |
| CI infrastructure only | Planner/cache/runner/archive contracts and cold/warm CMake cache probes on Linux/Windows |

The CI-only category includes the orchestration scripts and their explicit
test-harness adapters. In mixed product changes those adapters still select their
product suites. A generic shared source edit is never treated as CI-only.

Normal profile checks use `test-compilation-runtime.py --targeted`, keeping all
profile behavior, migration and real-compiler assertions but excluding navigation
and the multi-scale UI atlas. Themes/sizes are not multiplied on every PR.

## Comparison refs and manual full checks

PR validation compares against the merge base with the PR base SHA. Releases
compare actual endpoint trees, including reversions and rollbacks. A manual
source/release run defaults to the latest published release. `validation_base`
overrides this ref. It is deliberately independent of `base_tag` and
`linux_base_tag`, which select the compatible SDK/runtime used for packaging.

Choose `validation_mode=full` when manually dispatching
`compilation-profile-checks.yml` or `release-linux-ui-update.yml` to run the full
corpus, UI atlas/reference and authoring checks. There is no automatic nightly
full run. Botanical contact sheets remain explicitly selectable. The standalone
audio workflow remains available manually; normal audio changes run through the
affected pipeline instead of compiling the Windows editor twice.

Every release, including one with no selected long suites, runs a separate
mandatory gate for all three platforms: archive checksums, every distribution
manifest entry, version/source revision, matching extracted launch binaries,
corresponding-source contents and a short startup of the real ImGui editor.
Software OpenGL is installed only in the extracted test runtime, not the archive.

## Safe dependency reuse

`.github/actions/build-dependency` caches Bullet, the profile core/probes and the
Windows FFmpeg/LAME/Ogg/Vorbis output. Keys include exact source hashes, build
recipes, target platform, compiler/tool binaries and versions, package/image
identity, workspace paths and build flags. Audio source revisions are pinned in
the build recipe and the FFmpeg tarball is hashed. No partial `restore-keys` are
used. Native editor modules and CSQC are rebuilt from the current checkout.

Restores require the matching input key and a complete SHA-256 output inventory.
Missing/modified outputs are rebuilt. CTest results are excluded from the cache
inventory and are executed again when profile checks are selected. Audio cache
outputs live separately under `.ci-deps/audio`, then merge into `tools/audio`
without overwriting the tracked `BUNDLE.md` documentation.

`ci-policy-checks.yml` checks dependency closure, rename/deletion selection,
invalid baselines, runner fail-fast behavior, tampered artifacts and cache
invalidation. Its small CMake fixture exercises a cold build and an actual
verified warm restore on the same Linux/Windows runner, avoiding false misses
caused by different runner images. These are CI tests, not editor feature tests.
New QA artifacts expire after seven days; existing artifacts and releases are
untouched.

World Architect has separate `world_core` and `world_ui` suites. The core suite
checks mission prerequisites, serialization, cancellation, exact convex player
clearance, a small seed corpus and real Q1/Q3/FTE MAP compilation. Manual full
mode expands the corpus to 100 seeds per preset/profile. UI checks exercise the
real render target, project replay, export, acknowledged Apply and Undo/Redo.

Known World Architect additions to shared host/command files are classified only
when removing those exact registrations/guards leaves the complete file equal
to the base revision. Any other edit keeps the conservative shared-code scope.
Focus guards include input consumers; geometry changes include the tool UI and
placement. This does not select the unrelated plant-generation corpus.
