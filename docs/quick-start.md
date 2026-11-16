# Quick start

Run the editor, load your game data, and compile a MAP with the default build settings.

## Install

Download a binary archive from [GitHub Releases](https://github.com/reyond9079/ReyondBroom/releases). Choose the package for your system. The accompanying `.sha256` file contains the archive checksum.

- **Windows x64.** Extract the whole ZIP to a writable folder and run `Launch-ReyondBroom.cmd` or `ReyondBroom.exe`.
- **Linux x64 or Linux SDL2 x64.** Extract the whole archive to a writable folder and run `./Launch-ReyondBroom.sh` from the package directory. Check the [Linux requirements](linux.md) if a dependency is missing. The SDL2 build is an alternative desktop backend.

Keep the launcher, runtime files, and tools together. Supply your own retail game data or use LibreQuake for Quake 1.

## Set up your first game

1. Choose **Work offline** for local mapping and follow the setup screens. Choose your [game profile](game-compatibility.md) before loading resources.
2. For a first Quake map, select **Quake 1** and point the editor to your `id1` or mod folder. You can also use [LibreQuake](quake1.md). On Linux, download it separately, extract it, and select the `lq1` folder. See [Linux setup differences](linux.md#platform-differences).
3. Select **Load resources and start**. The [manual](manual.md#installation-and-first-time-setup) explains how to revisit resource settings and add WADs or custom entity definitions.

## Edit, compile, and play

1. Open a copy of a small, sealed MAP made for the selected game. For Quake, use a room with an `info_player_start` and a light. If you are building from scratch, follow [the brush tools guide](manual.md#brushes-and-geometric-tools).
2. Make an edit, then save the MAP in a writable folder. The [workspace guide](manual.md#workspace-and-navigation) covers navigation and layout.
3. Click **Compile**. Use **Setup compile** to change compiler paths, stages, or arguments. When the build finishes, use **Show BSP** to inspect the compiled geometry and baked lighting. Use **Back to editing** to return to the MAP.
4. Use **Play mod** with a compatible runtime, or a **Run** profile for a separate game engine. See [game compatibility](game-compatibility.md) and [compile and launch profiles](compilation-profiles-build.md).

## Troubleshooting

- **Missing textures or entity icons.** Confirm that the active profile points to the right game data. For Quake 1, check the palette, WAD2 files, and mod archives. An FGD can refer to a model or sprite that must be present in the mod.
- **A compile fails or reports a leak.** Read the first error in the build log. If the compiler writes a pointfile, use the [leak viewer](leak111.md) to follow the route in the map.
- **The BSP opens but the game does not launch.** Check which runtime the profile uses, where the mod files are installed, and how the engine receives its map name. See the [launch configuration guide](compilation-profiles-build.md#vkquake--ironwail-launch-reyon-8).
- **Mapping Jams resources stall.** See [Mapping Jams troubleshooting](mapping-jams.md#troubleshooting) and the [Linux requirements](linux.md).

When [reporting an issue](https://github.com/reyond9079/ReyondBroom/issues), include the editor version, OS and backend, game profile, and the steps that reproduce the problem. Attach a build log or a shareable map if it helps reproduce the issue.
