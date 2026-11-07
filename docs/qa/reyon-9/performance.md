# REYON-9 performance measurements

Measured 2026-11-07, sequential processes without another local editor/compiler
running. AMD EPYC 9V74 host, container quota 4 CPUs / 16 GiB, Linux 6.18.57,
glibc 2.41, GCC 14.2.0. Both standalone binaries use `-std=c++17 -O2` and the
same `tests/plant-performance.cpp`; baseline headers are frozen at `9e5b30a`.
Seed 137, factory presets, unchanged 32,000 tree triangle budgets. Each LOD has
one warmup and 15 measured samples. Values below are median generation wall
milliseconds. Texture generation is measured separately, once, at the default
1024×1024 atlas size; it runs on the background worker in the editor.

| Preset | Before LOD0 / 1 / 2 ms | After LOD0 / 1 / 2 ms | Before → after triangles LOD0 / 1 / 2 |
| --- | --- | --- | --- |
| oak | 11.94 / 6.35 / 2.60 | 20.69 / 10.17 / 4.54 | 31988 / 15988 / 5856 → 31994 / 16000 / 6396 |
| spruce | 20.35 / 8.92 / 3.32 | 16.92 / 9.51 / 4.79 | 32000 / 16000 / 6396 → 31998 / 15998 / 6400 |
| meadow-grass | 1.00 / 0.50 / 0.19 | 0.75 / 0.31 / 0.12 | 2176 / 1184 / 496 → 1632 / 568 / 168 |

The maximum default LOD0 triangle count is 32,000 on both catalogues. The
first maximum is spruce in the 30-preset baseline and paper-birch in the
121-preset catalogue; their full measurements are the fourth entries in the
raw JSON files. This means heaviest **by geometry count**, not an assertion
that it is the slowest seed or the worst case for arbitrary user settings.

| Version / preset | LOD0 vertices | Materials used | Texture time ms | Three CPU RGBA maps bytes |
| --- | ---: | ---: | ---: | ---: |
| generator 1 / oak | 24896 | 2 | 104.34 | 12582912 |
| generator 1 / spruce | 36868 | 2 | 120.32 | 12582912 |
| generator 1 / meadow-grass | 2040 | 1 | 109.47 | 12582912 |
| generator 1 / spruce | 36868 | 2 | 135.58 | 12582912 |
| generator 2 / oak | 38326 | 5 | 353.28 | 12582912 |
| generator 2 / spruce | 32676 | 4 | 306.89 | 12582912 |
| generator 2 / meadow-grass | 1530 | 1 | 292.04 | 12582912 |
| generator 2 / paper-birch | 36818 | 5 | 342.10 | 12582912 |

Standalone peak RSS, including scanning each whole catalogue to identify its
largest mesh: 21,668 → 25,540 KiB. These figures are
not the full editor's memory usage. Mesh byte estimates, vertices/materials for
every LOD, all 15 timing samples, compiler/host and peak-RSS metadata are retained
in the adjacent JSON files.

The new oak takes more CPU time because it plans and distributes an entire
canopy instead of stopping recursive growth when the budget runs out. Spruce
LOD0 and grass improve; other tree LODs can cost more. Texture generation also
costs more for species-specific organs, veins and bark. No overall speedup is
claimed. Geometry, maps and lazy thumbnails remain cancellable background jobs;
main-thread engine resource registration and rendering are measured separately.

## Real editor frames and memory

Sequential 12-control OS-input runs, both native host modules built with GCC 14
`-O1`, same packaged base engine, Modern Dark, 1920×1080. Mesa 25.0.7 llvmpipe
(LLVM 19.1.7, 256 bits), software OpenGL. Baseline telemetry was added read-only;
its generator and material code are unchanged. Runtime folder build-info still
labels the retained base as alpha.126; `hardware.module_sha256` identifies the
actual instrumented modules. These are local comparison builds, not published
alpha.129 binaries. Packaged-platform evidence is collected separately.

| Selection | Before recent frame p95 ms | After recent frame p95 ms | Before → after peak RSS KiB |
| --- | ---: | ---: | ---: |
| oak | 53.0 | 71.0 | 319,256 → 333,520 |
| spruce | 50.0 | 62.0 | 359,680 → 367,784 |
| meadow-grass | 54.0 | 63.0 | 359,960 → 367,784 |
| date-palm | 49.0 | 55.0 | 358,960 → 367,784 |

Across the final 12-control run, baseline recorded 1 frame above 100 ms / 1,009
frames, maximum 128 ms; the new editor recorded 7 / 981, maximum 198 ms.
Recent p95 after warmup is 55 ms, versus 49 ms. These wall-frame times include
software rasterization, engine resource compilation and OS scheduling; they are
not the duration of Plant Editor's UI work alone and do not promise hardware
FPS. An earlier run with another local compiler contending for CPU reached a
119 ms recent p95; another run concurrent with image-sheet processing had a
949 ms outlier. Those contended runs are excluded from the table, not hidden as
algorithmic improvements. Final QA additionally records `plant_main_thread`
wall timings and slow-event preset names to distinguish the panel's work from
whole-engine/render/scheduler stalls. Inspect that evidence before interpreting
a slow software-rendered frame as synchronous mesh generation.

All shader/engine calls remain on the main thread. Texture generation's hundreds
of milliseconds occur on the worker. Camera changes do not regenerate meshes.
No claim of zero warmup stalls or universally sub-100 ms engine frames is made.
