# Game compatibility

A game profile selects the source MAP format, resource lookup, entity definitions, and default build tools. Choose the profile that matches the game or mod you plan to run.

| Profile | MAP and resources | Compile and playtest |
| --- | --- | --- |
| [Quake 1](quake1.md) | Standard and Valve 220 MAP, WAD2 textures, PAK or loose files. Use your own Quake data or LibreQuake. | ericw-tools builds BSP29 or BSP2. Use **Play mod** with a compatible runtime or configure an external engine such as VKQuake or Ironwail. |
| [Quake II](legacy-games-alpha94.md#quake-ii) | Valve 220 MAP with Quake II surface flags and values, PAK resources, and WAL/PCX textures. | The Quake II ericw-tools chain builds IBSP38. Use **Show BSP** to inspect the build and an external Quake II engine to play. |
| [Quake III Arena](manual.md#installation-and-first-time-setup) | Quake III MAP with brushes and patches. Select a `baseq3` folder with PK3 archives or loose textures and shader scripts. | q3map2 builds Quake III BSP. Configure a Quake III engine through a Run profile to playtest. |
| [Half-Life](legacy-games-alpha94.md#half-life-and-counter-strike-15) | Valve 220 MAP, WAD3 materials, FGD definitions, and GoldSrc studio model previews. | SDHLT builds BSP30. FreeHL and FreeCS determine which gameplay features are available in preview and require compatible game data. |
| [FTE Custom game](when-the-sky-went-dark.md) | Quake III authoring format with an optional separate FTE runtime mod. This profile exposes [Reyond's GLSL Shader Pack](shader-pack.md). | q3map2 builds Quake III BSP. Playtesting requires your FTE runtime mod and its game data. |

**Show BSP** loads the compiled result in the editor, including lightmaps. **Play mod** launches compatible game logic and resources. Windows and Linux present the game preview differently. See [Linux platform differences](linux.md#platform-differences).

Edit or import per-game tasks in [Compilation profiles](compilation-profiles-build.md). When choosing a model export format or a runtime entity for a generated asset, use the target profile's limits in the [Model Editor guide](model-authoring.md) and [Plant Editor guide](plant-editor.md).
