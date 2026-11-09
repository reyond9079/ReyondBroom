#!/usr/bin/env python3
"""Exercise the actual patched engine's ordered, read-only resource mounting helper.

No engine graphics or product fixtures are required. The helper is extracted
unchanged from the fully patched pinned fs.c and calls a recording mount API.
"""
import argparse
from pathlib import Path
import os
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('engine', type=Path)
a = parser.parse_args()
source = (a.engine/'engine/common/fs.c').read_text(encoding='utf-8')
start = source.index('static void FS_AddEditorResourceDirectories(')
end = source.index('\nstatic void FS_AddGameDirectory (', start)
helper = source[start:end]
# Retain the editor's original overlay position and nonpersistent/local cvar.
call = source.index('FS_AddEditorResourceDirectories(&oldpaths, reloadflags);')
assert source.index('fs_manifest->gamepath', call) > call
assert 'CVAR_NOTFROMSERVER|CVAR_NOSAVE' in source[source.index('static cvar_t ftebroom_mapassetpaths'):].split('\n', 1)[0]
assert 'Cvar_Register(&ftebroom_mapassetpaths, "Filesystem");' in source
harness = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define MAX_OSPATH 1024
#define SPF_PRIVATE 1
#define SPF_COPYPROTECTED 2
#define SPF_WRITABLE 4
#define Q_snprintfz snprintf
typedef struct {const char *string;} cvar_t;
typedef struct {int unused;} searchpath_t;
static cvar_t ftebroom_mapassetpaths={""},ftebroom_assetpath={"/old base"},ftebroom_modassetpath={"/old mod"};
static char mounted[16][MAX_OSPATH],labels[16][64];
static unsigned int mounts, warnings;
static void Con_Printf(const char *fmt,...) { (void)fmt; ++warnings; }
static searchpath_t *FS_AddSingleGameDirectory(searchpath_t **oldpaths,const char *name,const char *dir,unsigned int reload,unsigned int flags)
{
  (void)oldpaths;
  assert(reload==0x1234);
  assert(flags==(SPF_PRIVATE|SPF_COPYPROTECTED));
  assert(!(flags&SPF_WRITABLE));
  assert(mounts<16);
  strcpy(mounted[mounts],dir);strcpy(labels[mounts],name);++mounts;
  return NULL;
}
'''
cases = r'''
static void run(const char *paths){mounts=0;warnings=0;ftebroom_mapassetpaths.string=paths;FS_AddEditorResourceDirectories(NULL,0x1234);}
int main(void)
{
  run("");assert(mounts==2&&!strcmp(mounted[0],"/old base")&&!strcmp(mounted[1],"/old mod"));
  run("/Quake Game/id1;/Quake Game/modA;/Quake Game/AD19b5");
  assert(mounts==3&&!strcmp(mounted[0],"/Quake Game/id1")&&!strcmp(mounted[1],"/Quake Game/modA")&&!strcmp(mounted[2],"/Quake Game/AD19b5"));
  assert(strcmp(labels[0],labels[1])&&strcmp(labels[1],labels[2]));
  run("D:\\Quake Game\\id1;D:/Quake Game/AD19b5;\\\\server\\game\\custom mod");
  assert(mounts==3&&!strcmp(mounted[0],"D:\\Quake Game\\id1")&&!strcmp(mounted[1],"D:/Quake Game/AD19b5")&&!strcmp(mounted[2],"\\\\server\\game\\custom mod"));
  run(";/base;;/mod;");assert(mounts==2&&!strcmp(mounted[0],"/base")&&!strcmp(mounted[1],"/mod"));
  run("relative;D:relative;/valid");assert(mounts==1&&warnings==2&&!strcmp(mounted[0],"/valid"));
  char longer[MAX_OSPATH+64];longer[0]='/';memset(longer+1,'a',sizeof(longer)-2);longer[sizeof(longer)-1]=0;
  run(longer);assert(mounts==0&&warnings==1);
  ftebroom_assetpath.string="";ftebroom_modassetpath.string="";run("");assert(mounts==0);
  puts("PASS engine ordered resource stack, spaces, Windows/UNC paths, read-only flags, malformed entries, legacy fallback");
}
'''
with tempfile.TemporaryDirectory(prefix='rb-resource-stack-') as folder:
    path = Path(folder)
    (path/'check.c').write_text(harness+helper+cases, encoding='utf-8')
    executable = path/('check.exe' if os.name == 'nt' else 'check')
    subprocess.run([os.environ.get('CC', 'gcc'), '-std=c99', '-Wall', '-Wextra', '-Werror', str(path/'check.c'), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
