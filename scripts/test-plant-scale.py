#!/usr/bin/env python3
"""Check profile-relative plant dimensions and real generated/runtime bounds."""
import argparse
import os
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[1]
binary=a.output/('plant-scale.exe' if os.name=='nt' else 'plant-scale')
subprocess.run(['g++','-std=c++17','-O1',str(repo/'tests/plant-scale.cpp'),'-lz','-o',str(binary.resolve())],check=True)
subprocess.run([str(binary.resolve())],check=True)
