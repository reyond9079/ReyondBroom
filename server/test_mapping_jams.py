"""End-to-end loopback test against the local Jam and the installed Quake 1 compiler."""

import argparse
import concurrent.futures
import io
import json
import sqlite3
import struct
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
import uuid
import zipfile
from pathlib import Path
from PIL import Image, PngImagePlugin

from mapping_jams.compiler import digest


def verify(base, state):
    def api(path, method="GET", body=None, token=None, key=None, expected=200, binary=False):
        headers = {"Connection": "close"}
        if token:
            headers["Authorization"] = "Bearer " + token
        if key:
            headers["Idempotency-Key"] = key
        if isinstance(body, dict):
            headers["Content-Type"] = "application/json"
            body = json.dumps(body).encode()
        request = urllib.request.Request(base + "/v1/" + path, data=body, headers=headers, method=method)
        try:
            result = urllib.request.urlopen(request, timeout=30)
        except urllib.error.HTTPError as error:
            result = error
        with result:
            data = result.read()
            assert result.status == expected, (path, expected, result.status, data[:400])
            return data if binary else json.loads(data) if data else None

    def new_account(label):
        nonce = uuid.uuid4().hex[:10]
        email = f"{label}-{nonce}@example.test"
        password = uuid.uuid4().hex
        api("auth/register", "POST", {"email": email, "password": password, "nickname": f"{label}-{nonce}"}, expected=202)
        verify_file = state / "mail" / (digest(email.encode()) + ".json")
        code = json.loads(verify_file.read_text())["token"]
        api("auth/email/verify", "POST", {"token": code})
        login = api("auth/login", "POST", {"email": email, "password": password})
        assert login["state"] == "profile_pending"
        token = login["accessToken"]
        profile = api("me/profile", "PATCH", {"bio": "Test author", "completeOnboarding": True}, token=token)
        assert profile["state"] == "ready"
        assert api(jam + "join", "POST", {}, token, expected=201)["wallet"]["spendableRb"] == 20
        assert api(jam + "join", "POST", {}, token)["wallet"]["giftableRb"] == 10
        return token, profile, email, password

    config = api("jams/current")["jam"]
    jam = "jams/" + config["jamId"] + "/"
    a, profile_a, email_a, password_a = new_account("author")
    b, profile_b, email_b, password_b = new_account("player")
    rules = api(jam + "rules", token=a)
    board = api(jam + "board", token=a)
    assert len(board["cells"]) == 25 and sum(c["kind"] == "hub" for c in board["cells"]) == 9
    assert {c["row"] + str(c["column"]) for c in board["cells"] if c["kind"] == "hub"} == {
        row + str(col) for row in "BCD" for col in (2, 3, 4)}
    free = next(c["plotId"] for c in board["cells"] if c["kind"] == "plot" and c["owner"] is None)
    original_hub = api(jam + "hub/manifest", token=a)
    buy = {"expectedRulesVersion": rules["rulesVersion"], "expectedPriceRb": 20}
    api(jam + "plots/C3/purchase", "POST", buy, a, uuid.uuid4().hex, expected=409)

    def race(token):
        key = uuid.uuid4().hex
        try:
            result = api(jam + f"plots/{free}/purchase", "POST", buy, token, key)
        except AssertionError as error:
            if error.args[0][2] != 409:
                raise
            return None
        again = api(jam + f"plots/{free}/purchase", "POST", buy, token, key)
        assert again == result and again["wallet"]["spendableRb"] == 0
        return token

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
        winners = [t for t in executor.map(race, [a, b]) if t]
    assert len(winners) == 1
    author = winners[0]
    player = b if author == a else a
    cutoff = time.monotonic()+10
    while len(list((state/"hub").glob("*.zip")))<2 and time.monotonic()<cutoff:
        time.sleep(.05)
    assert len(list((state/"hub").glob("*.zip")))>=2, "Buying land did not rebuild the Hub in the background."
    claimed_hub = api(jam + "hub/manifest", token=player)
    assert claimed_hub["hubVersion"] != original_hub["hubVersion"] and not claimed_hub["maps"]
    claimed_archive = api(claimed_hub["downloadPath"].removeprefix("/v1/"), token=player, binary=True)
    with zipfile.ZipFile(io.BytesIO(claimed_archive)) as package:
        assert f"Plot {free} - no playable map".encode() in package.read("maps/jam_hub.bsp")
    owner = (profile_a if author == a else profile_b)["accountId"]
    presence = jam + "presence"
    api(presence, "POST", {}, expected=401)
    assert api(presence, "POST", {}, player)["players"] == [
        {"accountId": owner, "nickname": (profile_a if author == a else profile_b)["nickname"],
         "plots": 1, "online": False}]
    assert api(presence, "POST", {}, author)["players"][0]["online"]
    assert api(presence, "POST", {}, player)["players"][0]["online"]
    with sqlite3.connect(state / "jam.sqlite3") as db:
        db.execute("UPDATE jam_presence SET seen=0 WHERE session=?", (digest(author.encode()),))
    assert not api(presence, "POST", {}, player)["players"][0]["online"]
    recipient = api("me", token=player)
    gift = {"recipientAccountId": recipient["accountId"], "amountRb": 3}
    key = uuid.uuid4().hex
    for _ in range(2):
        result = api(jam + "gifts", "POST", gift, author, key, expected=201)
        assert result["wallet"]["giftableRb"] == 7
    assert api(jam + "me/wallet", token=player)["spendableRb"] == 23
    like = jam + f"plots/{free}/likes"
    api(like, "POST", {}, author, uuid.uuid4().hex, expected=422)
    for count in (1, 2):
        key = uuid.uuid4().hex
        for _ in range(2):
            result = api(like, "POST", {}, player, key, expected=201)
            assert result["rbLikes"] == count and result["wallet"]["giftableRb"] == 10-count
    assert api(jam + "me/wallet", token=author)["spendableRb"] == 2
    assert next(cell for cell in api(jam + "board", token=author)["cells"] if cell.get("plotId")==free)["rbLikes"] == 2
    pack_path = jam + "packs/" + rules["packVersion"] + "/manifest"
    pack = api(pack_path, token=author)
    assert any(f["path"] == "plants/builtin/catalog.json" for f in pack["files"])
    assert any(f["path"] == "lq1/pak0.pak" for f in pack["files"])

    with tempfile.TemporaryDirectory() as temporary:
        destination = Path(temporary) / "room.map"
        fixture = Path(__file__).resolve().parents[1] / "tests" / "make-build-fixture.py"
        subprocess.run([sys.executable, str(fixture), str(destination)], check=True)
        source = destination.read_text().replace("ftebroom/grid", "wall_pink_b").replace(
            "info_player_deathmatch", "info_player_start").replace("1 1 0 0 0", "1 1").encode()
    state.joinpath("fixture.map").write_bytes(source)

    def publish(map_data, title, base_revision=None):
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            archive.writestr("manifest.json", json.dumps({"schemaVersion": 1, "sha256": digest(map_data)}))
            archive.writestr("map/source.map", map_data)
        snapshot = buffer.getvalue()
        upload = api(jam + f"plots/{free}/uploads", "POST",
                     {"title": title, "sourceSha256": digest(snapshot), "sourceSizeBytes": len(snapshot),
                      "baseRevisionId": base_revision, "rulesVersion": rules["rulesVersion"],
                      "packVersion": rules["packVersion"], "termsVersion": rules["termsVersion"]},
                     author, uuid.uuid4().hex, expected=201)
        upload_id = upload["uploadId"]
        api(upload["uploadPath"].removeprefix("/v1/"), "PUT", snapshot, author, expected=204)
        operation = api(jam + f"uploads/{upload_id}/finalize", "POST", {}, author, uuid.uuid4().hex, expected=202)
        assert operation["operationId"] == upload_id
        for _ in range(160):
            result = api(jam + f"operations/{upload_id}", token=author)
            if result["status"] in {"published", "failed"}:
                return result
            time.sleep(.25)
        raise AssertionError("Compilation exceeded the test deadline.")

    def check_hub(active_plots):
        manifest = api(jam + "hub/manifest", token=player)
        assert {entry["plotId"]: entry["revisionId"] for entry in manifest["maps"]} == active_plots
        artifact = api(manifest["downloadPath"].removeprefix("/v1/"), token=player, binary=True)
        assert digest(artifact) == manifest["sha256"]
        with zipfile.ZipFile(io.BytesIO(artifact)) as package:
            bsp = package.read("maps/jam_hub.bsp")
            version, offset, length = struct.unpack_from("<iii", bsp)
            entities = bsp[offset:offset+length]
            assert version == 29 and entities.count(b'"trigger_multiple"') == 16
            assert entities.count(b'"trigger_changelevel"') == len(active_plots)
            for plot in active_plots:
                assert f'"map" "jam_{plot.lower()}"'.encode() in entities
        return manifest

    check_hub({})
    published = publish(source, "Local test room")
    assert published["status"] == "published", published
    manifest = api(jam + f"plots/{free}/play-manifest", token=player)
    assert manifest["revisionId"] == published["revisionId"]
    live_hub = check_hub({free:published["revisionId"]})
    assert live_hub["hubVersion"] != claimed_hub["hubVersion"]
    artifact = api(manifest["downloadPath"].removeprefix("/v1/"), token=player, binary=True)
    assert digest(artifact) == manifest["sha256"]
    with zipfile.ZipFile(io.BytesIO(artifact)) as package:
        assert package.read("maps/jam_map.bsp").startswith(b"\x1d\x00\x00\x00")
        assert package.read("maps/jam_map.lit").startswith(b"QLIT")
    failed = publish(source.replace(b"0 -160 64", b"32000 -160 64"), "Oversized replacement", published["revisionId"])
    assert failed["status"] == "failed" and failed["error"]["code"] == "LIMIT_EXCEEDED", failed
    assert api(jam + f"plots/{free}/play-manifest", token=player)["revisionId"] == published["revisionId"]
    replacement = publish(source + b'\n{\n"classname" "light"\n"origin" "\'-48 0 64\'"\n}\n',
                          "Replacement test room", published["revisionId"])
    assert replacement["status"] == "published" and replacement["revisionId"] != published["revisionId"], replacement
    active = api(jam + f"plots/{free}/play-manifest", token=player)
    assert active["revisionId"] == replacement["revisionId"] and active["title"] == "Replacement test room"
    replaced_hub = check_hub({free:replacement["revisionId"]})
    assert live_hub["hubVersion"] != replaced_hub["hubVersion"]
    assert live_hub["downloadPath"] == replaced_hub["downloadPath"]

    # Avatars are canonicalized and private until assigned to a participant.
    buffer = io.BytesIO()
    metadata = PngImagePlugin.PngInfo()
    metadata.add_text("test-metadata", "must disappear")
    Image.new("RGBA", (360, 210), (25, 135, 179, 255)).save(buffer, "PNG", pnginfo=metadata)
    api("me/avatar", "POST", b"invalid image", author, expected=422)
    avatar_id = api("me/avatar", "POST", buffer.getvalue(), author, expected=201)["avatarId"]
    api("avatars/" + avatar_id, token=player, expected=404)
    api("me/profile", "PATCH", {"avatarId": avatar_id}, player, expected=422)
    profile = api("me/profile", "PATCH", {"avatarId": avatar_id}, author)
    avatar_path = profile["avatarUrl"].removeprefix("/v1/")
    with Image.open(io.BytesIO(api(avatar_path, token=player, binary=True))) as image:
        assert image.size == (128, 128) and image.format == "PNG" and not image.info

    # Operator routes require the separate key; a participant never gains those rights.
    moderator = (state / "moderator.key").read_text(encoding="ascii").strip()
    api("admin/reports", token=player, expected=403)
    target = active["revisionId"]
    report_body = {"targetKind": "revision", "targetId": target, "reasonCode": "broken_map", "note": "Test report"}
    report_key = uuid.uuid4().hex
    report = api(jam + "reports", "POST", report_body, player, report_key, expected=202)
    assert api(jam + "reports", "POST", report_body, player, report_key, expected=202) == report
    api(jam + "reports", "POST", report_body, player, uuid.uuid4().hex, expected=409)
    queue = api("admin/reports", token=moderator)["reports"]
    item = next(item for item in queue if item["id"] == report["reportId"])
    assert item["status"] == "open" and "reporter" not in item
    api("admin/revisions/" + target, "PATCH", {"action": "hide", "reason": "Validate visibility"}, moderator)
    api(jam + f"plots/{free}/play-manifest", token=player, expected=404)
    check_hub({})
    api(active["downloadPath"].removeprefix("/v1/"), token=player, expected=404)
    cells = api(jam + "board", token=player)["cells"]
    assert next(cell for cell in cells if cell.get("plotId")==free)["publication"] == {
        "revisionId": target, "playable": False}
    api("admin/reports/" + report["reportId"], "PATCH", {"action": "resolve", "reason": "Checked"}, moderator)
    api("admin/revisions/" + target, "PATCH", {"action": "restore", "reason": "Restore fixture"}, moderator)
    assert api(jam + f"plots/{free}/play-manifest", token=player)["revisionId"] == target
    check_hub({free:target})
    author_id = profile["accountId"]
    api("admin/accounts/" + author_id, "PATCH", {"action": "restrict", "reason": "Check suspension"}, moderator)
    api("me", token=author, expected=401)
    api(avatar_path, token=player, expected=404)
    api(jam + f"plots/{free}/play-manifest", token=player, expected=404)
    api("admin/accounts/" + author_id, "PATCH", {"action": "restore", "reason": "Restore fixture"}, moderator)
    assert any(event["action"]=="accounts/restore" for event in api("admin/audit", token=moderator)["events"])

    # Password recovery is generic for unknown email, single-use and revokes every session.
    email, original = (email_a, password_a) if author == a else (email_b, password_b)
    author = api("auth/login", "POST", {"email": email, "password": original})["accessToken"]
    session = api("auth/login", "POST", {"email": email, "password": original})["accessToken"]
    requested = api("auth/password/forgot", "POST", {"email":email}, expected=202)
    assert api("auth/password/forgot", "POST", {"email": "absent@example.test"}, expected=202) == requested
    reset_file = state / "mail" / (digest(email.encode()) + ".reset.json")
    first_token = json.loads(reset_file.read_text())["token"]
    api("auth/password/forgot", "POST", {"email":email}, expected=202)
    assert json.loads(reset_file.read_text())["token"] == first_token
    # Use only the newly created test account to exercise expiry without a 15-minute wait.
    with sqlite3.connect(state / "jam.sqlite3") as db:
        db.execute("UPDATE password_resets SET expires=0, created=0 WHERE token=?", (digest(first_token.encode()),))
    api("auth/password/reset", "POST", {"token":first_token,"password":uuid.uuid4().hex}, expected=422)
    api("auth/password/forgot", "POST", {"email":email}, expected=202)
    reset = {"token":json.loads(reset_file.read_text())["token"], "password":uuid.uuid4().hex}
    assert reset["token"] != first_token
    api("auth/password/reset", "POST", reset)
    api("auth/password/reset", "POST", reset, expected=422)
    api("me", token=author, expected=401)
    api("me", token=session, expected=401)
    api("auth/login", "POST", {"email":email,"password":original}, expected=401)
    author = api("auth/login", "POST", {"email":email,"password":reset["password"]})["accessToken"]
    assert api("me/profile", "PATCH", {"avatarId":None}, author)["avatarUrl"] is None
    api(avatar_path, token=player, expected=404)
    print("PASS: Jam publication and economy, avatar upload/removal, password recovery and revocation, reports and audited moderation.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--state", type=Path, required=True)
    parser.add_argument("--url", default="http://127.0.0.1:8765")
    args = parser.parse_args()
    verify(args.url, args.state)
