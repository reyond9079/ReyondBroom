# Texture Studio development build

Open **Assets → Texture Studio** or use the button in the Material Browser. The Model Editor also opens the same source from its texture panel. Quick, Layers, and Node Graph share the document and undo history.

Use **Save As** to create a .rbtex.json source or **Save preset** to add a copy to the local library. **New** searches built-in and user presets and filters favorites and recently used recipes. **Import Image** copies the original PNG, JPEG, TGA, WAD2, WAD3, or WAL bytes and records the supplied license. Switch to the source game's profile for WAD2 or Quake II WAL so the original palette can be retained. Enter a texture name when a WAD contains more than one texture. The derived height, normal, and ambient occlusion are editable estimates. **Save Package** bundles the source and its original images as an .rbtexpkg. Open accepts either format. Saved sources carry their original files in the adjacent assets directory. Recovery copies are managed by the editor.

Preview the selected channel flat or tiled, or preview Base Color on a lit sphere or cube. **Selected surface** uses a selected map face with its existing UV projection. **Game profile** uses the active indexed export palette where applicable. **Dry run** in Export encodes the current result and shows its exact byte count and SHA-256 without creating a file. In Node Graph, drag a card to move it and drag a typed output pin to a compatible input. Choose a node from the dropdown to center its card. Select group members with Ctrl-click or the node checkbox, then use **Group nodes** to retain shared parameters and seed streams. Ctrl+Z and Ctrl+Y apply to the focused Texture Studio window.

**Generate & Apply** captures the selected faces before baking. The Quake 1 and Half-Life paths attach a WAD in the same undo step. Quake II writes a WAL. Quake III and FTE Custom write a namespaced TGA. These paths preserve the selected face projections. **Apply to model texture** replaces the active procedural texture slot as one model history edit. The slot keeps its UUID and UV dimensions. A model texture can carry up to 16 MiB of original image data. Its **Texture Studio** button reopens the saved source.

## Headless usage

The native update workflow builds and packages the command as `tools/texture-studio` on Linux and `tools/texture-studio.exe` on Windows.

```sh
texture-studio new brick.rbtex.json
texture-studio new panel.rbtex.json --preset "Luminous Panel"
texture-studio new plate.rbtex.json --preset metal-diamond-plate --variation 2 --seed 42
texture-studio family industrial-bunker --seed 42 --out bunker
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

## Verification

- The native core suite builds with C++17, zlib and floating-point contraction disabled. It covers graph validation, reference generation, seeds, thread invariance, bounded rendering, and codec fixtures. Run the core binary with `--catalog-matrix DIR` to evaluate all 120 library defaults with five seeds at 128 and 256 pixels and the default seed at 64 and 512. The matrix renders 480 curated variants and 48 family members and verifies parameter ranges. It saves the previews to DIR.
- The CLI suite checks profile export paths, file hashes, dry runs, PNG-derived material inputs, package round trips, and rejection of unexpected archive members.
- The native editor script runs on a disposable runtime copy. It uses real input to test face Apply and history, model-slot Apply and history, and image import with a portable package. Use `--layout-only` with `--theme`, `--size`, and `--scale` to check the studio controls in the live editor.
- Static and animated material fixtures were viewed in QuakeSpasm with LibreQuake, Yamagi Quake II, OpenArena, Steam Half-Life and the FTE preview engine from alpha.31. The FTE material fixture compares the same scene with a flat normal and with emission disabled.

## Remaining release criteria

RB-23 still requires the remaining graph operations and profile checks, full visual coverage of the preset and family variants, long fuzz and performance evidence, and the complete platform regression suite on an exact candidate. The current implementation is an internal development build. The release verifier requires passing evidence and the exact package hashes before granting upload credentials.
