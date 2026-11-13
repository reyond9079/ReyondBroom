# Mapping Jams Linux staging

This branch packages the server for the existing Mapping Jams protocol. API revision 5 uses a local mail outbox. Use this setup for private staging over SSH while public mail delivery and isolated compilation are prepared. Keep port 8765 on loopback and leave this revision behind the SSH tunnel.

## Install

1. Check out `codex/mapping-jams-server` at `/opt/mapping-jams`. Record `git rev-parse HEAD`. Keep source and dependencies readable by the service account and writable only by the deployment operator.
2. Create the unprivileged system user and group `mapping-jams` with no login shell. Create `/opt/mapping-jams-venv` with Python 3.12 or newer and install the dependencies below.

   ```sh
   python3 -m venv /opt/mapping-jams-venv
   /opt/mapping-jams-venv/bin/python -m pip install -r /opt/mapping-jams/server/requirements-local.txt
   ```

3. Install the Linux `qbsp` and `light` executables from ericw-tools in `/opt/mapping-jams-deps/ericw`, with their shared libraries as required. The reference version is 0.18.1. The official ReyondBroom Linux distribution includes a compatible `tools/ericw` directory. Check both executables with `ldd` and record their versions. See the [tool source and releases](https://github.com/ericwa/ericw-tools).
4. Download the pinned LibreQuake Mod Lite and Dev WADs using the existing checksum-verifying downloader. The service expects `mod_lite/lq1/pak0.pak` and `mod_lite/lq1/texture-wads/`. The Plant Generator catalog comes from the same checkout.

   ```sh
   python3 /opt/mapping-jams/runtime/download-librequake.py \
     --destination /opt/mapping-jams-deps/librequake
   ```

5. Create `/var/lib/mapping-jams` with owner `mapping-jams:mapping-jams` and mode `0700`. Create `/var/lib/mapping-jams/moderator.key` once with a random URL-safe secret of 32 bytes or more, the same owner and mode `0600`. Do not print its contents. Keep the state directory outside Git. Back up the SQLite database and associated upload and build files together while the service is stopped, or use a consistent snapshot.
6. Install `mapping-jams.service` in `/etc/systemd/system/`. Verify it with `systemd-analyze verify`, then enable and start it. Check `journalctl -u mapping-jams` if startup fails.

   ```sh
   sudo systemctl daemon-reload
   sudo systemctl enable --now mapping-jams
   curl --fail --silent --show-error http://127.0.0.1:8765/v1/jams/current
   ```

The response should contain `jamId=local-first-jam`, `localApiRevision=5`, `rulesVersion=local-v1` and `packVersion=librequake-0.09-plants-local1`.

## Verify with disposable data

Run the API under the service account with the same dependencies and a fresh test data directory owned by that account. Generate a separate `moderator.key` there. Use a second loopback port such as `8766`. The integration suite uses the local outbox and consumes a plot, so point it only at the disposable instance.

```sh
/opt/mapping-jams-venv/bin/python /opt/mapping-jams/server/test_mapping_jams.py \
  --url http://127.0.0.1:8766 --state /path/to/disposable-state
```

For a private native-client test, run the tunnel on the computer where you run the editor, using any free local port. The local client’s `REYONDBROOM_JAM_PORT` should match that local port. The backend continues to listen on 8765 on the server.

```sh
ssh -N -o ExitOnForwardFailure=yes -L 127.0.0.1:8790:127.0.0.1:8765 USER@SERVER
curl --fail http://127.0.0.1:8790/v1/jams/current
```

## Return to the editor developer

Send the intended HTTPS base URL, commit SHA, non-sensitive `/v1/jams/current` response, architecture and tool versions, test summary, and the status of DNS/TLS, real email delivery and compiler isolation. Keep moderator credentials, SSH private keys, player credentials and mail tokens on the server.
