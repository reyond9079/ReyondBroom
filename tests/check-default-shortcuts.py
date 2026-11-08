"""Audit the real catalogue with FTE's key parser and the editor conflict check.

Usage: python3 tests/check-default-shortcuts.py /path/to/fteqw
No editor profile is read or modified.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
engine = Path(sys.argv[1]) / "engine/client"
actions = (root / "native/ftebroom_actions.h").read_text()
preferences = (root / "native/ftebroom_preferences.h").read_text()
keys = (engine / "keys.c").read_text()


def function(source, signature):
    start = source.index(signature)
    begin = source.index("{", start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


prefix = actions[actions.index("enum ActionContext"):actions.index("static bool actionReference")]
add = actions[actions.index("  auto add ="):actions.index("  if (actions.empty())")]
table = keys[keys.index("typedef struct\n{"):keys.index("#if defined(CSQC_DAT)")]
key_parser = function(keys, "int Key_StringToKeynum (")
source = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <strings.h>
#include <vector>
#define countof(a) (sizeof(a)/sizeof((a)[0]))
#define strnicmp strncasecmp
#define Q_strncasecmp strncasecmp
#define Q_strcasecmp strcasecmp
'''
source += '#include "' + str(engine / "keys.h") + '"\n'
source += table + key_parser
source += r'''
struct TestInput {
  int GetKeyCode(const char* text,int*) { int modifier;return Key_StringToKeynum(text,&modifier); }
} testInput, *input=&testInput;
'''
source += prefix
source += 'static std::map<std::string,std::string> shortcutOverrides,shortcutDefaultBindings;\n'
for signature in ("static std::string shortcutVariable(", "static void shortcutApply(",
                  "static void shortcutRestoreDefault(",
                  "static bool shortcutOverlap(", "static std::set<std::string> shortcutConflicts("):
    source += function(preferences, signature) + "\n"
source += "int main(){\n" + add
source += '#include "' + str(root / "native/ftebroom_actions.inc") + '"\n'
source += r'''
  for(const auto& a:actions)for(const auto& key:a.defaults)assert(key.key>0);
  auto defaults=shortcutConflicts();
  for(const auto& id:defaults)std::cerr<<"Default conflict: "<<id<<'\n';
  assert(defaults.empty());
  Action* vertex=nullptr;Action* clip=nullptr;
  for(auto& a:actions){
    if(std::string(a.id)=="Menu/Tools/Vertex")vertex=&a;
    if(std::string(a.id)=="Menu/Tools/Clip")clip=&a;
  }
  assert(vertex&&clip);
  for(const auto& a:actions)shortcutDefaultBindings.emplace(a.id,a.shortcutText);
  const auto stableVariable=shortcutVariable(vertex->id);
  const auto initialVertex=vertex->shortcutText;
  shortcutApply(*vertex,clip->shortcutText);
  auto conflict=shortcutConflicts();
  assert(conflict.count(vertex->id)&&conflict.count(clip->id));
  assert(shortcutVariable(vertex->id)==stableVariable);
  shortcutOverrides[stableVariable]=vertex->shortcutText;
  shortcutOverrides[shortcutVariable(clip->id)]="Ctrl+Shift+c";
  shortcutRestoreDefault(*vertex);
  assert(vertex->shortcutText==initialVertex&&!shortcutOverrides.count(stableVariable));
  assert(shortcutOverrides.at(shortcutVariable(clip->id))=="Ctrl+Shift+c");
  assert(shortcutConflicts().empty());
  assert(!shortcutOverlap(CtxModelEditor,CtxAny));
  assert(!shortcutOverlap(CtxView3D|CtxFaceSelection,CtxView3D|CtxNodeSelection));
  assert(shortcutOverlap(CtxAny,CtxView3D|CtxFaceSelection));
  std::cout<<"PASS: "<<actions.size()<<" default actions use valid FTE key codes; zero overlapping bindings; user conflicts remain detected; stable override IDs\n";
}
'''
with tempfile.TemporaryDirectory(prefix="rb-default-shortcuts-") as directory:
    src = Path(directory) / "audit.cpp"
    exe = Path(directory) / "audit"
    src.write_text(source)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wno-write-strings", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
