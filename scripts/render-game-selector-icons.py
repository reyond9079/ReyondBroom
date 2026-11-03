"""Rasterize the original selector SVG symbols without runtime SVG dependencies."""
from pathlib import Path
import re,xml.etree.ElementTree as ET
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parents[1]/'runtime/ui/games';scale=4
for name in ('quake2','halflife'):
    doc=ET.fromstring((root/(name+'.svg')).read_text())
    image=Image.new('RGBA',(256*scale,256*scale));draw=ImageDraw.Draw(image)
    for node in doc:
        tag=node.tag.rsplit('}',1)[-1];a=node.attrib
        fill=a.get('fill');fill=None if fill=='none' else fill
        if tag in ('circle','ellipse'):
            cx=float(a['cx']);cy=float(a['cy']);rx=float(a.get('rx',a.get('r',0)));ry=float(a.get('ry',a.get('r',0)))
            stroke=float(a.get('stroke-width',0));half=stroke*.5
            bounds=tuple(round(v*scale) for v in (cx-rx-half,cy-ry-half,cx+rx+half,cy+ry+half))
            draw.ellipse(bounds,fill=fill,outline=a.get('stroke'),width=round(stroke*scale))
        elif tag=='path':
            for part in a['d'].split('M')[1:]:
                values=[float(v) for v in re.findall(r'-?\d+(?:\.\d+)?',part)]
                draw.polygon([(round(values[i]*scale),round(values[i+1]*scale)) for i in range(0,len(values),2)],fill=fill)
    image.resize((256,256),Image.Resampling.LANCZOS).save(root/(name+'.png'))
    print(name+'.png')
