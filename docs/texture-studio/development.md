# Texture Studio development build

Open **Assets → Texture Studio** or use the button in the Material Browser. The Model Editor also opens the same source from its texture panel. Quick, Layers, and Node Graph share the document and undo history.

Use **Save As** to create a .rbtex.json source or **Save preset** to add a copy to the local library. **New** searches built-in and user presets and filters favorites and recently used recipes. Right-click a user preset to duplicate or remove it. **Import Image** copies the original PNG, JPEG, TGA, WAD2, WAD3, or WAL bytes and records the supplied license. Switch to the source game's profile for WAD2 or Quake II WAL so the original palette can be retained. Enter a texture name when a WAD contains more than one texture. The derived height, normal, and ambient occlusion are editable estimates. **Save Package** bundles the source and its original images as an .rbtexpkg. Open accepts either format. Saved sources carry their original files in the adjacent assets directory. Recovery copies are managed by the editor.

Open and Import Image use the bundled Texture Studio executable as a decoder. The editor keeps the current source and preview while the new input is read. Cancel stops the decoder, and an input decoder that exceeds 30 seconds stops with a visible error.

Preview the selected channel flat or tiled with a UV grid, seam markers and pixel values. Orbit or pan the sphere and cube to inspect the material's relief under two lighting setups. **Selected surface** uses a selected map face with its existing UV projection. **Game profile** uses the active indexed export palette where applicable. **Dry run** in Export encodes the current result and shows its exact byte count and SHA-256 without creating a file. In Node Graph, drag a card to move it and drag a typed output pin to a compatible input. Choose a node from the dropdown to center its card. Select group members with Ctrl-click or the node checkbox, then use **Group nodes** to retain shared parameters and seed streams. Groups expose their typed links in the same canvas and connection dropdowns. Ctrl+Z and Ctrl+Y apply to the focused Texture Studio window.

Each built-in recipe has four named alternatives. They vary features that fit the material, such as firing and mortar in masonry, enamel tone and sheen in smooth panels, or wave shape and reflection in water. **New → Texture families** provides six coordinated sets of eight surfaces. Open **Family settings** on the Quick tab to set the palette, seed, pattern frequency and wear for the current family member. Applying those settings preserves values you edited locally. **Reset local overrides** restores the values inherited from the family.

**Generate & Apply** captures the selected faces before baking. The Quake 1 and Half-Life paths attach a WAD in the same undo step. Quake II writes a WAL. Quake III and FTE Custom write a namespaced TGA. These paths preserve the selected face projections. With an empty selection, **Generate Asset** writes the material and its source to the game directory without touching the map. Right-click a generated material in the Material Browser and choose **Edit Texture Studio source** to reopen its graph. **Apply to model texture** replaces the active procedural texture slot as one model history edit. The slot keeps its UUID and UV dimensions. A model texture can carry up to 16 MiB of original image data. Its **Texture Studio** button reopens the saved source.

## Headless usage

The native update workflow builds and packages the command as `tools/texture-studio` on Linux and `tools/texture-studio.exe` on Windows.

```sh
texture-studio new brick.rbtex.json
texture-studio new panel.rbtex.json --preset "Luminous Panel"
texture-studio new plate.rbtex.json --preset metal-diamond-plate --variation 2 --seed 42
texture-studio family industrial-bunker --seed 42 --out bunker
texture-studio family industrial-bunker --from-family bunker --family-shade 384046 --family-scale 115 --family-wear 12 --out bunker-revised
texture-studio validate brick.rbtex.json
texture-studio render brick.rbtex.json --threads 4 --out preview
texture-studio new ripples.rbtex.json --preset "Water Ripples"
texture-studio render ripples.rbtex.json --all-frames --out ripple-frames
texture-studio export ripples.rbtex.json --all-frames --profile q3-baseline --frames 8 --name demo/ripples --pk3 --out game-ripples
texture-studio export panel.rbtex.json --profile fte-rich --name demo/panel --dry-run
texture-studio export brick.rbtex.json --profile q3-baseline --name demo/brick --pk3 --out package
texture-studio import original.png --license CC0-1.0 --out material
texture-studio import original.wad --texture floor01 --palette quake1.lmp --license CC0-1.0 --out floor
texture-studio bundle material/source.rbtex.json --out material.rbtexpkg
texture-studio unpack material.rbtexpkg --out restored
```

The indexed profiles use a local 768-byte palette supplied through `--palette`. The Quake III profile accepts `--jpeg 92` for an opaque static texture and `--pk3` for a runtime archive. The FTE material adapter uses the engine's defaultwall program with explicit Normal and Emissive inputs. Use `--frame` for a specific frame. To export an animation as WAD2, WAD3, WAL or a Quake III shader, use `export --all-frames`. Set `--frames` when a target needs resampling and acknowledge changes to loop duration with `--accept-rate-change`. The manifest identifies the first material and maps exported frames to the source.

Use `family --from-family` to apply shared settings to all eight sources. The command copies their local edits and referenced originals into a new output directory.

## Verification

- The native core suite builds with C++17, zlib and floating-point contraction disabled. It covers graph validation, reference generation, seeds, thread invariance, bounded rendering, and codec fixtures. Run the core binary with `--catalog-matrix DIR` to evaluate all 120 library defaults with five seeds at 128 and 256 pixels and the default seed at 64 and 512. The matrix renders 480 curated variants and 48 family members and verifies parameter ranges. It saves the previews to DIR.
- The CLI suite checks profile export paths, file hashes, dry runs, PNG-derived material inputs, package round trips, and rejection of unexpected archive members.
- The native editor script runs on a disposable runtime copy. It uses real input to test face Apply and history, model-slot Apply and history, and image import with a portable package. Use `--layout-only` with `--theme`, `--size`, and `--scale` to check the studio controls in the live editor.
- Use `--input-stall-helper` with the compiled `tests/fixtures/texture-studio/stalled-decoder.cpp` to verify cancellation, the decoder deadline, and reopening the source with the real decoder.
- Static and animated material fixtures were viewed in QuakeSpasm with LibreQuake, Yamagi Quake II, OpenArena, Steam Half-Life and the FTE preview engine from alpha.31. The FTE material fixture compares the same scene with a flat normal and with emission disabled.

## Release controls

The build workflow produces unpublished artifacts. The Texture Studio checks run the graph, format, CLI and release-gate suites on Windows and Linux. The scheduled job exercises the graph, image and ZIP parsers under AddressSanitizer and UndefinedBehaviorSanitizer. Set `rb23_parser_handoff` to run each parser for two hours without building release packages.

The release workflow checks a successful evidence run and candidate build run from the same checkout and repository. The owner can import the evidence from `qa/rb-23/evidence` in a direct child commit by setting `rb23_evidence_commit`, `rb23_candidate_run_id` and `rb23_build_sha256`. The import verifies the complete RB-23 test inventory and the SHA-256 of every runtime and source archive with read-only permissions. The owner authorized automated acceptance for RB-23. Publication requires the owner's explicit authorization, the frozen package hashes and a run on `main` in the `rb23-release` environment. The publisher creates the exact version tag with the repository deploy key and then uploads and publishes the prerelease.

The GitHub rules restrict updates to `main` to the owner and block its deletion and force pushes. The `rb23-release` environment can deploy from `main`. A separate repository deploy key creates version tags matching `v0.6.0-alpha.*`. Keep other deploy keys read-only. An independent rule blocks updates and deletion of the version tags. Store the private material only as `RB23_RELEASE_TAG_KEY` in the release environment.
