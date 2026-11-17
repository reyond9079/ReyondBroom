# Pinned Quake compiler bundle

Run `scripts/fetch-ericw.ps1` on Windows, or `scripts/fetch-ericw.py` on Linux, to fetch and checksum-verify ericw-tools 2.0.0-alpha11 and its pinned corresponding source. The release packager installs this version for Quake 1 and local Mapping Jams compilation. Each bundle includes QBSP, VIS, LIGHT, runtime dependencies and upstream license notices. Complete corresponding source accompanies the editor release.

`PIN.json` records archive and binary hashes. Generated binaries, source archive and upstream HTML documentation are excluded from Git to avoid duplicating the distributed package. Packaging requires the local bundle; it does not download anything at editor runtime.

The tools are independent executables under their upstream GPL-3.0 license. Embree and TBB retain their release notices; see `LICENSE-embree.txt`, `gpl_v3.txt` and upstream `README.md` after fetching. FTEBroom does not link the compiler libraries into its plugin.
