// SPDX-License-Identifier: MIT
#include "../plugins/levelgen/levelgen.cpp"
#include <iostream>
#include <thread>
#include <map>
static std::map<std::string,std::string> data;
static std::string clicked,imported,replaced;static int disabledDepth=0;static size_t previewSegments=0;
static void check(bool value,const char* name){if(!value)throw std::runtime_error(std::string(name)+": "+message);std::cout<<"PASS "<<name<<'\n';}
static double num(const char* name){return std::string(name)=="rb.profile"?2:1;}
static int str(const char*,char* out,size_t count){return std::snprintf(out,count,"fb_grey");}
static int readData(const char* name,char* out,size_t count){auto it=data.find(name);if(it==data.end())return -1;if(!out)return int(it->second.size());if(count<=it->second.size())return -1;std::copy(it->second.begin(),it->second.end(),out);out[it->second.size()]=0;return int(it->second.size());}
static int writeData(const char* name,const char* bytes,size_t count){data[name]=std::string(bytes,count);return 1;}
static void command(const char* text){if(std::string(text).find("surfaces block")!=std::string::npos)data["plugin-levelgen-capture.map"]="BOUNDS 0 0 0 128 128 128\n{\n\"classname\" \"worldspawn\"\n"+rbgen::box({0,0,0},{128,128,128},"fb_grey")+"}\n";}
static void text(const char*){}static int intValue(const char*,int*){return 0;}static int floatValue(const char*,float*){return 0;}static int combo(const char*,int*,const char*){return 0;}
static int button(const char* label){if(disabledDepth||clicked!=label)return 0;clicked.clear();return 1;}
static void disabled(int start){disabledDepth+=start?1:-1;if(disabledDepth<0)throw std::runtime_error("UI disabled stack underflow");}
static int apply(const char* value,size_t count){imported.assign(value,count);return 1;}static int replace(const char* value,size_t count){replaced.assign(value,count);return 1;}
static void previewLines(const float*,size_t count){previewSegments=count;}static int section(const char*,int){return 1;}
int main(){try{
 RBHostV1 fake{};fake.api_version=1;fake.struct_size=sizeof fake;fake.number=num;fake.string=str;fake.command=command;fake.text=text;fake.separator=text;fake.input_int=intValue;fake.input_float=floatValue;fake.checkbox=intValue;fake.combo=combo;fake.button=button;fake.same_line=+[]{};fake.disabled=disabled;fake.progress=+[](float,const char*){};fake.read_data=readData;fake.write_data=writeData;fake.preview_lines=previewLines;fake.import_map=apply;fake.replace_selection=replace;fake.section=section;fake.input_text=+[](const char*,char*,size_t){return 0;};
 const auto* plugin=ReyondBroomPluginInit(&fake);check(plugin!=nullptr,"plugin_initializes_with_extended_host");
 auto click=[&](const char* name){clicked=name;plugin->draw();check(clicked.empty(),name);check(disabledDepth==0,"balanced_disabled_controls");};
 auto wait=[&]{for(int i=0;plugin->busy()&&i<6000;i++){plugin->tick();std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!plugin->busy(),"background_job_completes");};
 click("Generate preview");wait();check(previewSegments>100,"live_wire_preview");click("Export MAP to plugin data");check(data["plugin-levelgen-export.map"].find("info_player_start")!=std::string::npos,"map_export_contains_player");
 click("Apply to map");check(imported.size()>1000&&previewSegments==0,"apply_imports_and_clears_preview");
 click("Capture selected brushes as module");wait();click("Save library / settings");check(data["plugin-levelgen-settings.txt"].find("Captured block")!=std::string::npos,"captured_module_persisted");
 click("Load library / settings");click("Generate preview");wait();click("Rebuild region");wait();check(previewSegments>100,"region_rebuild_produces_preview");
 plugin->shutdown();check(previewSegments==0,"shutdown_clears_preview");
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
