#!/usr/bin/env python3
"""Use the same short native startup check for a freshly built source runtime."""
import argparse
import importlib.util
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('runtime', type=Path); p.add_argument('output', type=Path)
a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('release_smoke', Path(__file__).with_name('ci-release-smoke.py'))
smoke = importlib.util.module_from_spec(spec); spec.loader.exec_module(smoke)
smoke.launch(a.runtime.resolve(), a.output.resolve())
print('PASS native startup')
