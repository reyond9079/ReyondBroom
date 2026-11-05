// SPDX-License-Identifier: MIT
#include "../plugins/autolevel/autolevel.cpp"
#include <iostream>
#include <stdexcept>
int main(){int depth=0;std::string text;RBHostV1 h{};h.api_version=1;h.struct_size=offsetof(RBHostV1,read_data);h.text=+[](const char* t){std::cout<<t<<'\n';};auto p=ReyondBroomPluginInit(&h);if(!p)return 1;p->draw();if(alui::ready)return 2;
 h.struct_size=sizeof h;h.read_data=+[](const char*,char*,size_t){return -1;};h.write_data=+[](const char*,const char*,size_t){return 1;};h.input_text=+[](const char*,char*,size_t){return 0;};h.section=+[](const char*,int){return 1;};h.preview_lines=+[](const float*,size_t){};h.replace_selection=h.import_map=+[](const char*,size_t){return 1;};h.number=+[](const char*){return 0.;};h.string=+[](const char*,char*,size_t){return 0;};h.command=h.separator=+[](const char*){};h.button=+[](const char*){return 0;};h.disabled=+[](int){};h.same_line=+[]{};h.input_int=h.checkbox=+[](const char*,int*){return 0;};h.input_float=+[](const char*,float*){return 0;};h.combo=+[](const char*,int*,const char*){return 0;};h.progress=+[](float,const char*){};
 p=ReyondBroomPluginInit(&h);if(!alui::ready)return 3;for(int i=0;i<5;i++){alui::page=i;p->draw();}p->shutdown();std::cout<<"PASS scaffold: five pages and old host guard\n";
}
