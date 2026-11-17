#!/usr/bin/env python3
"""Regress Quake model paths, declared bounds and committed entity rotations."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('runtime',type=Path)
parser.add_argument('output',type=Path)
parser.add_argument('--module',type=Path)
parser.add_argument('--csprogs',type=Path)
args=parser.parse_args()
repo=Path(__file__).resolve().parents[1]
out=args.output.resolve()
root=out/'editor-runtime'
assert not root.exists(),'Use a fresh QA output directory'
shutil.copytree(args.runtime.resolve(),root,ignore=shutil.ignore_patterns('updates','downloads','__pycache__'))
windows=os.name=='nt'
if args.module:
    shutil.copy2(args.module,root/('fteplug_ftebroom_x64.dll' if windows else 'fteplug_ftebroom_amd64.so'))
data=root/'ftebroom'
if args.csprogs:
    shutil.copy2(args.csprogs,data/'csaddon.dat')
model=data/'progs/qa-offset.mdl'
model.parent.mkdir(exist_ok=True)
vertices=[(0,0,0,0),(96,0,0,0),(96,16,0,0),(0,16,0,0)]
header=struct.pack('<4si3f3ff3f8if',b'IDPO',6,1,1,1,0,0,0,98,0,0,0,1,4,4,4,2,1,0,0,1)
body=struct.pack('<i',0)+bytes(range(16))
for x,y in ((0,0),(3,0),(3,3),(0,3)):
    body+=struct.pack('<3i',0,x,y)
body+=struct.pack('<4i',1,0,1,2)+struct.pack('<4i',1,0,2,3)
body+=struct.pack('<i4B4B16s',0,0,0,0,0,96,16,0,0,b'qa')
body+=b''.join(struct.pack('<4B',*v) for v in vertices)
model.write_bytes(header+body)
(data/'gfx').mkdir(exist_ok=True)
(data/'gfx/palette.lmp').write_bytes(b''.join(bytes((x,x,x)) for x in range(256)))
definition=data/'qa-rb36.fgd'
definition.write_text('@PointClass size(-16 -16 -24, 16 16 32) model("progs/qa-offset.mdl") = info_player_start : "Player start" []\n',encoding='utf-8')
for folder in (root,data):
    (folder/'ftebroom-game-profile.txt').write_text('2 0 "" "" "" "" 1 5 "" 1 1\n')
    (folder/'ftebroom-project.txt').write_text(data.as_posix())
    (folder/'ftebroom-quake1.txt').write_text('"" '+json.dumps(data.as_posix())+' "" "" 1 0 '+json.dumps(data.as_posix())+' 0\n')
subprocess.run([sys.executable,str(repo/'tests/make-model-pick-fixture.py'),str(data)],check=True)
config=data/'qa-rb36.cfg'
config.write_text('exec ftebroom.cfg\n'
    f'in 6 ftebroom_definitions "{definition.as_posix()}"\n'
    f'in 7 ftebroom_entity_definition_path "external:{definition.as_posix()}"\n'
    'in 8 ftebroom_test_entities rb36\n'
    'in 9 mod_terrain_save "" rb36-saved\n'
    'in 10 edit rb36-saved\n'
    'in 12 ftebroom_test_entities rb36-reopened\n'
    'in 12.5 set mod_obj_orientation 0\n'
    'in 13 ftebroom_test_pick mesh\n'
    'in 15 screenshot rb36-entities.png\n'
    'in 17 quit force\n',encoding='utf-8')
engine=root/('bin/fteqw64.exe' if windows else 'fteqw-gl64')
command=[str(engine),'-basedir',str(root),'-nohome','-noupdates','-nosound','-condebug','-window','-width','1280','-height','800',
         '+set','ftebroom_skip_setup','1','+vid_width','1280','+vid_height','800','+exec',config.name]
log=out/'editor.log'
with log.open('wb') as stream:
    subprocess.run(command,cwd=root,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'},stdout=stream,stderr=subprocess.STDOUT,check=True,timeout=70)
text=log.read_text(errors='replace')
for console in root.rglob('qconsole.log'):
    text+='\n'+console.read_text(encoding='utf-8',errors='replace')
assert not re.search(r'_FAIL|CSQC_Abort|runaway loop error',text),text[-6000:]
checks=['misc_model_mdl_from_progs','explicit_player_bounds','mesh_keeps_full_culling_bounds',
        'pick_mesh_beyond_definition_box','capture_asymmetric_player','rotation_commits_angle',
        'rotation_single_undo','rotation_redo','rotating_preserves_definition_box',
        'default_2d_grid_brighter','saved_rotation_reopens']
for check in checks:
    assert 'FTEBROOM_ENTITY_PASS rb36_'+check in text,check
    print('PASS rb36_'+check)
for check in ('mesh_surface_hit_at_its_depth','empty_area_inside_model_box_is_not_pickable',
              'rotated_non_uniform_mesh_hit','large_mesh_repeated_click_queries',
              'frame_hover_does_not_trace_render_meshes'):
    assert 'FTEBROOM_PICK_PASS '+check in text,check
    print('PASS '+check)
benchmark=re.search(r'FTEBROOM_MESH_PICK_BENCHMARK 256 changing rays, ([\d.e+-]+) ms per query',text)
assert benchmark,'Missing mesh-picking timing'
print(benchmark.group(0))
saved=data/'maps/rb36-saved.map'
assert saved.is_file()
saved_angle=re.search(r'"angle"\s+"([^\"]+)"',saved.read_text(encoding='utf-8'))
assert saved_angle and abs(float(saved_angle[1])-90)<.001
(out/'results.json').write_text(json.dumps({'passed':True,'checks':checks,'mesh_query_ms':float(benchmark[1])},indent=2)+'\n')
