#!/usr/bin/env python3
"""Verify patched engine activation policy, or query a real focused editor HWND.

The real HWND query sends WM_MOUSEACTIVATE only. It neither clicks a widget nor
invokes ImGui callbacks. Call after the normal OS focus and engine-tick barrier.
"""
import argparse
import ctypes
import json
import os
from pathlib import Path
import subprocess
import tempfile


def check_active_editor_window(hwnd, output=None):
    if os.name != 'nt':
        raise RuntimeError('A real editor HWND check requires Windows')
    from ctypes import wintypes
    user = ctypes.windll.user32
    class GuiThreadInfo(ctypes.Structure):
        _fields_ = [('cbSize', wintypes.DWORD), ('flags', wintypes.DWORD),
                    ('hwndActive', wintypes.HWND), ('hwndFocus', wintypes.HWND),
                    ('hwndCapture', wintypes.HWND), ('hwndMenuOwner', wintypes.HWND),
                    ('hwndMoveSize', wintypes.HWND), ('hwndCaret', wintypes.HWND),
                    ('rcCaret', wintypes.RECT)]
    user.GetForegroundWindow.restype = wintypes.HWND
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetWindowThreadProcessId.restype = wintypes.DWORD
    user.GetGUIThreadInfo.argtypes = [wintypes.DWORD, ctypes.POINTER(GuiThreadInfo)]
    user.GetGUIThreadInfo.restype = wintypes.BOOL
    user.IsChild.argtypes = [wintypes.HWND, wintypes.HWND]
    user.IsChild.restype = wintypes.BOOL
    user.SendMessageTimeoutW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM,
                                       wintypes.LPARAM, wintypes.UINT, wintypes.UINT,
                                       ctypes.POINTER(ctypes.c_size_t)]
    user.SendMessageTimeoutW.restype = ctypes.c_ssize_t
    info = GuiThreadInfo(); info.cbSize = ctypes.sizeof(info)
    assert user.GetGUIThreadInfo(user.GetWindowThreadProcessId(hwnd, None), ctypes.byref(info))
    assert user.GetForegroundWindow() == hwnd and info.hwndActive == hwnd
    assert info.hwndFocus == hwnd or (info.hwndFocus and user.IsChild(hwnd, info.hwndFocus))
    result = ctypes.c_size_t()
    assert user.SendMessageTimeoutW(hwnd, 0x21, hwnd, (0x201 << 16) | 1, 2, 5000, ctypes.byref(result))
    proof = {'method': 'Actual editor HWND WM_MOUSEACTIVATE policy response; no mouse action or ImGui callback',
             'editor': hwnd, 'foreground': user.GetForegroundWindow(), 'active': info.hwndActive,
             'focus': info.hwndFocus, 'response': result.value, 'expected': 1}
    if output:
        Path(output).write_text(json.dumps(proof, indent=2)+'\n')
    assert result.value == 1, 'The focused editor must accept an activation click (MA_ACTIVATE), not eat it'
    return proof


def check_source(engine):
    source = (engine/'engine/gl/gl_vidnt.c').read_text(encoding='utf-8')
    start = source.index('\t\tcase WM_MOUSEACTIVATE:')
    end = source.index('\n\t\t\tbreak;', start)+len('\n\t\t\tbreak;')
    case = source[start:end]
    harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uintptr_t HWND;
#define WM_MOUSEACTIVATE 0x21
#define MA_ACTIVATE 1
#define MA_ACTIVATEANDEAT 2
static struct {int activeapp;} vid;
static HWND foreground,active,focused;
static int editor;
static HWND GetFocus(void){return focused;}
static HWND GetForegroundWindow(void){return foreground;}
static HWND GetActiveWindow(void){return active;}
static int IsChild(HWND parent,HWND child){return parent==1&&child==2;}
static int Cmd_Exists(const char *name){assert(!strcmp(name,"ftebroom_ui"));return editor;}
static int response(HWND hWnd){int lRet=0;switch(WM_MOUSEACTIVATE){
'''
    tests = r'''
}return lRet;}
static void expect(int iseditor,int internal,HWND fg,HWND threadactive,HWND focus,int expected){
 editor=iseditor;vid.activeapp=internal;foreground=fg;active=threadactive;focused=focus;
 assert(response(1)==expected);
}
int main(void){
 expect(1,1,1,1,1,MA_ACTIVATE);          /* observed already-focused editor */
 expect(1,1,1,1,2,MA_ACTIVATE);          /* legitimate native child focus */
 expect(0,1,1,1,1,MA_ACTIVATEANDEAT);    /* generic game keeps prior behavior */
 expect(1,0,1,1,1,MA_ACTIVATEANDEAT);    /* engine has not acknowledged focus */
 expect(1,1,3,1,1,MA_ACTIVATEANDEAT);    /* another foreground application */
 expect(1,1,1,3,1,MA_ACTIVATEANDEAT);    /* another active window on thread */
 expect(1,1,1,1,3,MA_ACTIVATEANDEAT);    /* unrelated focused window */
 expect(1,1,1,1,0,MA_ACTIVATEANDEAT);    /* no keyboard focus */
 expect(1,0,3,3,3,MA_ACTIVATEANDEAT);    /* genuinely inactive editor */
 puts("PASS focused editor activation click, child focus, inactive editor and generic-game policy");
}
'''
    with tempfile.TemporaryDirectory(prefix='rb-win-activation-') as directory:
        root = Path(directory)
        (root/'policy.c').write_text(harness+case+tests)
        executable = root/('policy.exe' if os.name == 'nt' else 'policy')
        subprocess.run([os.environ.get('CC', 'gcc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                        str(root/'policy.c'), '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('engine', nargs='?', type=Path)
    parser.add_argument('--hwnd', type=lambda text: int(text, 0))
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.hwnd is not None:
        print(json.dumps(check_active_editor_window(args.hwnd, args.output), indent=2))
    else:
        if args.engine is None:
            parser.error('Supply an engine source directory or --hwnd')
        check_source(args.engine)
