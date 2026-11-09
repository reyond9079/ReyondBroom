#!/usr/bin/env python3
"""Temporary, diagnostic-branch-only engine logging; never edits product source."""
from pathlib import Path
import argparse, hashlib, json, subprocess
p=argparse.ArgumentParser();p.add_argument('engine',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
header=r'''#ifndef RB_WINDOWS_INPUT_DIAG_H
#define RB_WINDOWS_INPUT_DIAG_H
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
static void RB_InputLog(const char *stage,const char *format,...)
{
    static PVOID volatile storedhandle;
    HANDLE file=(HANDLE)InterlockedCompareExchangePointer(&storedhandle,NULL,NULL);
    char payload[2048],line[3072],path[1024];
    va_list args;int bytes;
    if(!file){
        DWORD size=GetEnvironmentVariableA("RB_INPUT_DIAGNOSTIC",path,sizeof(path));
        HANDLE created;
        if(!size||size>=sizeof(path))return;
        created=CreateFileA(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        if(created==INVALID_HANDLE_VALUE)return;
        file=(HANDLE)InterlockedCompareExchangePointer(&storedhandle,created,NULL);
        if(file)CloseHandle(created);else file=created;
    }
    va_start(args,format);vsnprintf(payload,sizeof(payload),format,args);va_end(args);
    bytes=snprintf(line,sizeof(line),"%llu tid=%lu %s %s\r\n",(unsigned long long)GetTickCount64(),(unsigned long)GetCurrentThreadId(),stage,payload);
    if(bytes>0){DWORD written;WriteFile(file,line,(DWORD)(bytes<(int)sizeof(line)?bytes:(int)sizeof(line)-1),&written,NULL);}
}
#endif
'''
changes={}
for name in ['engine/gl/gl_vidnt.c','engine/client/in_win.c','engine/client/in_generic.c','engine/client/keys.c','engine/client/menu.c']:
    file=a.engine/name;before=file.read_bytes();text=before.decode().replace('\r\n','\n')
    needle='#include "quakedef.h"';assert needle in text,name
    text=text.replace(needle,needle+'\n#include "rb_windows_input_diag.h"',1)
    if name.endswith('gl_vidnt.c'):
        needle='\tif ( uMsg == uiWheelMessage )\n\t\tuMsg = WM_MOUSEWHEEL;'
        assert text.count(needle)==1
        text=text.replace(needle,needle+r'''
    if(uMsg==WM_MOUSEACTIVATE || uMsg==WM_SETFOCUS || uMsg==WM_KILLFOCUS ||
       uMsg==WM_LBUTTONDOWN || uMsg==WM_LBUTTONUP || uMsg==WM_INPUT || uMsg==WM_CAPTURECHANGED){
        GUITHREADINFO info;memset(&info,0,sizeof(info));info.cbSize=sizeof(info);
        GetGUIThreadInfo(GetWindowThreadProcessId(hWnd,NULL),&info);
        RB_InputLog("WMSG","msg=%u hwnd=%p wp=%llu lp=%lld activeapp=%d mouseactive=%d foreground=%p active=%p focus=%p capture=%p",
          uMsg,hWnd,(unsigned long long)wParam,(long long)lParam,vid.activeapp,mouseactive,GetForegroundWindow(),info.hwndActive,info.hwndFocus,info.hwndCapture);
    }
''',1)
    elif name.endswith('in_win.c'):
        needle='void INS_MouseEvent (int mstate)\n{\n\tint\t\ti;'
        assert text.count(needle)==1
        text=text.replace(needle,needle+r'''
    if(mstate || mstate!=sysmouse.oldbuttons){
        int rawcount=0;
#ifdef USINGRAWINPUT
        rawcount=rawmicecount;
#endif
        RB_InputLog("MOUSE_EVENT","state=%d old=%d buttons=%d mouseactive=%d activeapp=%d dinput=%d rawmice=%d devid=%u",
            mstate,sysmouse.oldbuttons,sysmouse.numbuttons,mouseactive,vid.activeapp,dinput,rawcount,sysmouse.qdeviceid);
    }
''',1)
    elif name.endswith('in_generic.c'):
        needle='void IN_KeyEvent(unsigned int devid, int down, int keycode, int unicode)\n{\n\tstruct eventlist_s *ev = in_newevent();'
        assert text.count(needle)==1
        text=text.replace(needle,needle+r'''
    if(keycode==178 || keycode==-1)RB_InputLog("ENQUEUE","key=%d down=%d devid=%u avail=%d used=%d slot=%p",keycode,down,devid,events_avail,events_used,ev);
''',1)
    elif name.endswith('keys.c'):
        needle='void Key_Event (unsigned int devid, int key, unsigned int unicode, qboolean down)\n{'
        assert text.count(needle)==1
        text=text.replace(needle,needle+r'''
    if(key==178)RB_InputLog("KEY_EVENT","key=%d down=%d devid=%u destination=%u",key,down,devid,key_dest_mask);
''',1)
    elif name.endswith('menu.c'):
        needle='void Menu_KeyEvent(qboolean isdown, int deviceid, int key, int unicode)\n{'
        assert text.count(needle)==1
        text=text.replace(needle,needle+r'''
    if(key==178)RB_InputLog("MENU_EVENT","key=%d down=%d top=%p prompt=%p context=%p callback=%p",key,isdown,topmenu,promptmenu,topmenu?topmenu->ctx:NULL,topmenu?topmenu->keyevent:NULL);
''',1)
    file.write_text(text)
    changes[name]={'before_sha256':hashlib.sha256(before).hexdigest(),'after_sha256':hashlib.sha256(file.read_bytes()).hexdigest()}
file=a.engine/'engine/common/rb_windows_input_diag.h';file.write_text(header)
changes['engine/common/rb_windows_input_diag.h']={'before_sha256':None,'after_sha256':hashlib.sha256(file.read_bytes()).hexdigest()}
(a.output/'instrumentation-header.h').write_text(header)
(a.output/'instrumentation-hashes.json').write_text(json.dumps(changes,indent=2)+'\n')
with (a.output/'engine-source-diff.patch').open('wb') as stream:subprocess.run(['git','-C',str(a.engine),'diff','--binary'],stdout=stream,check=True)
print('Instrumented',len(changes),'engine source files')
