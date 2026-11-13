# Mapping Jams local integration

For private deployment on Linux, see the [staging guide and systemd unit](deploy/README.md).

This loopback-only development instance uses SQLite, a local mail outbox and the installed Quake 1 tools. The first Jam has a 5 × 5 board, a 3 × 3 Hub, and 16 plots. Geometry thresholds are test values.

## Windows launcher

Open **out/jam-editor-clean/Launch-Local-Mapping-Jams.cmd**. It starts or reuses the API on 127.0.0.1:8770 and launches the isolated editor. It keeps the console open on startup failure. The launcher invokes Python directly and follows the machine's existing PowerShell policy.

To set up the interpreter in a fresh checkout, run the following from the repository root. A compatible rebuilt editor module and isolated engine bundle are required.

~~~powershell
py -3.13 -m venv out/jam-python
out/jam-python/Scripts/python.exe -m pip install -r server/requirements-local.txt
out/jam-python/Scripts/python.exe scripts/launch-mapping-jams-local.py
~~~

The Python launcher accepts --bundle, --state, --tools, --librequake and --port. Use --server-only to start or check the API without opening the editor. Startup diagnostics are in **out/jam-v2/api.log** and **out/jam-editor-clean/ftebroom/qconsole.log**. The updated instance uses its own data directory and keeps earlier local test data intact.

## Plot owners and local maps

The Jam board lists each plot owner once. The editor renews the session's presence every 10 seconds while the Jam screen or its editing workspace is active. A signed-out session is removed immediately. A session that stops responding expires from the presence list after 40 seconds.

Jam maps are saved by name inside the editor's local account and plot directories. **My maps** and **Open** in the Jam workspace list saved maps from that account's plots. **Compile** asks for a map name when the map has unsaved changes and starts once that revision is saved.

**Compile and upload map** sends a source snapshot to the Jam server and opens the compiled BSP in the viewport after publication. **Back to editing** restores the draft. **View compiled map** reopens the matching publication while the draft is unchanged. Plot tiles show their owner and the title of their current published map.

The viewport also offers **Play**, which compiles and publishes the current draft on the Jam server, then launches the published result. It requests a name for an unnamed map. **Browse Jam** opens the board. **RB Like** on another participant's plot transfers one giftable RB to the owner's ordinary balance. The button shows the total received through likes on that plot.

Select the center tile and choose **Play Hub** to enter the Quake Hub. When entering from a draft, **F10** opens a **Return to editing** button in the game. The server refreshes the Hub after purchases and publication changes. The editor checks the latest manifest on entry and reuses verified files. Each perimeter plot has a labeled portal with an indicator for free, claimed and playable plots. Entering an active portal loads the plot map staged when the Hub launched. The editable all-ports layout is in `runtime/maps/mapping-jam-hub.map`. The server builds its local variants from `mapping_jams/hub.py`.

## Registration, avatars and recovery

Use test credentials. Registration writes a verification token under **out/jam-v2/mail**. The filename is the SHA-256 of the lowercased email followed by .json. **Forgot password?** writes a separate .reset.json file in the same folder. Paste its token into the editor within 15 minutes. Reset consumes the code and revokes previous sessions. This local server uses an outbox for both flows. No real email is sent.

Set or remove an avatar in first-run profile setup or **My profile**. Windows provides an image picker. The Linux client accepts an image path. The server validates PNG, JPEG and WebP inputs of up to 4 MiB, center-crops and normalizes them to 128 × 128 PNG, and strips source metadata. Save the profile to make the image visible to other Jam participants.

## Moderation

Use **Report…** on another player's plot and choose the plot, published map or owner profile. The local operator reviews reports and can resolve or dismiss them. It can hide or restore a revision and restrict or restore an account. A hidden map stays unavailable for new launches until restored. Existing copies can remain on disk. Restricting an account revokes its sessions, removes its maps from play and blocks gifts to it. Restoring an account leaves its individual revision restrictions intact.

The launcher creates a separate **out/jam-v2/moderator.key** for this instance. Keep it local. The operator command reads the key from disk.

~~~powershell
out/jam-python/Scripts/python.exe server/moderate_mapping_jams.py reports
out/jam-python/Scripts/python.exe server/moderate_mapping_jams.py revision REVISION_ID hide --reason "Reviewed report"
out/jam-python/Scripts/python.exe server/moderate_mapping_jams.py report REPORT_ID resolve --reason "Publication hidden"
out/jam-python/Scripts/python.exe server/moderate_mapping_jams.py audit
~~~

See [OpenAPI](openapi.local.json) for participant and operator endpoints. Use --key-file and --port with the operator command for a different development instance.

## Tests

~~~powershell
out/jam-python/Scripts/python.exe server/test_mapping_jams.py --url http://127.0.0.1:8770 --state out/jam-v2
out/jam-python/Scripts/python.exe scripts/test-mapping-jams-editor.py --bundle out/jam-editor-clean --state out/jam-v2 --port 8770
~~~

Each full test consumes a plot. Use a fresh disposable data directory and a separate port for repeated tests. The editor test uses native document export, file verification and the FTE game process. It also exercises avatar loading, the report form's service action and password reset, then checks FTE's console for actual player admission. It is gated by test-only environment variables and ftebroom_skip_setup.

The service is intended for private development. Public mail, remote signed distribution, multi-event lifecycle, production deployment and the broader asset-upload rules remain outside this local implementation.
