// SPDX-License-Identifier: MIT
#include "imgui.h"
#include "imgui_internal.h"
#include "../plugins/autolevel/autolevel.cpp"
#include <iostream>
#include <set>
using namespace alui;
static std::map<ImGuiID,std::string> ids;
static std::vector<std::string> errors;
static std::map<std::string,std::string> data;
static bool expanded=true;static std::map<std::string,ImGuiID> pageIds;
static void mark(const char* label,const char* type){auto id=ImGui::GetID(label);std::string full=label;if(auto at=full.find("###autolevel-page-");at!=std::string::npos){auto key=full.substr(at);auto prior=pageIds.find(key);if(prior!=pageIds.end()&&prior->second!=id)errors.push_back("Page ID changed with active label");pageIds[key]=id;}auto old=ids.find(id);if(old!=ids.end())errors.push_back(std::string("ID collision: ")+old->second+" / "+type+" "+label);else ids[id]=std::string(type)+" "+label;}
static void check(bool v,const char* what){if(!v)throw std::runtime_error(what);}
int main(){try{
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={1400,1000};io.DeltaTime=1.f/60;io.IniFilename=nullptr;io.ConfigErrorRecoveryEnableAssert=false;io.ConfigErrorRecoveryEnableDebugLog=false;io.ConfigErrorRecoveryEnableTooltip=false;GImGui->ErrorCallback=+[](ImGuiContext*,void*,const char* msg){errors.push_back(msg);};unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 RBHostV1 host{};host.api_version=1;host.struct_size=offsetof(RBHostV1,preview_colored_lines);
 host.number=+[](const char*){return 2.;};host.string=+[](const char*,char* out,size_t n){if(n)*out=0;return 0;};host.command=host.log=+[](const char*){};
 host.text=+[](const char* s){ImGui::TextWrapped("%s",s);};host.separator=+[](const char* s){ImGui::SeparatorText(s);};
 host.input_int=+[](const char* s,int* p){mark(s,"int");return int(ImGui::InputInt(s,p));};host.input_float=+[](const char* s,float* p){mark(s,"float");return int(ImGui::InputFloat(s,p));};
 host.checkbox=+[](const char* s,int* p){mark(s,"bool");bool b=*p;int changed=ImGui::Checkbox(s,&b);*p=b;return changed;};host.combo=+[](const char* s,int* p,const char* choices){mark(s,"combo");return int(ImGui::Combo(s,p,choices));};host.button=+[](const char* s){mark(s,"button");return int(ImGui::Button(s));};host.same_line=+[]{ImGui::SameLine();};host.disabled=+[](int begin){if(begin)ImGui::BeginDisabled();else ImGui::EndDisabled();};host.progress=+[](float n,const char* s){ImGui::ProgressBar(n,{-1,0},s);};
 host.read_data=+[](const char* s,char* out,size_t size){auto it=data.find(s);if(it==data.end())return -1;if(!out)return int(it->second.size());if(size<=it->second.size())return -1;std::memcpy(out,it->second.c_str(),it->second.size()+1);return int(it->second.size());};host.write_data=+[](const char* s,const char* bytes,size_t n){data[s]=std::string(bytes,n);return 1;};host.import_map=host.replace_selection=+[](const char*,size_t){return 1;};host.input_text=+[](const char* s,char* p,size_t n){mark(s,"text");return int(ImGui::InputText(s,p,n));};host.preview_lines=+[](const float*,size_t){};host.section=+[](const char* s,int){mark(s,"section");ImGui::SetNextItemOpen(expanded,ImGuiCond_Always);return int(ImGui::CollapsingHeader(s));};
 auto plugin=ReyondBroomPluginInit(&host);check(plugin&&ready,"plugin initializes");
 int failed=0,frames=0;
 auto frame=[&](const char* scenario,float width=1000.f){
  ids.clear();errors.clear();ImGui::NewFrame();ImGui::SetNextWindowSize({width,950});ImGui::Begin("AutoLevel");ImGui::PushID("org.reyondbroom.autolevel");plugin->draw();ImGui::PopID();ImGui::End();ImGui::Render();
  for(auto& e:errors)std::cerr<<scenario<<": "<<e<<'\n';
  if(GImGui->DisabledStackSize!=0)errors.push_back("disabled stack imbalance");
  if(!errors.empty())failed++;else std::cout<<"PASS real ImGui "<<scenario<<" unique IDs and balanced stacks\n";frames++;
 };
 for(bool open:{true,false}){expanded=open;for(int p=0;p<5;p++){page=p;auto scenario=std::string(open?"expanded page ":"collapsed page ")+std::to_string(p);frame(scenario.c_str(),open?1000.f:340.f);}}
 expanded=true;page=2;blockIndex=variantIndex=0;blockEditSelection=1;frame("Block Connection mode");
 repos[0].useFilling=true;blockEditSelection=2;frame("Block Fill mode");
 repos[0].blocks[0].variants[0].layerSettings.layer=1;blockEditSelection=2;frame("Block Layer mode");
 repos[0].blocks[0].variants[0].layerSettings.layer=0;auto variant=repos[0].blocks[0].variants[0];variant.actions={BlockAction::RotateZ};repos[0].blocks[0].variants.push_back(variant);variantIndex=1;blockEditSelection=0;frame("Variant actions and offset");
 page=3;bigIndex=0;bigEditMode=1;frame("Big Block external connections");
 page=0;settings.tool=1;frame("Builder Selection painting");settings.tool=2;frame("Builder Connecting");
 auto second=builders[0];second.uid="second";second.name=builders[0].name;builders.push_back(second);page=4;frame("Duplicate builder names in groups");
 page=1;auto empty=Repo{};empty.uid="empty";repos.push_back(empty);repoIndex=repos.size()-1;frame("Empty repository");page=2;frame("Empty repository block page");page=3;frame("Empty repository Big Block page");
 repoIndex=0;page=0;settings.tool=0;job=std::async(std::launch::deferred,[]{return Job{};});frame("Busy worker disabled controls");job.get();
 page=1;repos[0].groups={"Duplicate","Duplicate"};repos[0].weightGroups={"Duplicate","Duplicate"};repos[0].actionsGroups.push_back(repos[0].actionsGroups[0]);frame("Duplicate editable group names");
 std::cout<<"Real ImGui frames checked: "<<frames<<'\n';
 plugin->shutdown();ImGui::DestroyContext();if(failed)return 1;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

void FteBroomTranslate(const char*&,const char*&){}
