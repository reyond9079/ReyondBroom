# Mapping Jams

> [!WARNING]
> Mapping Jams is an unstable alpha. Online progress may be reset in future updates. The welcome screen explains the anonymous button and error statistics collected to help fix bugs.

Mapping Jams is the online mode for creating and sharing Quake maps. The jam supplies a common resource pack. Choose **Work offline** to edit your local projects.

## Join and get the resources

1. On the welcome screen, choose **Join Mapping Jam (unstable alpha)** and read the notice.
2. Sign in or register. If prompted, confirm your email using the code sent to you.
3. If the jam asks for game resources, choose **Prepare Jam resources**. The editor downloads and verifies the pack, then opens the jam board. It reuses a verified local installation on the next visit.

## Make a map

1. Choose an available plot or select one you already own. The central **HUB** is reserved for the jam.
2. Choose **Create or continue map**. The editor uses the jam's Quake 1 setup and manages the paths for your jam maps. Use the provided resources when building your level.
3. Use **Compile and upload map** to publish. The jam checks the map against its limits before other players can download it. **Back to editing** returns to the source MAP after you inspect the compiled result.

The 3D editor shows published maps from other plots with their baked lighting while you edit your MAP. On the board or in the editor, use **Jam chat** to talk to the group. **Hide chat** collapses the panel, and new messages appear briefly above the sender in the 3D view when their avatar is visible.

## Play published maps

Select a plot with a published map and use **Play** to download and launch it. The **HUB** also has portals to published maps. Use **Browse Jam** from the jam editor to return to the board.

## Troubleshooting

- If **Game resources** stalls, note whether it is downloading, verifying, installing, or finishing the installation. On Linux, choose **Prepare Jam resources** again after a timeout. New packs use `$XDG_CACHE_HOME/reyondbroom` or `~/.cache/reyondbroom` when available. See the [HTTPS download dependencies](linux.md).
- If a map fails to compile, open the local build log. Include the exact stage and error message when [reporting an issue](https://github.com/reyond9079/ReyondBroom/issues).
