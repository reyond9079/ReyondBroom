// SPDX-License-Identifier: GPL-2.0-or-later
#include "imgui.h"
#include "imgui_internal.h"
#include "../native/ftebroom_entity_defs.h"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <map>
#include <thread>
// The production ImGui build calls this hook while drawing labels.
void FteBroomTranslate(const char*&,const char*&){}
using qhandle_t=int;
static std::string modelBytes,imageBytes;
static int reads=0,uploads=0,unloads=0;
struct MockFiles {void* LoadFile(const char* name,size_t* size){++reads;std::string data;auto path=std::string(name);if(path.find("bad/")==0)return nullptr;if(path.size()>=4&&path.substr(path.size()-4)==".md3")data=modelBytes;else if((path=="models/test/red.tga"||path=="browser132/models/skin.tga"))data=imageBytes;else return nullptr;*size=data.size();auto out=std::malloc(data.size());std::memcpy(out,data.data(),data.size());return out;}} mockFiles;
struct MockCore {void Free(void* ptr){std::free(ptr);}} mockCore;
struct MockDraw {int LoadImageData(const char*,const char*,void*,size_t){return ++uploads;}int LoadImage(const char* name){static std::map<std::string,int> handles;static int next=1;auto found=handles.find(name);if(found!=handles.end())return found->second;return handles[name]=next++;}int LoadImageShader(const char*,const char*){return LoadImage("");}void UnloadImage(int){++unloads;}} mockDraw;
static auto files=&mockFiles;static auto core=&mockCore;static auto draw=&mockDraw;
static std::string q1Palette(){return std::string(768,127);}
static unsigned entityDefinitionEpoch=1;
struct Material {std::string preview;};static std::map<std::string,Material> materials;
#include "../native/ftebroom_entity_preview.h"
#include "../native/ftebroom_browser_navigation.h"
#ifdef RB_PREVIEW_LEGACY_HEADER
// Compile this optional comparative benchmark against the exact preceding
// release's header (`git show <base>:native/ftebroom_entity_preview.h`), rather
// than a hand-written approximation of its triangle submission path.
namespace legacyPreview {
#include RB_PREVIEW_LEGACY_HEADER
}
#endif
static void begin(){auto& io=ImGui::GetIO();io.DisplaySize={640,480};io.DeltaTime=1.f/60;ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({640,480});ImGui::Begin("test",nullptr,ImGuiWindowFlags_NoSavedSettings);}
static void end(){ImGui::End();ImGui::Render();}
int main(int argc,char** argv){
 auto put=[](std::string& b,size_t at,uint32_t n){for(int i=0;i<4;i++)b[at+i]=char(n>>(i*8));};
 modelBytes.assign(108+236,0);auto& b=modelBytes;b.replace(0,4,"IDP3");put(b,4,15);put(b,84,1);put(b,100,108);put(b,104,b.size());size_t s=108;b.replace(s,4,"IDP3");b.replace(s+4,4,"body");for(auto pair:{std::pair<int,int>{72,1},{76,1},{80,3},{84,1},{88,108},{92,120},{96,188},{100,212},{104,236}})put(b,s+pair.first,pair.second);put(b,s+112,1);put(b,s+116,2);b.replace(s+120,15,"models/test/red");b[s+220]=64;b[s+230]=64;
 imageBytes.assign(18+4,0);imageBytes[2]=2;imageBytes[12]=imageBytes[14]=1;imageBytes[16]=32;imageBytes[17]=40;imageBytes[20]=char(255);imageBytes[21]=char(255);
 if(argc>1){std::ifstream model(argv[1],std::ios::binary);assert(model);modelBytes.assign(std::istreambuf_iterator<char>(model),{});}
 ImGui::CreateContext();ImGui::GetIO().BackendFlags|=ImGuiBackendFlags_RendererHasVtxOffset;ImGui::GetIO().IniFilename=nullptr;unsigned char* pixels;int w,h;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 ftebroom::EntityDefinition a;a.model="models/a.md3";ftebroom::EntityDefinition other=a;other.model="models/b.md3";
 begin();int before=reads;assert(entityModelTile(a,{10000,10000},72));assert(reads==before&&entityPreviews.empty());
 entityModelTile(a,{40,40},72);assert(entityPreviews.size()==1);entityModelTile(other,{130,40},72);assert(entityPreviews.size()==1);end();
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 while(!entityPreviews[a.model+":"].thumbnail&&std::chrono::steady_clock::now()<deadline){std::this_thread::yield();begin();entityModelTile(a,{40,40},72);end();}
 assert(entityPreviews[a.model+":"].thumbnail);int coldReads=reads,coldUploads=uploads;
 begin();auto list=ImGui::GetWindowDrawList();auto vertices=list->VtxBuffer.Size,indices=list->IdxBuffer.Size;assert(entityModelTile(a,{40,40},72));assert(list->VtxBuffer.Size-vertices==4&&list->IdxBuffer.Size-indices==6);assert(reads==coldReads&&uploads==coldUploads);end();
#ifdef RB_PREVIEW_LEGACY_HEADER
 std::vector<ftebroom::EntityDefinition> probes;for(int i=0;i<9;i++){auto d=a;d.model="models/probe"+std::to_string(i)+".md3";probes.push_back(d);}
 begin();for(int i=0;i<9;i++)legacyPreview::entityModelTile(probes[i],{float(40+i%3*90),float(40+i/3*100)},72);end();
 deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 while(std::chrono::steady_clock::now()<deadline){bool ready=true;begin();for(int i=0;i<9;i++){entityModelTile(probes[i],{float(40+i%3*90),float(40+i/3*100)},72);auto it=entityPreviews.find(probes[i].model+":");ready&=it!=entityPreviews.end()&&it->second.thumbnail;}end();if(ready)break;std::this_thread::yield();}
 std::vector<double> oldTimes,newTimes;int oldVertices=0,newVertices=0;
 for(int pair=0;pair<100;pair++)for(int method=0;method<2;method++){
  begin();auto drawList=ImGui::GetWindowDrawList();int first=drawList->VtxBuffer.Size;auto start=std::chrono::steady_clock::now();
  for(int i=0;i<9;i++){auto tile=ImVec2(float(40+i%3*90),float(40+i/3*100));bool okay=method?entityModelTile(probes[i],tile,72):legacyPreview::entityModelTile(probes[i],tile,72);assert(okay);}
  auto micros=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
  (method?newTimes:oldTimes).push_back(micros);(method?newVertices:oldVertices)=drawList->VtxBuffer.Size-first;end();
 }
 std::sort(oldTimes.begin(),oldTimes.end());std::sort(newTimes.begin(),newTimes.end());auto oldMedian=oldTimes[50],newMedian=newTimes[50];
 assert(newVertices==36&&oldVertices>newVertices&&newMedian<oldMedian*.5);
 std::cout<<"BENCHMARK {\"paired_samples\":100,\"visible_models\":9,\"legacy_median_submit_us\":"<<oldMedian<<",\"raster_median_submit_us\":"<<newMedian<<",\"legacy_vertices\":"<<oldVertices<<",\"raster_vertices\":"<<newVertices<<",\"speedup\":"<<oldMedian/newMedian<<"}\n";
 legacyPreview::clearEntityPreviews();
#endif
 for(int i=0;i<140;i++){begin();ftebroom::EntityDefinition bad;bad.model="bad/"+std::to_string(i)+".md3";entityModelTile(bad,{40,40},72);end();}assert(entityPreviews.size()==128&&unloads>=2);
 begin();BrowserNavigationState navigation;auto chosen=browserNavigation("test-path",navigation,"/games/a mod/pak0.pk3/maps");assert(!chosen);end();
 clearEntityPreviews();assert(entityPreviews.empty());ImGui::DestroyContext();std::cout<<"PASS: offscreen zero resource reads, one generation/frame, async raster upload, warm model exactly 4 vertices/6 indices and no reads/uploads, 128-entry LRU eviction, shared breadcrumb UI.\n";
}
