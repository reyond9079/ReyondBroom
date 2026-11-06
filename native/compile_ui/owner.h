// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QtGui>
#include <QtCore>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif
// Locate only the editor window belonging to this process. Qt wraps it as a
// foreign transient owner; it never takes ownership of the engine's window.
inline WId reyondEditorWindow(){
#ifdef Q_OS_WIN
 HWND found=nullptr;EnumWindows([](HWND w,LPARAM data)->BOOL{DWORD pid=0;GetWindowThreadProcessId(w,&pid);if(pid!=GetCurrentProcessId()||!IsWindowVisible(w))return TRUE;wchar_t title[256]={};GetWindowTextW(w,title,256);if(std::wstring(title).rfind(L"ReyondBroom",0)!=0)return TRUE;*reinterpret_cast<HWND*>(data)=w;return FALSE;},reinterpret_cast<LPARAM>(&found));return reinterpret_cast<WId>(found);
#else
 QLibrary x{"libX11.so.6"};if(!x.load())return 0;using D=void;using W=unsigned long;
 auto open=reinterpret_cast<D*(*)(const char*)>(x.resolve("XOpenDisplay"));auto close=reinterpret_cast<int(*)(D*)>(x.resolve("XCloseDisplay"));auto root=reinterpret_cast<W(*)(D*)>(x.resolve("XDefaultRootWindow"));auto tree=reinterpret_cast<int(*)(D*,W,W*,W*,W**,unsigned*)>(x.resolve("XQueryTree"));auto atom=reinterpret_cast<W(*)(D*,const char*,int)>(x.resolve("XInternAtom"));auto prop=reinterpret_cast<int(*)(D*,W,W,long,long,int,W,W*,int*,W*,W*,unsigned char**)>(x.resolve("XGetWindowProperty"));auto name=reinterpret_cast<int(*)(D*,W,char**)>(x.resolve("XFetchName"));auto free=reinterpret_cast<int(*)(void*)>(x.resolve("XFree"));
 if(!open||!close||!root||!tree||!atom||!prop||!name||!free)return 0;D* d=open(nullptr);if(!d)return 0;W pidAtom=atom(d,"_NET_WM_PID",0),found=0;
 std::function<void(W,int)> visit=[&](W parent,int depth){W parentReturn=0,rootReturn=0,*children=nullptr;unsigned count=0;if(!tree(d,parent,&rootReturn,&parentReturn,&children,&count))return;
  for(unsigned i=0;i<count&&!found;i++){W type=0,n=0,remaining=0;int format=0;unsigned char* data=nullptr;prop(d,children[i],pidAtom,0,1,0,0,&type,&format,&n,&remaining,&data);if(data&&format==32&&n&&*reinterpret_cast<W*>(data)==W(getpid())){char* title=nullptr;name(d,children[i],&title);if(title&&QByteArray(title).startsWith("ReyondBroom:"))found=children[i];if(title)free(title);}if(data)free(data);if(!found&&depth<3)visit(children[i],depth+1);}if(children)free(children);
 };visit(root(d),0);close(d);return WId(found);
#endif
}
