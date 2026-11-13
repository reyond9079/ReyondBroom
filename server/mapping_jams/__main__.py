"""Start a private Mapping Jams development instance on loopback."""

import argparse
import concurrent.futures
import contextlib
import datetime
import hashlib
import hmac
import json
import re
import secrets
import sqlite3
import subprocess
import tempfile
import threading
import time
import uuid
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

from .compiler import MAX_ARTIFACT, MAX_SOURCE, MapError, build_map, digest
from . import hub
from .participant import MAX_AVATAR, REPORT_REASONS, avatar_bytes

JAM = "local-first-jam"
RULES_VERSION = "local-v1"
PACK = "librequake-0.09-plants-local1"
TERMS = "local-test-v1"
BOARD = [(row, col, "hub" if row in "BCD" and 2 <= col <= 4 else "plot")
         for row in "ABCDE" for col in range(1, 6)]
RULES = {"jamId": JAM, "rulesVersion": RULES_VERSION, "packVersion": PACK,
         "rows": 5, "columns": 5, "priceRb": 20, "initialSpendableRb": 20, "initialGiftableRb": 10,
         "maxBrushes": 512, "maxEntities": 256, "bounds": {"maxSize": [2048, 2048, 1024], "worldLimit": 4096},
         "maxSourceBytes": MAX_SOURCE, "maxPlayBytes": MAX_ARTIFACT, "termsVersion": TERMS}
EMAIL = re.compile(r"[^@\s]{1,120}@[^@\s.]+(?:\.[^@\s.]+)+")
SECRET = re.compile(r"[A-Za-z0-9_-]{20,120}")
UUID = re.compile(r"[a-f0-9]{32}")
PRESENCE_SECONDS = 40

SCHEMA = """
CREATE TABLE IF NOT EXISTS accounts (
 id TEXT PRIMARY KEY, email TEXT UNIQUE NOT NULL, nickname TEXT NOT NULL,
 password TEXT NOT NULL, verified INTEGER NOT NULL DEFAULT 0, ready INTEGER NOT NULL DEFAULT 0,
 bio TEXT NOT NULL DEFAULT '');
CREATE UNIQUE INDEX IF NOT EXISTS nickname_unique ON accounts(lower(nickname));
CREATE TABLE IF NOT EXISTS challenges (
 token TEXT PRIMARY KEY, account TEXT NOT NULL REFERENCES accounts(id), expires INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS sessions (
 token TEXT PRIMARY KEY, account TEXT NOT NULL REFERENCES accounts(id), expires INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS jam_presence (
 session TEXT PRIMARY KEY REFERENCES sessions(token) ON DELETE CASCADE, seen INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS wallets (
 account TEXT PRIMARY KEY REFERENCES accounts(id), ordinary INTEGER NOT NULL CHECK(ordinary>=0),
 giftable INTEGER NOT NULL CHECK(giftable>=0), version INTEGER NOT NULL DEFAULT 1);
CREATE TABLE IF NOT EXISTS ledger (
 id TEXT PRIMARY KEY, kind TEXT NOT NULL, account TEXT NOT NULL REFERENCES accounts(id),
 ordinary INTEGER NOT NULL, giftable INTEGER NOT NULL, created INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS plots (
 id TEXT PRIMARY KEY, owner TEXT REFERENCES accounts(id), revision TEXT);
CREATE TABLE IF NOT EXISTS rb_likes (
 gift TEXT PRIMARY KEY REFERENCES ledger(id), plot TEXT NOT NULL REFERENCES plots(id));
CREATE TABLE IF NOT EXISTS idempotency (
 account TEXT NOT NULL REFERENCES accounts(id), key TEXT NOT NULL, request TEXT NOT NULL,
 status INTEGER NOT NULL, response TEXT NOT NULL, PRIMARY KEY(account,key));
CREATE TABLE IF NOT EXISTS uploads (
 id TEXT PRIMARY KEY, account TEXT NOT NULL REFERENCES accounts(id), plot TEXT NOT NULL REFERENCES plots(id),
 title TEXT NOT NULL, base TEXT, checksum TEXT NOT NULL, size INTEGER NOT NULL, expires INTEGER NOT NULL,
 status TEXT NOT NULL, error TEXT, files TEXT, checksum_play TEXT, size_play INTEGER);
CREATE TABLE IF NOT EXISTS board_version (id INTEGER PRIMARY KEY CHECK(id=1), version INTEGER NOT NULL);
INSERT OR IGNORE INTO board_version VALUES (1, 1);
CREATE TABLE IF NOT EXISTS password_resets (
 token TEXT PRIMARY KEY, account TEXT UNIQUE NOT NULL REFERENCES accounts(id), expires INTEGER NOT NULL,
 created INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS avatars (
 id TEXT PRIMARY KEY, owner TEXT NOT NULL REFERENCES accounts(id), png BLOB NOT NULL);
CREATE TABLE IF NOT EXISTS profile_avatars (
 account TEXT PRIMARY KEY REFERENCES accounts(id), avatar TEXT NOT NULL REFERENCES avatars(id));
CREATE TABLE IF NOT EXISTS reports (
 id TEXT PRIMARY KEY, reporter TEXT NOT NULL REFERENCES accounts(id), kind TEXT NOT NULL,
 target TEXT NOT NULL, reason TEXT NOT NULL, note TEXT NOT NULL, created INTEGER NOT NULL,
 status TEXT NOT NULL DEFAULT 'open', resolution TEXT);
CREATE TABLE IF NOT EXISTS restricted_accounts (
 account TEXT PRIMARY KEY REFERENCES accounts(id));
CREATE TABLE IF NOT EXISTS hidden_revisions (
 revision TEXT PRIMARY KEY REFERENCES uploads(id));
CREATE TABLE IF NOT EXISTS moderation_log (
 id TEXT PRIMARY KEY, actor TEXT NOT NULL, action TEXT NOT NULL,
 target TEXT NOT NULL, reason TEXT NOT NULL, created INTEGER NOT NULL);
"""


class Problem(Exception):
    def __init__(self, status, code, title, **fields):
        super().__init__(title)
        self.status = status
        self.value = {"type": "about:blank", "title": title, "status": status, "code": code, **fields}


def fail(status, code, title, **fields):
    raise Problem(status, code, title, **fields)


def password_hash(password, salt=None):
    salt = salt or secrets.token_bytes(16)
    value = hashlib.scrypt(password.encode("utf-8"), salt=salt, n=32768, r=8, p=1, maxmem=64*1024*1024)
    return salt.hex() + ":" + value.hex()


