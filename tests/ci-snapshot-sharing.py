#!/usr/bin/env python3
"""Exercise actual OS file replacement while a QA snapshot reader is open."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from editor_os_input import _open_ui_snapshot,read_ui_json


class SnapshotSharing(unittest.TestCase):
    def test_open_reader_does_not_prevent_atomic_producer_replacement(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'snapshot.json';path.write_text('{"version":1}',encoding='utf-8')
            replacement=path.with_suffix('.tmp');replacement.write_text('{"version":2}',encoding='utf-8')
            with _open_ui_snapshot(path) as reader:
                replacement.replace(path)
                self.assertEqual(json.load(reader),{'version':1})
            self.assertEqual(read_ui_json(path),{'version':2})

    def test_missing_or_incomplete_snapshot_does_not_report_success(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'snapshot.json'
            with self.assertRaises(FileNotFoundError):read_ui_json(path,timeout=.025)
            path.write_text('{',encoding='utf-8')
            with self.assertRaises(json.JSONDecodeError):read_ui_json(path,timeout=.025)


if __name__=='__main__':unittest.main()
