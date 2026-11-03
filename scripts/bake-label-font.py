# Deterministic proportional label atlas for FTE's built-in KFONT loader.
# Source Sans Pro is licensed under the SIL Open Font License.
from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
from fontTools.ttLib import TTFont
root=Path(__file__).resolve().parents[1];out=root/'runtime/fonts'
font=ImageFont.truetype(str(out/'SourceSansPro-Regular.otf'),26)
fallback=ImageFont.truetype(str(out/'FTEBroom-CJK.otf'),26)
primary=TTFont(out/'SourceSansPro-Regular.otf').getBestCmap()
secondary=TTFont(out/'FTEBroom-CJK.otf').getBestCmap()
atlas=Image.new('RGBA',(1024,1024),(0,0,0,0));draw=ImageDraw.Draw(atlas)
rows=['texture "fonts/ReyondLabels.tga"'];x=y=0;h=30
points=list(range(32,127))+list(range(160,592))+list(range(880,1328))+list(range(8192,8304))
for cp in points:
 if cp not in primary and cp not in secondary:continue
 face=font if cp in primary else fallback
 c=chr(cp);width=max(1,round(face.getlength(c)))
 if x+width+2>=1024:x=0;y+=h+2
 if y+h>1024:raise RuntimeError('Font atlas overflow')
 draw.text((x,y+22),c,font=face,fill=(255,255,255,255),anchor='ls')
 # KFONT removes two rows from each edge and normalizes advances by height.
 rows.append(f'{cp} {x} {y} {width} {h} 0')
 x+=width+2
atlas.save(out/'ReyondLabels.tga',compression='tga_rle')
(out/'ReyondLabels.kfont').write_text('\n'.join(rows)+'\n')
print('Baked label font:',len(rows)-1,'characters')
