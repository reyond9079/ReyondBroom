#!/usr/bin/env python3
"""Prepare a tiny actual CMake dependency to exercise cold and warm CI caches."""
from pathlib import Path
import argparse
import shutil

parser = argparse.ArgumentParser(); parser.add_argument('--clear-output', action='store_true'); args = parser.parse_args()
if args.clear_output:
    shutil.rmtree('.ci-deps/probe-build')
root = Path('.ci-deps/probe-source'); root.mkdir(parents=True, exist_ok=True)
(root/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.20)
project(CacheContract LANGUAGES CXX)
foreach(name BulletDynamics BulletCollision LinearMath)
 add_library(${name} STATIC unit.cpp)
endforeach()
''')
(root/'unit.cpp').write_text('int cache_contract() { return 42; }\n')
