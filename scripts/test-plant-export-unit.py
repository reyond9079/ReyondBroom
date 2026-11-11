#!/usr/bin/env python3
"""Roundtrip the plant export manager's encoders without a generator corpus."""
import argparse
import os
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[1]
binary=a.output/('plant-export-manager.exe' if os.name=='nt' else 'plant-export-manager')
subprocess.run(['g++','-std=c++17','-O1',str(repo/'tests/plant-export-manager.cpp'),'-lz','-o',str(binary.resolve())],check=True)
subprocess.run([str(binary.resolve())],check=True)
