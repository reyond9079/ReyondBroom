// SPDX-License-Identifier: MIT
#include "../sdk/reyondbroom_plugin.h"
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
int main(int argc,char** argv){if(argc!=2)return 2;RBHostV1 h{};h.api_version=1;h.struct_size=sizeof h;h.number=+[](const char*){return 0.;};h.string=+[](const char*,char* p,size_t n){if(n)*p=0;return 0;};h.text=h.separator=h.command=h.log=+[](const char*){};h.input_int=h.checkbox=+[](const char*,int*){return 0;};h.input_float=+[](const char*,float*){return 0;};h.button=+[](const char*){return 0;};h.combo=+[](const char*,int*,const char*){return 0;};h.same_line=+[]{};h.disabled=+[](int){};h.progress=+[](float,const char*){};h.read_data=+[](const char*,char*,size_t){return -1;};h.write_data=+[](const char*,const char*,size_t){return 1;};h.replace_selection=h.import_map=+[](const char*,size_t){return 1;};h.input_text=+[](const char*,char*,size_t){return 0;};h.preview_lines=+[](const float*,size_t){};h.section=+[](const char*,int){return 1;};
#ifdef _WIN32
 auto module=LoadLibraryA(argv[1]);auto init=module?reinterpret_cast<const RBPluginV1*(*)(const RBHostV1*)>(GetProcAddress(module,"ReyondBroomPluginInit")):nullptr;
#else
 auto module=dlopen(argv[1],RTLD_NOW);auto init=module?reinterpret_cast<const RBPluginV1*(*)(const RBHostV1*)>(dlsym(module,"ReyondBroomPluginInit")):nullptr;
#endif
 if(!init)return 3;auto plugin=init(&h);if(!plugin)return 4;plugin->draw();plugin->tick();if(plugin->busy())return 5;plugin->shutdown();
#ifdef _WIN32
 FreeLibrary(module);
#else
 dlclose(module);
#endif
 std::cout<<"PASS pre-extension compiled plugin loads, draws and shuts down on extended v1 host\n";return 0;}
