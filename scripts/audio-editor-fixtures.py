#!/usr/bin/env python3
"""Deterministic synthetic audio for native Sound Editor integration checks."""
import array
import math
from pathlib import Path
import random
import subprocess
import sys
import wave

root = Path(sys.argv[1]).resolve()
converter = sys.argv[2] if len(sys.argv) > 2 else 'ffmpeg'
root.mkdir(parents=True, exist_ok=True)
rng = random.Random(42)
for name, duration, channels in [('voice', 6, 1), ('music', 18, 2), ('impact', 1.2, 2)]:
    pcm = array.array('h')
    for i in range(int(44100 * duration)):
        t = i / 44100
        if name == 'voice':
            envelope = .3 * (max(0, math.sin(t * 7)) ** .7) * min(1, t * 8)
            sample = envelope * (math.sin(t * math.tau * 180) + .25 * math.sin(t * math.tau * 360))
        elif name == 'music':
            envelope = .2 + .12 * math.sin(t * 1.2)
            sample = envelope * sum(math.sin(t * math.tau * f) for f in [110, 164.81, 220]) / 3
        else:
            sample = .6 * math.exp(-t * 5) * (rng.uniform(-1, 1) + math.sin(t * math.tau * (80 - 25 * t))) / 2
        for channel in range(channels):
            pcm.append(int(max(-1, min(1, sample * (1 if channel == 0 else .8))) * 32767))
    wav = root / f'{name}.wav'
    with wave.open(str(wav), 'wb') as out:
        out.setparams((channels, 2, 44100, 0, 'NONE', 'not compressed'))
        out.writeframes(pcm.tobytes())
    if name != 'voice':
        ext = '.ogg' if name == 'music' else '.flac'
        subprocess.run([converter, '-nostdin', '-hide_banner', '-loglevel', 'error', '-y', '-i', str(wav), str(root / (name + ext))], check=True)
print(f'Audio fixtures: {root}')
