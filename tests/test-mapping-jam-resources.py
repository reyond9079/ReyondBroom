#!/usr/bin/env python3
"""Exercise full-size Jam packs, corrupt ZIPs and compressed map installation."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import http.server
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import threading
import zipfile


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream,"sha256").hexdigest()


def fixture(root, source):
    if source is None:
        source = root/"stock"
        (source/"lq1").mkdir(parents=True)
        (source/"plants/builtin").mkdir(parents=True)
        for name,length in (("pak0.pak",55*1024*1024),("pak1.pak",15*1024*1024)):
            with (source/"lq1"/name).open("wb") as file:
                file.truncate(length)
        (source/"plants/builtin/catalog.json").write_text("{}",encoding="ascii")
    files = sorted(p for p in source.rglob("*") if p.is_file() and not p.name.startswith("."))
    pack = root/"pack.zip"
    with zipfile.ZipFile(pack,"w",zipfile.ZIP_STORED) as archive:
        for path in files:
            archive.write(path,path.relative_to(source).as_posix())
    assert 70*1024*1024 <= pack.stat().st_size <= 128*1024*1024
    manifest = {"schemaVersion":1,"sizeBytes":pack.stat().st_size,"sha256":digest(pack),"downloadPath":"/v1/pack",
                "packVersion":"librequake-0.09-plants-local1",
                "files":[{"path":p.relative_to(source).as_posix(),"sizeBytes":p.stat().st_size,"sha256":digest(p)}
                         for p in files]}
    return source,pack,manifest


def check(exe, root, source, peak_limit):
    source,pack,manifest = fixture(root,source)
    changed = root/"changed.zip"
    with zipfile.ZipFile(changed,"w",zipfile.ZIP_STORED) as archive:
        archive.writestr("lq1/pak0.pak","corrupt payload")
        archive.writestr("plants/builtin/catalog.json","{}")
    (root/"bad.json").write_text(json.dumps({
        "schemaVersion":1,"sizeBytes":changed.stat().st_size,"sha256":digest(changed),
        "downloadPath":"/v1/bad","files":[
            {"path":"lq1/pak0.pak","sizeBytes":15,"sha256":"0"*64},
            {"path":"plants/builtin/catalog.json","sizeBytes":2,"sha256":hashlib.sha256(b"{}").hexdigest()}]}))
    payload = b"BSP2"+bytes(32*1024)
    compressed = root/"map.zip"
    with zipfile.ZipFile(compressed,"w",zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("maps/jam_map.bsp",payload)
    (root/"map.json").write_text(json.dumps({
        "schemaVersion":1,"sizeBytes":compressed.stat().st_size,"sha256":digest(compressed),
        "downloadPath":"/v1/map","files":[
            {"path":"maps/jam_map.bsp","sizeBytes":len(payload),"sha256":hashlib.sha256(payload).hexdigest()}]}))
    (root/"pack.json").write_text(json.dumps(manifest))
    downloads = []
    pack_barrier = None

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            path = {"/v1/pack":pack,"/v1/bad":changed,"/v1/map":compressed}.get(self.path)
            if path is None:
                self.send_error(404)
                return
            downloads.append(self.path)
            if self.path == "/v1/pack" and pack_barrier is not None:
                pack_barrier.wait(timeout=15)
            self.send_response(200)
            self.send_header("Content-Length", str(path.stat().st_size))
            self.end_headers()
            with path.open("rb") as stream:
                shutil.copyfileobj(stream,self.wfile,256*1024)

        def log_message(self,*args):
            pass

    with http.server.ThreadingHTTPServer(("127.0.0.1",0),Handler) as server:
        thread = threading.Thread(target=server.serve_forever,daemon=True)
        thread.start()

        def run(kind="pack", destination="installed", mode="go", expected=0):
            result = subprocess.run([str(exe),str(server.server_port),str(root/(kind+".json")),
                                     str(root/destination),"map" if kind=="map" else "base",mode],
                                    text=True,capture_output=True,timeout=45)
            assert result.returncode==expected, (kind,result.returncode,result.stdout,result.stderr)
            return result

        try:
            first = run()
            peak = re.search(r"peakKiB=(\d+)",first.stdout)
            if peak_limit is not None:
                assert peak and int(peak[1])<=peak_limit*1024, first.stdout
            print(f"PASS verified and installed {pack.stat().st_size/2**20:.1f} MiB {first.stdout.strip()}")
            target = root/"installed"
            assert (target/".verified-pack.json").is_file()
            for item in manifest["files"]:
                assert digest(target/item["path"])==item["sha256"],item["path"]
            run()
            assert downloads==["/v1/pack"],downloads
            print("PASS reuses the verified installation without fetching the ZIP again")
            assert "pack=installed" in run(mode="versioned-probe").stdout
            assert downloads==["/v1/pack"],downloads
            print("PASS recognizes the verified legacy fixed directory without downloading")
            if os.name == "nt":
                cached = target/".verified-pack.json"
                legacy = json.loads(cached.read_text())
                del legacy["fileTimeFormat"]
                cached.write_text(json.dumps(legacy))
                run()
                assert downloads==["/v1/pack"] and json.loads(cached.read_text())["fileTimeFormat"]=="win32"
                print("PASS checks an old Windows receipt without fetching the ZIP")
            run("bad",expected=1)
            assert digest(target/"lq1/pak0.pak")==digest(source/"lq1/pak0.pak")
            assert not list(root.glob("installed.incoming-*"))
            print("PASS rejects a wrong member hash and preserves the installed pack")
            replacement = root/"replacement"
            replacement.mkdir()
            (replacement/"old-pack-marker").write_text("stale installation")
            run(destination="replacement",mode="replace")
            assert (replacement/".verified-pack.json").is_file()
            assert digest(replacement/"lq1/pak0.pak")==digest(source/"lq1/pak0.pak")
            assert not (replacement/"old-pack-marker").exists()
            print("PASS replacement finishes before old pack cleanup")
            run(destination="drafts",mode="draft-file")
            print("PASS the draft is readable and a second create preserves its contents")
            if os.name == "nt":
                long_target = "/".join(("nested directory with spaces " + "x"*52,)*3 + ("installed",))
                native_root = Path("\\\\?\\"+str(root))
                try:
                    run(destination=long_target)
                    assert (native_root/long_target/".verified-pack.json").is_file()
                    run(destination=long_target,mode="inventory-failure")
                    versioned_target = str(Path(long_target).parent/"packs"/manifest["packVersion"])
                    installed = run(destination=versioned_target,mode="versioned").stdout
                    assert installed.startswith("pack=")
                    assert run(destination=versioned_target,mode="versioned-probe").stdout==installed
                    run(destination=long_target+"/drafts",mode="draft-file")
                finally:
                    subtree = native_root/long_target.split("/")[0]
                    if subtree.is_dir():
                        shutil.rmtree(subtree)
                print("PASS installs and reuses resources and finds a Unicode map on a long Windows path")
                blocked = root/"blocked"
                blocked.mkdir()
                (blocked/"old-pack-marker").write_text("keep until a verified replacement is active")
                run(destination="blocked",mode="blocked-activation")
                run(destination="blocked",mode="replace")
                print("PASS returns a Windows activation error with deferred cleanup and can retry")
            legacy = root/"versioned"/manifest["packVersion"]
            legacy.mkdir(parents=True)
            (legacy/"keep").write_text("old cache")
            prefix = f'{manifest["packVersion"]}-{manifest["sha256"][:16]}-'
            unfinished = legacy.parent/(prefix+"0"*16)
            unfinished.mkdir()
            (unfinished/".verified-partial.tmp").write_text("partial receipt")
            relative = str(legacy.relative_to(root))
            assert "cache-miss" in run(destination=relative,mode="versioned-probe").stdout
            pack_barrier = threading.Barrier(2)
            try:
                with ThreadPoolExecutor(2) as workers:
                    results = [workers.submit(run,destination=relative,mode="versioned") for _ in range(2)]
                    names = {future.result().stdout.strip().removeprefix("pack=") for future in results}
            finally:
                pack_barrier = None
            assert len(names)==2 and all(name.startswith(prefix) for name in names), names
            for name in names:
                assert (legacy.parent/name/".verified-pack.json").is_file(),name
            assert (legacy/"keep").read_text()=="old cache" and not (unfinished/".verified-pack.json").exists()
            assert run(destination=relative,mode="versioned-probe").stdout.strip().removeprefix("pack=") in names
            print("PASS both concurrent installations published independent packs and the next client reused a complete one")
            assert "time limit" in run(mode="watchdog",expected=1).stderr
            print("PASS catches stalled filesystem work and renews the deadline on writes")
            assert "Canceled" in run(mode="cancel",expected=3).stderr
            run("map","published")
            assert (root/"published/maps/jam_map.bsp").read_bytes()==payload
            print("PASS cancellation and DEFLATE map installation")
        finally:
            server.shutdown()
            thread.join()


if __name__=="__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe",type=Path)
    parser.add_argument("--pack-source",type=Path,help="Optional existing Jam resources used read-only")
    parser.add_argument("--max-peak-mib",type=int,help="Optional Linux resident-memory regression ceiling")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="jam-pack-qa-") as work:
        check(args.exe.resolve(),Path(work),args.pack_source,args.max_peak_mib)
