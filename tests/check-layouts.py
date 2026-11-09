from pathlib import Path
import sys
r=Path(sys.argv[1]);log=(r/'qconsole.log').read_text(errors='replace')
assert 'FTEBROOM_LAYOUT_FAIL' not in log and 'FTEBROOM_VIEW_FAIL' not in log,log[-12000:]
names=[f'rectangles_{i}' for i in range(1,5)]+['cycle_single_xy','cycle_single_wrap','maximize','restore','hover_focus','drag_focus_locked','divider_drag']
names += [f'cycle_{prefix}_{count}_pane_{pane}' for prefix in ('layout','wrap') for count in (2,3,4) for pane in range(count)]
for name in names: assert 'FTEBROOM_LAYOUT_PASS '+name in log,name
for name in [f'{kind}_{i}' for i in range(1,4) for kind in ['viewport_projection','linked_pan']]+['reset_2d_zoom']:
    assert 'FTEBROOM_VIEW_PASS '+name in log,name
def faces(name): return sorted(x for x in (r/'maps'/f'layout_{name}.map').read_text().splitlines() if x.startswith('( '))
assert faces('before')==faces('after')
for name in ['four','two','three']: assert (r/f'layouts-{name}.png').exists()
print(f'PASS: {len(names)+7} layout/projection/focus assertions and unchanged MAP geometry.')

# A passing projection test is insufficient: verify visible geometry in the cached panes.
from PIL import Image
im=Image.open(r/'layouts-four.png').convert('RGB');w,h=im.size
sx=48+(w-358)*.5;sy=114+(h-140)*.5
for x0,y0,x1,y1 in [(sx+4,114,w-310,sy-4),(48,sy+4,sx-4,h-26),(sx+4,sy+4,w-310,h-26)]:
    pane=im.crop((int(x0+100),int(y0+30),int(x1-20),int(y1-90)))
    assert sum(1 for red,green,blue in pane.getdata() if 85<=red<=220 and abs(red-green)<5 and abs(red-blue)<25)>20,'Missing cached wireframe'
assert sum(1 for red,green,blue in im.crop((48,114,int(sx-4),int(sy-4))).getdata() if blue>red*1.4 and blue>green*1.1 and blue>80)>100,'Missing perspective geometry'
print('PASS: perspective geometry and cached wireframes are present in the screenshot.')
