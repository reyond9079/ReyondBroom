"""Render the licensed Lucide source as an antialiased viewport marker.

Development-only dependencies: cairosvg, Pillow. No runtime dependency is added.
"""
from pathlib import Path
import io
import cairosvg
from PIL import Image

root = Path(__file__).resolve().parents[1]
svg = (root / "assets/lucide/lightbulb.svg").read_text(encoding="utf-8")
svg = svg.replace('viewBox="0 0 24 24"', 'viewBox="-2 -2 28 28"')

def layer(color, width):
    source = svg.replace('stroke="currentColor"', f'stroke="{color}"')
    source = source.replace('stroke-width="2"', f'stroke-width="{width}"')
    return Image.open(io.BytesIO(cairosvg.svg2png(
        bytestring=source.encode(), output_width=512, output_height=512))).convert("RGBA")

image = Image.alpha_composite(layer("#191d24", 3.6), layer("#f7df97", 1.9))
image.resize((128, 128), Image.Resampling.LANCZOS).save(root / "runtime/icons/light-bulb.tga")
