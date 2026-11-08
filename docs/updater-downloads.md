# GitHub download recovery

The updater fetches public release metadata and downloads runtime/checksum assets
through their canonical GitHub asset API URLs with `Accept: application/octet-stream`.
It requires no GitHub token. The normal release download URL is retained as a
fallback. Only URLs belonging to this repository are accepted.

Temporary HTTP 408/429/500/502/503/504 responses and interrupted transfers receive
up to three attempts per route with bounded backoff. Requests bypass stale caches.
Each archive attempt recreates its `.part` file. The complete byte count and
SHA-256 must match before staging; integrity failures do not trigger installation.
The editor may continue running during download. Installation still waits for it
to close and retains settings, maps and the rollback copy.

An older updater may fail to download the fix itself. On Windows, replace the
existing `ftebroom/update-ftebroom.ps1` with the same file from release alpha131,
then use **Help → Updates → Check for updates** again. The exact fixed source is
also available at `runtime/update-ftebroom.ps1` under Git tag `v0.5.0-alpha.131`.

The alpha131 packages retain the verified alpha130 editor, engine, CSQC and tools
byte for byte. `Editor revision` continues to identify that native build;
`Release revision` identifies the updated scripts and packaging source. All three
platform archives include complete corresponding sources and SHA-256 files.

`tests/test-updater-network.py` uses a real local HTTP server and changes only
the transport destination in a test harness. Retry logic, stream reads, hashing,
staging, process waiting and installation run unchanged. Windows CI executes
Windows PowerShell 5.1, matching the editor launcher. The packaged scripts also
download and install a real public alpha130 archive into a disposable copy.