def check_password(stored, password):
    return hmac.compare_digest(stored, password_hash(password, bytes.fromhex(stored.split(":")[0])))


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def profile(db, row):
    account = row["id"]
    restricted = db.execute("SELECT 1 FROM restricted_accounts WHERE account=?", (account,)).fetchone()
    avatar = db.execute("SELECT avatar FROM profile_avatars WHERE account=?", (account,)).fetchone()
    return {"accountId": account, "nickname": row["nickname"], "bio": "" if restricted else row["bio"],
            "avatarId": avatar[0] if avatar and not restricted else None,
            "avatarUrl": f"/v1/avatars/{avatar[0]}" if avatar and not restricted else None,
            "restricted": bool(restricted)}


def account_state(row):
    return "email_pending" if not row["verified"] else "profile_pending" if not row["ready"] else "ready"


def bounded_text(value, limit, label, optional=False):
    if not isinstance(value, str):
        fail(422, "INVALID_FIELD", f"Invalid {label}.")
    value = value.strip()
    if (not optional and not value) or len(value) > limit or any(ord(c) < 32 and not (optional and c == "\n") for c in value):
        fail(422, "INVALID_FIELD", f"Invalid {label}.")
    return value


class Service:
    def __init__(self, root, tools, libre, preset, moderator_file=None):
        self.root, self.tools, self.libre = root, tools, libre
        self.work = root / "builds"
        self.outbox = root / "mail"
        self.incoming = root / "uploads"
        self.hub_cache = root / "hub"
        self.hub_lock = threading.Lock()
        for folder in (root, self.work, self.outbox, self.incoming, self.hub_cache):
            folder.mkdir(parents=True, exist_ok=True)
        self.database = root / "jam.sqlite3"
        self.moderator_secret = moderator_file.read_text(encoding="ascii").strip() if moderator_file else ""
        if moderator_file and not SECRET.fullmatch(self.moderator_secret):
            raise ValueError("Invalid moderator key file.")
        with contextlib.closing(sqlite3.connect(self.database)) as db:
            db.execute("PRAGMA journal_mode=WAL")
        self.executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)
        self.hub_executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)
        self.wads = sorted((libre / "texture-wads").glob("*.wad"))
        if not self.wads or not (libre / "pak0.pak").is_file() or not preset.is_file():
            raise ValueError("Expected LibreQuake mod directory and the built-in Plant Generator catalog.")
        catalog = json.loads(preset.read_text(encoding="utf-8"))
        if catalog.get("format") != "ReyondBroomBotanicalCatalog" or not catalog.get("presets"):
            raise ValueError("Expected a nonempty ReyondBroom Plant Generator catalog.")
        if any('"' in p.as_posix() or ";" in p.as_posix() for p in self.wads):
            raise ValueError("Unsupported trusted WAD path.")
        with self.connection() as db:
            db.executescript(SCHEMA)
            db.executemany("INSERT OR IGNORE INTO plots(id) VALUES (?)", [(r+str(c),) for r,c,k in BOARD if k == "plot"])
            queued = [row[0] for row in db.execute("SELECT id FROM uploads WHERE status IN ('queued','compiling')")]
            db.execute("UPDATE uploads SET status='queued' WHERE status='compiling'")
        self.pack_path = root / "base-pack.zip"
        self.pack_manifest = self.build_pack(preset)
        for operation in queued:
            self.executor.submit(self.compile, operation)
        self.queue_hub_refresh()

    def queue_hub_refresh(self):
        self.hub_executor.submit(self.refresh_hub)

    def refresh_hub(self):
        try:
            with self.connection() as db:
                self.hub_manifest(db)
        except (Problem, OSError, sqlite3.Error) as error:
            print(f"Could not refresh the Hub: {error}", flush=True)

    @contextlib.contextmanager
    def connection(self, write=False):
        db = sqlite3.connect(self.database, timeout=15, isolation_level=None)
        db.row_factory = sqlite3.Row
        db.execute("PRAGMA foreign_keys=ON")
        try:
            db.execute("BEGIN IMMEDIATE" if write else "BEGIN")
            yield db
            if db.in_transaction:
                db.commit()
        except BaseException:
            if db.in_transaction:
                db.rollback()
            raise
        finally:
            db.close()

    def build_pack(self, preset):
        sources = [("lq1/" + p.name, p) for p in sorted(self.libre.glob("pak[01].pak"))]
        sources.extend(("lq1/texture-wads/" + p.name, p) for p in self.wads)
        sources.append(("plants/builtin/catalog.json", preset))
        files = [{"path": name, "sizeBytes": source.stat().st_size,
                  "sha256": digest(source.read_bytes())} for name, source in sources]
        with zipfile.ZipFile(self.pack_path, "w", zipfile.ZIP_STORED) as archive:
            for name, source in sources:
                archive.write(source, name)
        return {"schemaVersion": 1, "kind": "base_pack", "packVersion": PACK,
                "sizeBytes": self.pack_path.stat().st_size, "sha256": digest(self.pack_path.read_bytes()),
                "files": files, "downloadPath": f"/v1/jams/{JAM}/packs/{PACK}/download"}

    def hub_manifest(self, db):
        plots = db.execute("SELECT * FROM plots WHERE owner IS NOT NULL ORDER BY id").fetchall()
        maps = []
        for plot in plots:
            publication = self.publication(db, plot)
            if publication and publication["playable"]:
                revision = db.execute("SELECT * FROM uploads WHERE id=?", (plot["revision"],)).fetchone()
                maps.append(dict(self.play_manifest(revision), plotId=plot["id"]))
        source = hub.source({entry["plotId"] for entry in maps}, {plot["id"] for plot in plots})
        key = digest(PACK.encode("ascii") + source)
        version = digest(json.dumps([key, [(entry["plotId"], entry["revisionId"]) for entry in maps]]).encode())
        artifact = self.hub_cache / (key + ".zip")
        with self.hub_lock:
            if not artifact.is_file():
                with tempfile.TemporaryDirectory(dir=self.hub_cache) as directory:
                    try:
                        compiled, _ = build_map(source, Path(directory) / "build", RULES, self.tools, self.wads, hub=True)
                    except (MapError, subprocess.TimeoutExpired) as error:
                        print(f"Could not compile the first-party Hub: {error}", flush=True)
                        fail(503, "HUB_UNAVAILABLE", "Could not compile the Jam Hub.")
                    compiled.replace(artifact)
            with zipfile.ZipFile(artifact) as archive:
                files = [{"path": entry.filename, "sizeBytes": entry.file_size,
                          "sha256": digest(archive.read(entry))} for entry in archive.infolist()]
            return {"schemaVersion": 1, "kind": "hub", "hubVersion": version, "packVersion": PACK, "mapStem": "jam_hub",
                    "sizeBytes": artifact.stat().st_size, "sha256": digest(artifact.read_bytes()),
                    "files": files, "maps": maps, "downloadPath": f"/v1/jams/{JAM}/hub/{key}/download"}

    @staticmethod
    def play_manifest(revision):
        return {"schemaVersion": 1, "kind": "play_map", "revisionId": revision["id"],
                "title": revision["title"], "packVersion": PACK, "rulesVersion": RULES_VERSION,
                "mapStem": "jam_map", "sizeBytes": revision["size_play"], "sha256": revision["checksum_play"],
                "files": json.loads(revision["files"]), "downloadPath": f"/v1/jams/{JAM}/revisions/{revision['id']}/download"}

    def authenticate(self, db, header):
        scheme, _, token = header.partition(" ")
        if scheme != "Bearer" or not SECRET.fullmatch(token):
            fail(401, "AUTH_REQUIRED", "Sign in to continue.")
        account = db.execute(
            "SELECT a.* FROM sessions s JOIN accounts a ON a.id=s.account WHERE s.token=? AND s.expires>?",
            (digest(token.encode()), int(time.time()))).fetchone()
        if account is None:
            fail(401, "AUTH_REQUIRED", "Sign in again.")
        if db.execute("SELECT 1 FROM restricted_accounts WHERE account=?", (account["id"],)).fetchone():
            fail(403, "ACCOUNT_RESTRICTED", "This account is restricted in the Jam.")
        return account

    def authorize(self, account):
        if not account["verified"]:
            fail(403, "EMAIL_UNVERIFIED", "Verify your email to continue.")
        if not account["ready"]:
            fail(403, "PROFILE_REQUIRED", "Complete your profile to continue.")

    def wallet(self, db, account):
        row = db.execute("SELECT * FROM wallets WHERE account=?", (account,)).fetchone()
        if row is None:
            fail(403, "MEMBERSHIP_REQUIRED", "Join the Jam first.")
        return {"spendableRb": row["ordinary"], "giftableRb": row["giftable"], "walletVersion": row["version"]}

    def publication(self, db, plot):
        if not plot["revision"]:
            return None
        rev = db.execute("SELECT * FROM uploads WHERE id=?", (plot["revision"],)).fetchone()
        author = db.execute("SELECT * FROM accounts WHERE id=?", (rev["account"],)).fetchone()
        public_author = profile(db, author)
        if public_author["restricted"] or db.execute("SELECT 1 FROM hidden_revisions WHERE revision=?", (rev["id"],)).fetchone():
            return {"revisionId": rev["id"], "playable": False}
        return {"revisionId": rev["id"], "title": rev["title"], "author": public_author,
                "packVersion": PACK, "playable": True}

    def plot(self, db, plot):
        item = db.execute("SELECT * FROM plots WHERE id=?", (plot,)).fetchone()
        if item is None:
            if any(r+str(c) == plot and k == "hub" for r,c,k in BOARD):
                fail(409, "PLOT_RESERVED", "The Hub is reserved.")
            fail(404, "NOT_FOUND", "Plot not found.")
        return item

    def board(self, db):
        plots = {r["id"]: r for r in db.execute("SELECT * FROM plots")}
        likes = dict(db.execute("SELECT plot, count(*) FROM rb_likes GROUP BY plot"))
        cells = []
        for row, col, kind in BOARD:
            cell = {"row": row, "column": col, "kind": kind}
            if kind == "plot":
                plot = plots[row+str(col)]
                owner = db.execute("SELECT * FROM accounts WHERE id=?", (plot["owner"],)).fetchone()
                cell.update(plotId=plot["id"], priceRb=20, rbLikes=likes.get(plot["id"], 0), owner=profile(db, owner) if owner else None,
                            publication=self.publication(db, plot))
            cells.append(cell)
        return {"jamId": JAM, "rulesVersion": RULES_VERSION,
                "boardRevision": db.execute("SELECT version FROM board_version").fetchone()[0], "cells": cells}

    def gift(self, db, sender, recipient, amount, plot=None):
        if not isinstance(recipient, str) or not UUID.fullmatch(recipient):
            fail(422, "RECIPIENT_NOT_ELIGIBLE", "Choose a participant.")
        if recipient == sender:
            fail(422, "SELF_GIFT", "You cannot give RB to yourself.")
        if type(amount) is not int or not 1 <= amount <= 10:
            fail(422, "INVALID_FIELD", "Enter a positive whole number of RB.")
        if not db.execute("SELECT 1 FROM wallets WHERE account=?", (recipient,)).fetchone() or db.execute(
                "SELECT 1 FROM restricted_accounts WHERE account=?", (recipient,)).fetchone():
            fail(422, "RECIPIENT_NOT_ELIGIBLE", "This participant cannot receive gifts.")
        if self.wallet(db, sender)["giftableRb"] < amount:
            fail(409, "INSUFFICIENT_GIFT_RB", "Not enough giftable RB.")
        gid = uuid.uuid4().hex
        db.execute("UPDATE wallets SET giftable=giftable-?,version=version+1 WHERE account=?", (amount,sender))
        db.execute("UPDATE wallets SET ordinary=ordinary+?,version=version+1 WHERE account=?", (amount,recipient))
        now = int(time.time())
        db.executemany("INSERT INTO ledger VALUES (?,?,?,?,?,?)",
                       [(gid, "gift_out", sender, 0, -amount, now),
                        (uuid.uuid4().hex, "gift_in", recipient, amount, 0, now)])
        result = {"giftId": gid, "wallet": self.wallet(db, sender)}
        if plot:
            db.execute("INSERT INTO rb_likes VALUES (?,?)", (gid, plot))
            db.execute("UPDATE board_version SET version=version+1")
            result["rbLikes"] = db.execute("SELECT count(*) FROM rb_likes WHERE plot=?", (plot,)).fetchone()[0]
        return 201, result

    def presence(self, db, authorization):
        now = int(time.time())
        session = digest(authorization.split(" ", 1)[1].encode())
        db.execute("DELETE FROM jam_presence WHERE seen<=?", (now-PRESENCE_SECONDS,))
        db.execute("INSERT INTO jam_presence VALUES (?,?) ON CONFLICT(session) DO UPDATE SET seen=excluded.seen",
                   (session, now))
        owners = db.execute("""
            SELECT a.id, a.nickname, COUNT(*) AS plots,
                   EXISTS (SELECT 1 FROM sessions s JOIN jam_presence p ON p.session=s.token
                           WHERE s.account=a.id AND s.expires>?) AS online
            FROM plots JOIN accounts a ON a.id=plots.owner
            GROUP BY a.id ORDER BY a.nickname COLLATE NOCASE, a.id
            """, (now,))
        return {"players": [{"accountId": row["id"], "nickname": row["nickname"],
                             "plots": row["plots"], "online": bool(row["online"])} for row in owners]}

    def compile(self, operation):
        with self.connection(True) as db:
            row = db.execute("SELECT * FROM uploads WHERE id=? AND status='queued'", (operation,)).fetchone()
            if row is None:
                return
            db.execute("UPDATE uploads SET status='compiling' WHERE id=?", (operation,))
        try:
            output = self.work / (operation + "-" + uuid.uuid4().hex)
            source = (self.incoming / (operation + ".bin")).read_bytes()
            artifact, files = build_map(source, output, RULES, self.tools, self.wads)
            checksum = digest(artifact.read_bytes())
            with self.connection(True) as db:
                current = db.execute("SELECT * FROM uploads WHERE id=?", (operation,)).fetchone()
                plot = self.plot(db, row["plot"])
                if current["status"] != "compiling":
                    return
                if db.execute("SELECT 1 FROM restricted_accounts WHERE account=?", (row["account"],)).fetchone():
                    fail(403, "ACCOUNT_RESTRICTED", "This account is restricted in the Jam.")
                if plot["revision"] != row["base"] or plot["owner"] != row["account"]:
                    fail(409, "REVISION_CONFLICT", "The plot changed during compilation.")
                final = self.work / (operation + ".zip")
                artifact.replace(final)
                db.execute("UPDATE uploads SET status='published', files=?, checksum_play=?, size_play=? WHERE id=?",
                           (json.dumps(files), checksum, final.stat().st_size, operation))
                db.execute("UPDATE plots SET revision=? WHERE id=?", (operation, row["plot"]))
                db.execute("UPDATE board_version SET version=version+1")
        except (MapError, Problem, OSError, subprocess.TimeoutExpired) as error:
            if isinstance(error, Problem):
                problem = error.value
            elif isinstance(error, MapError):
                problem = {"code": "LIMIT_EXCEEDED" if error.violations else "BUILD_FAILED",
                           "title": str(error), "violations": error.violations}
            else:
                problem = {"code": "BUILD_FAILED", "title": "The local compiler could not finish."}
            with self.connection(True) as db:
                db.execute("UPDATE uploads SET status='failed', error=? WHERE id=? AND status='compiling'",
                           (json.dumps(problem), operation))
        else:
            self.queue_hub_refresh()

    def local_verification(self, account_id, email):
        token = secrets.token_urlsafe(32)
        with self.connection(True) as db:
            db.execute("DELETE FROM challenges WHERE account=?", (account_id,))
            db.execute("INSERT INTO challenges VALUES (?,?,?)",
                       (digest(token.encode()), account_id, int(time.time())+3600))
        self.write_mail(email, token)

    def write_mail(self, email, token, recovery=False):
        suffix = ".reset.json" if recovery else ".json"
        destination = self.outbox / (digest(email.encode()) + suffix)
        temp = destination.with_suffix("." + uuid.uuid4().hex + ".tmp")
        temp.write_text(json.dumps({"email": email, "token": token}), encoding="utf-8")
        temp.replace(destination)


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "ReyondBroomJamLocal/1"

    def log_message(self, format, *args):
        pass

    def do_GET(self): self.dispatch()
    def do_POST(self): self.dispatch()
    def do_PATCH(self): self.dispatch()
    def do_PUT(self): self.dispatch()

    def send_value(self, status, value=None, kind="application/json", extra=None):
        if isinstance(value, Path):
            size = value.stat().st_size
        else:
            data = value if isinstance(value, bytes) else b"" if value is None else json.dumps(value, ensure_ascii=False).encode("utf-8")
            size = len(data)
        self.send_response(status)
        self.send_header("Content-Length", str(size))
        self.send_header("Content-Type", kind)
        self.send_header("Cache-Control", "no-store")
        for key, content in (extra or {}).items():
            self.send_header(key, content)
        self.end_headers()
        if isinstance(value, Path):
            with value.open("rb") as source:
                while chunk := source.read(128*1024):
                    self.wfile.write(chunk)
        elif data:
            self.wfile.write(data)

    def read(self, max_size):
        length = self.headers.get("Content-Length", "0")
        if not length.isdecimal() or len(length) > 9 or int(length) > max_size:
            self.close_connection = True
            fail(413, "BODY_TOO_LARGE", "Request body too large.")
        return self.rfile.read(int(length))

    def dispatch(self):
        self.close_connection = True
        self.connection.settimeout(30)
        self.request_id = uuid.uuid4().hex
        self.service = self.server.service
        self.after = []
        try:
            if self.headers.get("Host", "").split(":")[0] != "127.0.0.1":
                fail(403, "LOCAL_ONLY", "This instance accepts loopback requests only.")
            if self.headers.get("Origin") or self.headers.get("Transfer-Encoding"):
                fail(403, "LOCAL_ONLY", "Unsupported request origin or transfer encoding.")
            route = urlsplit(self.path).path
            method = self.command
            body = None
            if method == "POST" and route == "/v1/me/avatar":
                body = self.read(MAX_AVATAR)
            elif method == "PUT":
                body = self.read(MAX_SOURCE)
            elif method in {"POST", "PATCH"}:
                raw = self.read(32*1024)
                body = json.loads(raw) if raw else {}
                if not isinstance(body, dict):
                    fail(400, "INVALID_REQUEST", "Expected a JSON object.")
            if method == "POST" and route == "/v1/me/avatar":
                with self.service.connection() as db:
                    user = self.service.authenticate(db, self.headers.get("Authorization", ""))
                    if not user["verified"]:
                        fail(403, "EMAIL_UNVERIFIED", "Verify your email to continue.")
                try:
                    body = avatar_bytes(body)
                except ValueError as error:
                    fail(422, "AVATAR_INVALID", str(error))
            if route.startswith("/v1/auth/"):
                response = self.auth_route(route, method, body)
            elif route.startswith("/v1/admin/"):
                with self.service.connection(method != "GET") as db:
                    response = self.moderator_route(db, route, method, body)
            else:
                with self.service.connection(method != "GET") as db:
                    response = self.route(db, route, method, body)
            for action in self.after:
                action()
            self.send_value(*response)
        except Problem as error:
            self.send_value(error.status, {**error.value, "requestId": self.request_id}, "application/problem+json")
        except (BrokenPipeError, ConnectionResetError):
            self.close_connection = True
        except (ValueError, UnicodeError):
            self.send_value(400, {"code": "INVALID_REQUEST", "title": "Invalid request.", "requestId": self.request_id},
                            "application/problem+json")
        except (sqlite3.Error, OSError):
            self.send_value(503, {"code": "SERVICE_UNAVAILABLE", "title": "Local service unavailable.",
                                  "requestId": self.request_id}, "application/problem+json")

    def auth_route(self, route, method, body):
        if method != "POST":
            fail(404, "NOT_FOUND", "Endpoint not found.")
        s = self.service
        if route == "/v1/auth/register":
            email = bounded_text(body.get("email"), 180, "email").lower()
            nickname = bounded_text(body.get("nickname"), 40, "nickname")
            password = body.get("password")
            if not EMAIL.fullmatch(email) or not isinstance(password, str) or not 12 <= len(password) <= 128:
                fail(422, "INVALID_FIELD", "Enter a valid email and a password of 12–128 characters.")
            identifier = uuid.uuid4().hex
            stored = password_hash(password)
            with s.connection(True) as db:
                if db.execute("SELECT 1 FROM accounts WHERE email=?", (email,)).fetchone():
                    return 202, {"state": "email_pending"}
                try:
                    db.execute("INSERT INTO accounts(id,email,nickname,password) VALUES (?,?,?,?)",
                               (identifier, email, nickname, stored))
                except sqlite3.IntegrityError:
                    fail(409, "NICKNAME_TAKEN", "Choose another nickname.")
            s.local_verification(identifier, email)
            return 202, {"state": "email_pending"}
        if route == "/v1/auth/email/verify":
            token = body.get("token", "")
            if not isinstance(token, str) or not SECRET.fullmatch(token):
                fail(422, "INVALID_TOKEN", "Enter the confirmation code.")
            with s.connection(True) as db:
                key = digest(token.encode())
                row = db.execute("SELECT * FROM challenges WHERE token=? AND expires>?",
                                 (key, int(time.time()))).fetchone()
                if not row:
                    fail(422, "INVALID_TOKEN", "The confirmation code is invalid or expired.")
                db.execute("UPDATE accounts SET verified=1 WHERE id=?", (row["account"],))
                db.execute("DELETE FROM challenges WHERE token=?", (key,))
            return 200, {"state": "profile_pending"}
        if route == "/v1/auth/login":
            email, password = body.get("email"), body.get("password")
            if not isinstance(email, str) or not isinstance(password, str) or len(password)>128 or len(email)>180:
                fail(401, "INVALID_CREDENTIALS", "Incorrect email or password.")
            with s.connection() as db:
                account = db.execute("SELECT * FROM accounts WHERE email=?", (email.strip().lower(),)).fetchone()
            if not account or not check_password(account["password"], password):
                fail(401, "INVALID_CREDENTIALS", "Incorrect email or password.")
            if not account["verified"]:
                fail(403, "EMAIL_UNVERIFIED", "Verify your email to continue.")
            token = secrets.token_urlsafe(32)
            expires = int(time.time())+(30*86400 if body.get("remember") is True else 8*3600)
            with s.connection(True) as db:
                if not db.execute("SELECT 1 FROM accounts WHERE id=? AND password=?",
                                  (account["id"], account["password"])).fetchone():
                    fail(401, "INVALID_CREDENTIALS", "Incorrect email or password.")
                if db.execute("SELECT 1 FROM restricted_accounts WHERE account=?", (account["id"],)).fetchone():
                    fail(403, "ACCOUNT_RESTRICTED", "This account is restricted in the Jam.")
                db.execute("INSERT INTO sessions VALUES (?,?,?)", (digest(token.encode()), account["id"], expires))
            return 200, {"accessToken": token, "expiresAt": expires, "state": account_state(account)}
        if route == "/v1/auth/logout":
            with s.connection(True) as db:
                s.authenticate(db, self.headers.get("Authorization", ""))
                db.execute("DELETE FROM sessions WHERE token=?",
                           (digest(self.headers["Authorization"].split(" ",1)[1].encode()),))
            return 204, None
        if route == "/v1/auth/password/forgot":
            email = bounded_text(body.get("email"), 180, "email").lower()
            if not EMAIL.fullmatch(email):
                fail(422, "INVALID_FIELD", "Enter a valid email.")
            now = int(time.time())
            with s.connection(True) as db:
                account = db.execute("SELECT id FROM accounts WHERE email=? AND verified=1", (email,)).fetchone()
                if account:
                    last = db.execute("SELECT created FROM password_resets WHERE account=?", (account["id"],)).fetchone()
                    if not last or last[0] <= now - 60:
                        token = secrets.token_urlsafe(32)
                        db.execute("DELETE FROM password_resets WHERE account=?", (account["id"],))
                        db.execute("INSERT INTO password_resets VALUES (?,?,?,?)",
                                   (digest(token.encode()), account["id"], now + 900, now))
                        s.write_mail(email, token, recovery=True)
            return 202, {"state": "reset_requested"}
        if route == "/v1/auth/password/reset":
            token, password = body.get("token"), body.get("password")
            if not isinstance(token, str) or not SECRET.fullmatch(token):
                fail(422, "INVALID_TOKEN", "The recovery code is invalid or expired.")
            if not isinstance(password, str) or not 12 <= len(password) <= 128:
                fail(422, "INVALID_FIELD", "Use a password of 12–128 characters.")
            key = digest(token.encode())
            with s.connection() as db:
                reset = db.execute("SELECT * FROM password_resets WHERE token=? AND expires>?",
                                   (key, int(time.time()))).fetchone()
            if reset is None:
                fail(422, "INVALID_TOKEN", "The recovery code is invalid or expired.")
            stored = password_hash(password)
            with s.connection(True) as db:
                consumed = db.execute("DELETE FROM password_resets WHERE token=? AND expires>?",
                                      (key, int(time.time())))
                if consumed.rowcount != 1:
                    fail(422, "INVALID_TOKEN", "The recovery code is invalid or expired.")
                db.execute("UPDATE accounts SET password=? WHERE id=?", (stored, reset["account"]))
                db.execute("DELETE FROM sessions WHERE account=?", (reset["account"],))
            return 200, {"state": "password_reset"}
        fail(404, "NOT_FOUND", "Endpoint not found.")

    def mutation(self, db, actor, path, body, operation):
        key = self.headers.get("Idempotency-Key", "")
        if not UUID.fullmatch(key):
            fail(400, "IDEMPOTENCY_REQUIRED", "An idempotency key is required.")
        request = digest((path + "\n" + json.dumps(body, sort_keys=True)).encode())
        previous = db.execute("SELECT * FROM idempotency WHERE account=? AND key=?", (actor, key)).fetchone()
        if previous:
            if previous["request"] != request:
                fail(409, "IDEMPOTENCY_MISMATCH", "This key belongs to a different operation.")
            return previous["status"], json.loads(previous["response"])
        status, value = operation()
        db.execute("INSERT INTO idempotency VALUES (?,?,?,?,?)",
                   (actor, key, request, status, json.dumps(value)))
        return status, value

    def route(self, db, route, method, body):
        s = self.service
        if method == "GET" and route == "/v1/jams/current":
            return 200, {"jam": {"jamId": JAM, "title": "Local Mapping Jam", "status": "open",
                                 "rulesVersion": RULES_VERSION, "packVersion": PACK, "serverTime": utc(),
                                 "canJoin": True, "canBuy": True, "canGift": True, "canPublish": True, "canPlay": True,
                                 "localApiRevision": 5}}
        user = s.authenticate(db, self.headers.get("Authorization", ""))
        account = user["id"]
        if route == "/v1/me":
            if method == "GET":
                return 200, {**profile(db, user), "email": user["email"], "state": account_state(user)}
        if route == "/v1/me/avatar" and method == "POST":
            if not user["verified"]:
                fail(403, "EMAIL_UNVERIFIED", "Verify your email to continue.")
            avatar = uuid.uuid4().hex
            db.execute("DELETE FROM avatars WHERE owner=? AND id NOT IN (SELECT avatar FROM profile_avatars)", (account,))
            db.execute("INSERT INTO avatars VALUES (?,?,?)", (avatar, account, body))
            return 201, {"avatarId": avatar}
        avatar_route = re.fullmatch(r"/v1/avatars/([a-f0-9]{32})", route)
        if avatar_route and method == "GET":
            row = db.execute(
                "SELECT a.png FROM avatars a WHERE a.id=? AND (a.owner=? OR "
                "EXISTS (SELECT 1 FROM profile_avatars p JOIN wallets w ON w.account=p.account "
                "WHERE p.avatar=a.id AND NOT EXISTS (SELECT 1 FROM restricted_accounts r WHERE r.account=p.account)))",
                (avatar_route[1], account)).fetchone()
            if row is None:
                fail(404, "NOT_FOUND", "Avatar not found.")
            return 200, row[0], "image/png"
        if route == "/v1/me/profile" and method == "PATCH":
            if not user["verified"]:
                fail(403, "EMAIL_UNVERIFIED", "Verify your email to continue.")
            nickname = bounded_text(body.get("nickname", user["nickname"]), 40, "nickname")
            bio = bounded_text(body.get("bio", user["bio"]), 400, "bio", optional=True)
            try:
                db.execute("UPDATE accounts SET nickname=?,bio=?,ready=? WHERE id=?",
                           (nickname, bio, int(user["ready"] or body.get("completeOnboarding") is True), account))
            except sqlite3.IntegrityError:
                fail(409, "NICKNAME_TAKEN", "Choose another nickname.")
            if "avatarId" in body:
                avatar = body["avatarId"]
                if avatar is None:
                    db.execute("DELETE FROM profile_avatars WHERE account=?", (account,))
                else:
                    if not isinstance(avatar, str) or not UUID.fullmatch(avatar) or not db.execute(
                        "SELECT 1 FROM avatars WHERE id=? AND owner=?", (avatar, account)).fetchone():
                        fail(422, "AVATAR_INVALID", "Choose an avatar uploaded by this account.")
                    db.execute("INSERT INTO profile_avatars VALUES (?,?) ON CONFLICT(account) DO UPDATE SET avatar=excluded.avatar",
                               (account, avatar))
                db.execute("DELETE FROM avatars WHERE owner=? AND id NOT IN (SELECT avatar FROM profile_avatars)", (account,))
            db.execute("UPDATE board_version SET version=version+1")
            row = db.execute("SELECT * FROM accounts WHERE id=?", (account,)).fetchone()
            return 200, {**profile(db, row), "state": account_state(row)}
        s.authorize(user)
        if method == "GET" and re.fullmatch(r"/v1/profiles/[a-f0-9]{32}", route):
            s.wallet(db, account)
            row = db.execute("SELECT * FROM accounts JOIN wallets ON wallets.account=accounts.id WHERE accounts.id=?",
                             (route.rsplit("/",1)[1],)).fetchone()
            if not row:
                fail(404, "NOT_FOUND", "Participant not found.")
            return 200, profile(db, row)
        prefix = f"/v1/jams/{JAM}/"
        if not route.startswith(prefix):
            fail(404, "NOT_FOUND", "Endpoint not found.")
        path = route[len(prefix):]
        if path == "rules" and method == "GET":
            return 200, RULES
        if path == "join" and method == "POST":
            cursor = db.execute("INSERT OR IGNORE INTO wallets VALUES (?,20,10,1)", (account,))
            if cursor.rowcount:
                db.execute("INSERT INTO ledger VALUES (?,?,?,?,?,?)",
                           (uuid.uuid4().hex, "join", account, 20, 10, int(time.time())))
            return (201 if cursor.rowcount else 200), {"wallet": s.wallet(db, account)}
        wallet = s.wallet(db, account)
        if path == "presence" and method == "POST":
            return 200, s.presence(db, self.headers.get("Authorization", ""))
        if path == "board" and method == "GET":
            board = s.board(db)
            etag = f'"{board["boardRevision"]}"'
            return (304, None, "application/json", {"ETag": etag}) if self.headers.get("If-None-Match")==etag else (200, board, "application/json", {"ETag": etag})
        if path == "me/wallet" and method == "GET":
            return 200, wallet
        if path == "reports" and method == "POST":
            return self.mutation(db, account, path, body, lambda: self.submit_report(db, account, body))
        if path == f"packs/{PACK}/manifest" and method == "GET":
            return 200, s.pack_manifest
        if path == f"packs/{PACK}/download" and method == "GET":
            return 200, s.pack_path, "application/zip"
        if path == "hub/manifest" and method == "GET":
            return 200, s.hub_manifest(db)
        hub_download = re.fullmatch(r"hub/([a-f0-9]{64})/download", path)
        if hub_download and method == "GET":
            artifact = s.hub_cache / (hub_download[1] + ".zip")
            if not artifact.is_file():
                fail(404, "NOT_FOUND", "This Hub build is unavailable.")
            return 200, artifact, "application/zip"
        if path == "gifts" and method == "POST":
            return self.mutation(db, account, path, body,
                                 lambda: s.gift(db, account, body.get("recipientAccountId"), body.get("amountRb")))
        plot_match = re.fullmatch(r"plots/([A-E][1-5])/(purchase|uploads|play-manifest|likes)", path)
        if plot_match:
            plot_id, action = plot_match.groups()
            plot = s.plot(db, plot_id)
            if action == "likes" and method == "POST":
                return self.mutation(db, account, path, body, lambda: s.gift(db, account, plot["owner"], 1, plot_id))
            if action == "purchase" and method == "POST":
                def buy():
                    if body.get("expectedRulesVersion") != RULES_VERSION or body.get("expectedPriceRb") != 20:
                        fail(409, "RULES_CHANGED", "Review the current price and rules.")
                    if plot["owner"]:
                        fail(409, "PLOT_ALREADY_OWNED", "The plot has been purchased.")
                    if wallet["spendableRb"] < 20:
                        fail(409, "INSUFFICIENT_RB", "Not enough ordinary RB.")
                    db.execute("UPDATE plots SET owner=? WHERE id=?", (account,plot_id))
                    db.execute("UPDATE wallets SET ordinary=ordinary-20,version=version+1 WHERE account=?", (account,))
                    db.execute("UPDATE board_version SET version=version+1")
                    db.execute("INSERT INTO ledger VALUES (?,?,?,?,?,?)",
                               (uuid.uuid4().hex, "purchase", account, -20, 0, int(time.time())))
                    self.after.append(s.queue_hub_refresh)
                    return 200, {"plot": {"plotId": plot_id, "owner": profile(db, user)}, "wallet": s.wallet(db, account)}
                return self.mutation(db, account, path, body, buy)
            if action == "uploads" and method == "POST":
                def upload():
                    if plot["owner"] != account:
                        fail(403, "PLOT_NOT_OWNED", "Only the owner can publish.")
                    if plot["revision"] != body.get("baseRevisionId"):
                        fail(409, "REVISION_CONFLICT", "The publication changed. Review the new revision.")
                    if any(body.get(k) != v for k,v in (("rulesVersion",RULES_VERSION),("packVersion",PACK),("termsVersion",TERMS))):
                        fail(409, "RULES_CHANGED", "Review the current rules and publication terms.")
                    title = bounded_text(body.get("title"), 80, "title")
                    checksum, size = body.get("sourceSha256"), body.get("sourceSizeBytes")
                    if not isinstance(checksum,str) or not re.fullmatch(r"[a-f0-9]{64}",checksum) or type(size) is not int or not 0<size<=MAX_SOURCE:
                        fail(422, "ARTIFACT_INVALID", "The source declaration is invalid.")
                    if db.execute("SELECT 1 FROM uploads WHERE plot=? AND (status IN ('queued','compiling') OR (status='upload_created' AND expires>?))",
                                  (plot_id,int(time.time()))).fetchone():
                        fail(409, "PUBLISH_IN_PROGRESS", "Finish the existing publication first.")
                    identifier, expires = uuid.uuid4().hex, int(time.time())+900
                    db.execute("INSERT INTO uploads(id,account,plot,title,base,checksum,size,expires,status) VALUES (?,?,?,?,?,?,?,?,?)",
                               (identifier,account,plot_id,title,plot["revision"],checksum,size,expires,"upload_created"))
                    return 201, {"uploadId":identifier,"expiresAt":expires,"uploadPath":prefix+"uploads/"+identifier+"/content"}
                return self.mutation(db, account, path, body, upload)
            if action == "play-manifest" and method == "GET":
                revision = self.playable(db, plot["revision"])
                return 200, s.play_manifest(revision)
        upload_match = re.fullmatch(r"uploads/([a-f0-9]{32})/(content|finalize)", path)
        if upload_match:
            identifier, action = upload_match.groups()
            row = self.owned_upload(db, account, identifier)
            source_path = s.incoming/(identifier+".bin")
            if action == "content" and method == "PUT":
                if row["expires"]<=int(time.time()) or row["status"]!="upload_created":
                    fail(409, "UPLOAD_EXPIRED", "This upload is closed or expired.")
                if len(body)!=row["size"] or digest(body)!=row["checksum"]:
                    fail(422, "ARTIFACT_INVALID", "The uploaded bytes do not match the declaration.")
                temp = source_path.with_suffix(".part")
                temp.write_bytes(body)
                temp.replace(source_path)
                return 204, None
            if action == "finalize" and method == "POST":
                def finalize():
                    if row["status"] in {"queued","compiling","published","failed"}:
                        return 202, {"operationId":identifier}
                    if row["expires"]<=int(time.time()) or row["status"]!="upload_created":
                        fail(409, "UPLOAD_EXPIRED", "This upload is closed or expired.")
                    if not source_path.is_file() or digest(source_path.read_bytes())!=row["checksum"]:
                        fail(422, "ARTIFACT_INVALID", "Upload the source before publishing.")
                    db.execute("UPDATE uploads SET status='queued' WHERE id=?", (identifier,))
                    self.after.append(lambda: s.executor.submit(s.compile, identifier))
                    return 202, {"operationId":identifier}
                return self.mutation(db, account, path, body, finalize)
        operation = re.fullmatch(r"operations/([a-f0-9]{32})(/cancel)?", path)
        if operation:
            identifier, cancel = operation.groups()
            row = self.owned_upload(db, account, identifier)
            if method == "GET" and not cancel:
                value = {"operationId":identifier,"status":row["status"]}
                if row["status"]=="published":
                    value["revisionId"] = identifier
                if row["error"]:
                    value["error"] = json.loads(row["error"])
                return 200, value
            if method == "POST" and cancel:
                def stop():
                    if row["status"]=="published":
                        fail(409, "REVISION_CONFLICT", "The publication already finished.")
                    if row["status"] in {"upload_created","queued","compiling"}:
                        db.execute("UPDATE uploads SET status='canceled' WHERE id=?", (identifier,))
                        return 200, {"status":"canceled"}
                    return 200, {"status":row["status"]}
                return self.mutation(db, account, path, body, stop)
        rev_match = re.fullmatch(r"revisions/([a-f0-9]{32})/download", path)
        if rev_match and method=="GET":
            rev = self.playable(db, rev_match[1])
            artifact = s.work / (rev["id"] + ".zip")
            if not artifact.is_file():
                fail(503, "ARTIFACT_UNAVAILABLE", "The artifact is unavailable.")
            return 200, artifact, "application/zip"
        fail(404, "NOT_FOUND", "Endpoint not found.")

    def owned_upload(self, db, account, identifier):
        row = db.execute("SELECT * FROM uploads WHERE id=? AND account=?", (identifier,account)).fetchone()
        if not row:
            fail(404, "NOT_FOUND", "Operation not found.")
        return row

    def submit_report(self, db, account, body):
        kind, target, reason = (body.get(key) for key in ("targetKind", "targetId", "reasonCode"))
        if (not isinstance(target, str) or reason not in REPORT_REASONS
                or (kind in ("profile", "revision") and not UUID.fullmatch(target))):
            fail(422, "INVALID_FIELD", "Choose a report target and reason.")
        if kind == "plot":
            owner = self.service.plot(db, target)["owner"]
        elif kind == "revision":
            row = self.playable(db, target)
            owner = row["account"]
        elif kind == "profile":
            self.service.wallet(db, target)
            owner = target
        else:
            fail(422, "INVALID_FIELD", "Choose a report target and reason.")
        if not owner or owner == account:
            fail(422, "INVALID_FIELD", "Choose another participant's content.")
        now = int(time.time())
        if db.execute("SELECT 1 FROM reports WHERE reporter=? AND kind=? AND target=? AND status='open'",
                      (account, kind, target)).fetchone():
            fail(409, "REPORT_ALREADY_OPEN", "A report for this target is already open.")
        if db.execute("SELECT count(*) FROM reports WHERE reporter=? AND created>?",
                      (account, now - 3600)).fetchone()[0] >= 8:
            fail(429, "RATE_LIMITED", "Wait before submitting another report.")
        note = bounded_text(body.get("note", ""), 600, "report note", optional=True)
        identifier = uuid.uuid4().hex
        db.execute("INSERT INTO reports(id,reporter,kind,target,reason,note,created) VALUES (?,?,?,?,?,?,?)",
                   (identifier, account, kind, target, reason, note, now))
        return 202, {"reportId": identifier}

    def moderator_route(self, db, route, method, body):
        secret = self.service.moderator_secret
        supplied = self.headers.get("Authorization", "").encode("utf-8")
        if not secret or not hmac.compare_digest(supplied, ("Bearer " + secret).encode("ascii")):
            fail(403, "MODERATOR_REQUIRED", "Moderator access is required.")
        if route == "/v1/admin/reports" and method == "GET":
            rows = db.execute("SELECT id,kind,target,reason,note,created,status,resolution FROM reports "
                              "ORDER BY status='open' DESC, created DESC LIMIT 100")
            return 200, {"reports": [dict(row) for row in rows]}
        if route == "/v1/admin/audit" and method == "GET":
            return 200, {"events": [dict(row) for row in db.execute(
                "SELECT * FROM moderation_log ORDER BY created DESC, rowid DESC LIMIT 100")]}
        match = re.fullmatch(r"/v1/admin/(reports|revisions|accounts)/([a-f0-9]{32})", route)
        if method != "PATCH" or match is None:
            fail(404, "NOT_FOUND", "Endpoint not found.")
        kind, target = match.groups()
        reason = bounded_text(body.get("reason"), 600, "moderation reason")
        action = body.get("action")
        if kind == "reports":
            if action not in ("resolve", "dismiss"):
                fail(422, "INVALID_FIELD", "Resolve or dismiss this report.")
            result = db.execute("UPDATE reports SET status=?,resolution=? WHERE id=? AND status='open'",
                                ("resolved" if action == "resolve" else "dismissed", reason, target))
            if not result.rowcount:
                fail(409, "REPORT_CLOSED", "The report is unavailable or already closed.")
        elif kind == "revisions":
            if action not in ("hide", "restore"):
                fail(422, "INVALID_FIELD", "Hide or restore this revision.")
            if not db.execute("SELECT 1 FROM uploads WHERE id=? AND status='published'", (target,)).fetchone():
                fail(404, "NOT_FOUND", "Publication not found.")
            if action == "hide":
                db.execute("INSERT OR IGNORE INTO hidden_revisions VALUES (?)", (target,))
            else:
                db.execute("DELETE FROM hidden_revisions WHERE revision=?", (target,))
        elif kind == "accounts":
            if action not in ("restrict", "restore"):
                fail(422, "INVALID_FIELD", "Restrict or restore this account.")
            if not db.execute("SELECT 1 FROM accounts WHERE id=?", (target,)).fetchone():
                fail(404, "NOT_FOUND", "Account not found.")
            if action == "restrict":
                db.execute("INSERT OR IGNORE INTO restricted_accounts VALUES (?)", (target,))
                db.execute("DELETE FROM sessions WHERE account=?", (target,))
            else:
                db.execute("DELETE FROM restricted_accounts WHERE account=?", (target,))
        if kind != "reports":
            db.execute("UPDATE board_version SET version=version+1")
            self.after.append(self.service.queue_hub_refresh)
        db.execute("INSERT INTO moderation_log VALUES (?,?,?,?,?,?)",
                   (uuid.uuid4().hex, "local-moderator", kind + "/" + action, target, reason, int(time.time())))
        return 200, {"targetId": target, "action": action}

    def playable(self, db, revision):
        row = db.execute("SELECT u.* FROM uploads u JOIN plots p ON p.revision=u.id WHERE u.id=? AND u.status='published' "
                         "AND NOT EXISTS (SELECT 1 FROM hidden_revisions h WHERE h.revision=u.id) "
                         "AND NOT EXISTS (SELECT 1 FROM restricted_accounts r WHERE r.account=u.account)",
                         (revision,)).fetchone()
        if not row:
            fail(404, "NO_PLAYABLE_REVISION", "No playable publication.")
        return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--tools", type=Path, required=True)
    parser.add_argument("--librequake", type=Path, required=True)
    parser.add_argument("--plant-preset", type=Path, required=True)
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--moderator-key-file", type=Path)
    options = parser.parse_args()
    service = Service(options.data.resolve(), options.tools.resolve(), options.librequake.resolve(),
                      options.plant_preset.resolve(), options.moderator_key_file)
    with ThreadingHTTPServer(("127.0.0.1", options.port), Handler) as http:
        http.service = service
        print(f"Mapping Jams local API: http://127.0.0.1:{http.server_port}/v1", flush=True)
        try:
            http.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            service.executor.shutdown(wait=True)
            service.hub_executor.shutdown(wait=True)


if __name__ == "__main__":
    main()
