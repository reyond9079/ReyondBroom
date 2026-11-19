#!/usr/bin/env python3
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

source = Path(__file__).resolve().parents[1] / "scripts/check-texture-studio-release.py"
module_spec = importlib.util.spec_from_file_location("texture_gate", source)
gate = importlib.util.module_from_spec(module_spec)
module_spec.loader.exec_module(gate)
SOURCE = "a" * 40
BUILD = "b" * 64
OWNER = "release-owner"


class GateTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name).resolve()
        evidence = b"synthetic verifier fixture"
        (self.root / "synthetic.log").write_bytes(evidence)
        self.gatefile = self.root / "gate.json"
        self.report = dict(schemaVersion=1, specVersion="1.0", sourceCommit=SOURCE,
                           sourceTreeClean=True, buildSha256=BUILD, status="PASS",
                           openDefects=[], publicationAuthorized=True,
                           humanAcceptance=dict(status="accepted", acceptedBy=OWNER,
                                                sourceCommit=SOURCE, buildSha256=BUILD,
                                                acceptedAt="2026-11-19T12:00:00Z"),
                           requiredChecks=[dict(id=key, status="PASS", exitCode=0,
                                                evidence={"synthetic.log": hashlib.sha256(evidence).hexdigest()})
                                           for key in sorted(gate.CHECKS)])

    def verify(self, report, authorized=True):
        self.gatefile.write_text(json.dumps(report), encoding="utf-8")
        return gate.validate(self.root, SOURCE, BUILD, publication=True, authorized=authorized, owner=OWNER)

    def test_only_complete_authorized_evidence_passes(self):
        self.assertEqual(self.verify(self.report), len(gate.CHECKS))

    def test_each_candidate_archive_is_bound_to_the_build(self):
        version = "0.6.0-alpha.32"
        parts = []
        for platform in gate.PLATFORMS:
            directory = self.root / f"release-packages-{platform}"
            directory.mkdir()
            extension = "zip" if platform.startswith("Windows-") else "tar.gz"
            for suffix in (f".{extension}", "-Sources.zip"):
                name = f"ReyondBroom-{version}-{platform}{suffix}"
                archive = directory / name
                archive.write_bytes(name.encode("ascii"))
                digest = hashlib.sha256(archive.read_bytes()).hexdigest()
                sidecar = archive.with_name(name + ".sha256")
                sidecar.write_text(f"{digest}  {name}\n", encoding="ascii")
                parts.append(f"{digest}  {name}\n")
        expected = hashlib.sha256((version + "\n" + "".join(parts)).encode("ascii")).hexdigest()
        self.assertEqual(gate.candidate_digest(self.root, version), expected)
        archive.write_bytes(b"different build")
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            gate.candidate_digest(self.root, version)
        new_hash = hashlib.sha256(archive.read_bytes()).hexdigest()
        sidecar.write_text(f"{new_hash}  {name}\n", encoding="ascii")
        self.assertNotEqual(gate.candidate_digest(self.root, version), expected)
        archive.unlink()
        with self.assertRaisesRegex(ValueError, "inventory"):
            gate.candidate_digest(self.root, version)

    def test_denials(self):
        variants = {
            "missing": lambda r: r["requiredChecks"].pop(),
            "skipped": lambda r: r["requiredChecks"][0].update(status="SKIPPED"),
            "failed": lambda r: r["requiredChecks"][0].update(status="FAIL", exitCode=1),
            "duplicate": lambda r: r["requiredChecks"].append(r["requiredChecks"][0]),
            "wrong commit": lambda r: r.update(sourceCommit="0" * 40),
            "dirty": lambda r: r.update(sourceTreeClean=False),
            "wrong build": lambda r: r.update(buildSha256="1" * 64),
            "open defects": lambda r: r.update(openDefects=["unverified profile"]),
            "no publication authorization": lambda r: r.update(publicationAuthorized=False),
            "no human acceptance": lambda r: r.pop("humanAcceptance"),
            "rejected by owner": lambda r: r["humanAcceptance"].update(status="rejected"),
            "wrong reviewer": lambda r: r["humanAcceptance"].update(acceptedBy="other"),
            "accepted old build": lambda r: r["humanAcceptance"].update(buildSha256="1" * 64),
            "accepted old commit": lambda r: r["humanAcceptance"].update(sourceCommit="1" * 40),
            "no acceptance timestamp": lambda r: r["humanAcceptance"].pop("acceptedAt"),
            "time without timezone": lambda r: r["humanAcceptance"].update(acceptedAt="2026-11-19T12:00:00"),
            "unsafe path": lambda r: r["requiredChecks"][0].update(evidence={"../other.log": "0" * 64}),
            "missing file": lambda r: r["requiredChecks"][0].update(evidence={"missing.log": "0" * 64}),
            "wrong file hash": lambda r: r["requiredChecks"][0].update(evidence={"synthetic.log": "0" * 64}),
        }
        for name, edit in variants.items():
            with self.subTest(name=name):
                report = copy.deepcopy(self.report)
                edit(report)
                with self.assertRaises(ValueError):
                    self.verify(report)
        with self.assertRaises(ValueError):
            self.verify(self.report, authorized=False)
        self.gatefile.unlink()
        with self.assertRaises(ValueError):
            gate.validate(self.root, SOURCE, BUILD)


if __name__ == "__main__":
    unittest.main()
