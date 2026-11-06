#!/usr/bin/env python3
"""Capture the official, unmodified Windows reference through accessibility/OS input.

This is a QA oracle, never an editor component or packaging input. No Qt SDK is used.
"""
import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

user = ctypes.windll.user32
user.SetProcessDPIAware()
from PIL import ImageGrab
from pywinauto import Application, Desktop

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('reference', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
root, out = a.reference.resolve(), a.output.resolve()
repo = Path(__file__).resolve().parents[2]
out.mkdir(parents=True, exist_ok=True)
config = root/'config'
(config/'games/Quake').mkdir(parents=True, exist_ok=True)
shutil.copy2(repo/'tests/fixtures/CompilationProfiles-original.cfg', config/'games/Quake/CompilationProfiles.cfg')
source = root/'qa-reference.map'
source.write_text('// Game: Quake\n// Format: Valve\n'+(repo/'tests/fixtures/compilation-reference.map').read_text())
user.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
user.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_uint]
results = []

def wait(fn, timeout=40):
    deadline = time.monotonic()+timeout
    while time.monotonic()<deadline:
        try:
            value=fn()
            if value:return value
        except Exception:
            pass
        time.sleep(.25)
    raise RuntimeError('Reference UI did not reach the expected state')

for scale in [1, 1.5, 2]:
    for theme in ['light', 'dark']:
        (config/'Preferences.json').write_text(json.dumps({'updater/Ask for auto updates':False, 'Theme':'Dark' if theme=='dark' else 'System'}))
        folder=out/f'{scale}-{theme}'
        folder.mkdir(exist_ok=True)
        process=subprocess.Popen([str(root/'TrenchBroom.exe'),'--portable',str(source)],cwd=root,env={**os.environ,'QT_SCALE_FACTOR':str(scale)})
        app=None
        try:
            app=Application(backend='uia').connect(process=process.pid,timeout=40)
            main=wait(lambda:next((w for w in app.windows() if 'qa-reference' in w.window_text()),None))
            main.set_focus()
            main.menu_select('Run->Compile Map...')
            dialog=wait(lambda:next((w for w in app.windows() if 'Compile' in w.window_text()),None))
            dialog.set_focus()
            for width,height in [(800,600),(1200,800)]:
                rect=wintypes.RECT();user.GetClientRect(dialog.handle,ctypes.byref(rect));outer=dialog.rectangle()
                border_x=outer.width()-(rect.right-rect.left);border_y=outer.height()-(rect.bottom-rect.top)
                user.SetWindowPos(dialog.handle,None,0,0,round(width*scale)+border_x,round(height*scale)+border_y,0x0040)
                time.sleep(.8)
                point=wintypes.POINT();user.ClientToScreen(dialog.handle,ctypes.byref(point));user.GetClientRect(dialog.handle,ctypes.byref(rect))
                widgets=[]
                for child in dialog.descendants():
                    bounds=child.rectangle()
                    widgets.append({'class':child.class_name(),'type':child.element_info.control_type,'text':child.window_text(),'x':(bounds.left-point.x)/scale,'y':(bounds.top-point.y)/scale,'width':bounds.width()/scale,'height':bounds.height()/scale})
                meta={'reference':'TrenchBroom 2026.2 official Windows portable','scale':scale,'theme':theme,'requested_dip':[width,height],'actual_client_px':[rect.right,rect.bottom],'desktop_px':[user.GetSystemMetrics(0),user.GetSystemMetrics(1)],'widgets':widgets}
                stem=folder/f'{width}x{height}'
                stem.with_suffix('.json').write_text(json.dumps(meta,indent=2))
                ImageGrab.grab(bbox=(point.x,point.y,point.x+rect.right,point.y+rect.bottom),all_screens=True).save(stem.with_suffix('.png'))
                assert any(w['text']=='Compile' for w in widgets), 'Compile control missing from accessible reference'
                results.append(str(stem.relative_to(out)))
                (out/'results.json').write_text(json.dumps(results,indent=2))
                print('PASS official Windows TB',scale,theme,width,height,flush=True)
        except Exception:
            ImageGrab.grab(all_screens=True).save(folder/'failure-desktop.png')
            if app:
                (folder/'failure-windows.txt').write_text('\n'.join(str(w) for w in app.windows()),errors='replace')
                for i,w in enumerate(app.windows()):
                    try:w.print_control_identifiers(filename=str(folder/f'failure-tree-{i}.txt'))
                    except Exception:pass
            raise
        finally:
            process.terminate()
            process.wait(timeout=15)
