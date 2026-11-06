# REYON-6 — implementation and acceptance evidence

The Compile, Launch Engine, Game Engines, settings and import interfaces run inside
the existing ImGui editor. No Qt SDK, library, helper process or runtime is needed.
The user's clarification waives pixel equality, Qt controls and Qt fonts; layout,
compactness, button order, icons and interaction remain acceptance requirements.

Reference: TrenchBroom **2026.2**, commit
`4c756be1a54a28d921d18c77716a5f170bdc31d0`. The 185 vendored upstream files were
compared byte for byte with that checkout: no differences. Original notices and
GPL-3.0-or-later licensing are retained; fmt 11.2.0 retains its MIT license.

The original customer attachment, the research copy and the committed fixture have
the same SHA-256:
`364f7a9635c8ac3fbc82764ca429afb5d9e07a6485ee0b3e073ea6bf6adeecda`.
It contains **5 profiles / 70 tasks**. Paths, expressions, flags and `.bspw` are
preserved, including after serialization and editor restart.

## Reproduce

Use disposable runtimes. The probes deliberately replace their profile state.
Build instructions and migration/rollback notes are in
[compilation-profiles-build.md](../../compilation-profiles-build.md).

```sh
python scripts/build-profile-support.py core --build-dir build/profile --tests
# Install the rebuilt host, csaddon.dat and build/profile/compilation-profile-probe.
xvfb-run -a -s '-screen 0 1920x1080x24' python scripts/test-compilation-runtime.py /disposable/runtime qa
xvfb-run -a -s '-screen 0 1920x1080x24' python scripts/test-compilation-ui.py /disposable/runtime qa/ui
```

Windows runs the same Python drivers directly. The CI machine uses a checksum-pinned
Mesa software renderer for working headless OpenGL; it is a test dependency and is
not packaged with the editor. SDL2 rebuilds the pinned engine with the editor's
window-close guard. Both event paths now ask before stopping an active compilation.

The CI artifact contains real editor logs, test results, geometry JSON and screenshots
for 800×600 / 1200×800 Compile at 100%, 150% and 200% editor scaling, light and dark.
Actual framebuffer dimensions are preserved when the desktop constrains its size.
These are editor screenshots, not rendered mockups. The Linux reference atlas was
captured from the unmodified official TrenchBroom executable. The Windows oracle
uses the checksum-pinned official portable release, OS input and Windows UI
accessibility: 12 captures and measured control rectangles, without a Qt SDK.

## Coverage

“Executed” below means a real filesystem, process, editor or upstream test ran.
“Source” explicitly distinguishes inherited behavior from an additional OS-level
oracle experiment. The matrix does not turn unexecuted cases into passes.

