#!/usr/bin/env python3
"""Overlay current release assets onto the disposable source-CI SDK runtime."""
import argparse
from pathlib import Path
import shutil

p = argparse.ArgumentParser(description=__doc__); p.add_argument('runtime', type=Path); a = p.parse_args()
repo = Path(__file__).resolve().parents[1]; data = a.runtime/'ftebroom'
assert (data/'csaddon.dat').is_file(), 'Stage only a built disposable source runtime'
for name in ('ftebroom.cfg', 'quake1.ent', 'update-ftebroom.py', 'update-ftebroom.ps1',
             'fonts/FTEBroom-CJK.otf', 'scripts/ftebroom.shader'):
    target = data/name; target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(repo/'runtime'/name, target)
for name in ('glsl', 'plants'):
    shutil.copytree(repo/'runtime'/name, data/name, dirs_exist_ok=True)
