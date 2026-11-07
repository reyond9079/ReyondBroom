#!/usr/bin/env python3
"""Assemble labelled crops of original native screenshots; never render fake plants."""
import argparse,hashlib,json,math
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
p=argparse.ArgumentParser(description=__doc__);p.add_argument('captures',type=Path);p.add_argument('output',type=Path);p.add_argument('--controls-only',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
catalog=json.loads((Path(__file__).resolve().parents[1]/'runtime/plants/builtin/catalog.json').read_text())['presets'];manifest=[]
controls={'oak','birch','willow','pine','spruce','wild-shrub','meadow-grass','bracken','daisy','amanita','saguaro','date-palm'}
if a.controls_only:catalog=[d for d in catalog if d['id'] in controls]
font=ImageFont.truetype('DejaVuSans.ttf',16)
def sheet(rows,path,title):
 state=json.loads((a.captures/(rows[0]['id']+'.json')).read_text());project=state['project'];caption=f" | generator {project['generatorVersion']}, seed {project['seed']}"
 cols=min(6,len(rows));tw,th=256,278;im=Image.new('RGB',(cols*tw,math.ceil(len(rows)/cols)*th+42),(38,42,46));draw=ImageDraw.Draw(im);draw.text((12,11),title+caption,font=font,fill='white')
 for i,d in enumerate(rows):
  name=d['id'];original=a.captures/(name+'.png');state=json.loads((a.captures/(name+'.json')).read_text());rect=state['widgets']['viewport'];source=Image.open(original).convert('RGB');x,y,w,h=[rect[k] for k in ['x','y','width','height']];crop=source.crop((int(x),int(y),int(x+w),int(y+h)));crop.thumbnail((tw-12,th-62));px=i%cols*tw;py=i//cols*th+42;im.paste(crop,(px+(tw-crop.width)//2,py+(th-62-crop.height)//2));draw.text((px+7,py+th-55),d['name'],font=font,fill='white');draw.text((px+7,py+th-31),name,font=font,fill=(165,178,182))
 im.save(path)
for d in catalog:
 original=a.captures/(d['id']+'.png');assert original.is_file(),str(original);manifest.append({'id':d['id'],'file':original.name,'sha256':hashlib.sha256(original.read_bytes()).hexdigest()})
for category in sorted({d['category'] for d in catalog}):sheet([d for d in catalog if d['category']==category],a.output/(category+'.png'),'Plant Editor: '+category+' — original app captures')
sheet([d for d in catalog if d['id'] in controls],a.output/'controls.png','REYON-9 — 12 control plants in the native Plant Editor')
(a.output/'originals-sha256.json').write_text(json.dumps({'count':len(manifest),'originals':manifest},indent=2));print('PASS',len(manifest),'original native screenshots verified')