| ID | Evidence and scope |
| --- | --- |
| A01 | Executed: original 5/70 import, exact text/flags, `.bspw`, JSON roundtrip, fresh-process restore, real UI Review/Cancel/Import. |
| A02 | Executed: unmodified upstream EL corpus, 969 assertions / 6 cases. |
| A03 | Executed: original profile CPU arithmetic for 1/2/8/64 CPUs; whitespace and expression errors. |
| A04 | Executed: ordered per-map MODS, first/negative indices, game root and Undo. Upstream `splitMods`; persistent project/store records. |
| A05 | Executed: workdir variable scope, unknown-variable errors and runtime expansion. Source: exact common/task/launch variable definitions. |
| A06 | Executed: absolute/empty/missing workdir, export-created workdir, relative executable with parent and PATH lookup, recorded actual probe cwd/argv. |
| A07 | Executed: native Windows/Linux processes and Unicode filenames; original `D:/...` strings remain unchanged. Source: UNC, other-drive and mixed-separator path operations use original TB path/Disk code; no SMB server or cross-volume oracle was provisioned. |
| A08 | Executed: unmodified upstream argv corpus, 10 assertions / 1 case; actual process arguments with quoted paths/backslashes, empty quotes and single quotes. Windows uses CRT quoting after the TB parser. |
| A09 | Executed: flat Copy, `*`/`?` and case matching, target creation, overwrite, same-file error with source retained, directory-source exclusion, missing parent and no-match continuation. Extra edge checks execute the pinned TB Disk implementation directly through the compatibility core. |
| A10 | Executed: recursive Delete, nested `.TMP`, untouched BSP, no-match, missing parent and a directory symlink inside an isolated Linux tree. A real TB readonly-folder oracle confirmed its individual-delete-error continuation behavior; core test preserves it. Windows junction traversal remains source-confirmed through original TB Disk traversal. |
| A11 | Executed: rename into created parents with Unicode names, Linux existing-target replacement, target directory, missing source, literal wildcard error and cross-filesystem `EXDEV` without fallback or source loss. Windows other-volume behavior remains source-confirmed through the same original move operation. |
| A12 | Executed: current-map export, added camera entity/extra properties, property/classname stripping and source-map preservation. Source: existing QC omit-layer/geometry export and pinned TB filtering rules; export options act only on the snapshot. |
| A13 | Executed: exit 0, nonzero stop/continue, spawn failure and crashes (access violation / breakpoint); failures/crashes always stop, regardless of nonzero policy. Windows exception ranges and the reference's reserved forced-termination status are preserved. Actual stdout/stderr reach Output. |
| A14 | Executed: engine IDs A/B, missing ID with both policies, actual executable cwd and detached process, edited parameters through real Launch controls. |
| A15 | Executed: Test produces a plan without changing the filesystem or starting a probe. No workdir is eagerly created. |
| A16 | Executed: disabled invalid expression is preserved and skipped. Import does not evaluate task expressions. |
| A17 | Executed via OS input: Enter/Tab/completion, Escape/No, Close/Yes, document-change/No, OS window-close/No, Stop and subsequent reopening. Actual sleeping process is terminated. |
| A18 | Executed: empty and duplicate names, profile/task CRUD, insertion and reorder, disabled state, per-game persistence and an empty list that stays empty. |
| A19 | Executed in two fresh processes: migration of actual legacy records, flags, variables and game scopes; idempotence, original and `.legacy.bak` retained. |
| A20 | Executed locally: Q1 BSP29/BSP2, QBSP/VIS/LIGHT, LIGHT-only and preview; Q3 build and automatic preview; real Q1 classic/Valve 220, Q2 and HL export/process/file pipelines; strict q3map2 export → compile → deploy → cleanup. Q2/HL full compiler builds were not run. |
| U01 | Executed: real UI atlas and measured layout invariants; Linux and Windows TB reference atlases are included in the evidence archive. Windows reference measurements come from actual UI accessibility. Pixel/font/control equality is waived. |
| U02 | Executed: real editing, an 8-KB clipboard paste without truncation, disabled/running states, menu insertion/reorder/duplicate/remove, completion, output and splitter layout, local keyboard behavior, Engine configuration and Launch. Six task forms follow the pinned source field order. |
| U03 | Executed: visible import row above mini-toolbar; Review/Cancel leave data unchanged; commit imports the original 5/70 with `.bspw`. |

## Actual compiler observations

A separate real Q1 chain runs Export → QBSP → VIS → LIGHT → Copy BSP/LIT → Delete
PRT → Launch Engine. VIS receives the literal **`.bspw`** argument. Bundled ericw VIS
itself strips that extension, opens the corresponding `.bsp` and exits **0**.
ReyondBroom does not rewrite the argument. The deployed result is BSP29; the original
map is unchanged. The evidence archive includes VIS argv/output and BSP SHA-256.

TrenchBroom's Delete runner has a less obvious quirk: an individual deletion failure
stops that deletion batch but continues the pipeline without logging the deletion
error. A readonly-folder experiment against the official executable confirmed it.
Missing-directory/traversal failures still fail. The compatible implementation keeps
this observed distinction rather than inventing a stricter policy.

## Delivery

Implementation PR: https://github.com/reyond9079/ReyondBroom/pull/8

Issue/evidence archive: https://reyond.atlassian.net/browse/REYON-6

The evidence distinguishes completed implementation from the remaining oracle
coverage listed above. Source-only filesystem edge cases must not be described as
independently executed cross-editor acceptance. No release was published for this task.
