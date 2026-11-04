// SPDX-License-Identifier: MIT
#pragma once
#include "wfc.h"
#include "../common/geometry.h"
#include <iomanip>
namespace rbwfc {
inline std::vector<Block> defaults(){std::vector<Block> out;auto add=[&](const char* name,int mask,int turns,double weight,int group=0,int kind=0){Block b;b.name=name;b.rotations=turns;b.weight=weight;b.group=group;b.kind=kind;for(int d=0;d<4;d++)b.sockets[d]=(mask>>d)&1;out.push_back(b);};add("Cross room",15,0,.6);add("T junction",7,1,.7);add("Corner room",3,1,1);add("Straight corridor",5,1,1);add("End room",1,1,.7);add("Closed room",0,0,.15);add("Solid",0,0,1,1,1);add("Empty",0,0,1,2,2);return out;}
inline rbgen::V rotate(rbgen::V p,int turn){switch(turn%4){case 1:return {-p.y,p.x,p.z};case 2:return {-p.x,-p.y,p.z};case 3:return {p.y,-p.x,p.z};default:return p;}}
inline std::string transformBrushes(const std::string& source,rbgen::V low,rbgen::V extent,rbgen::V anchor,int turn){
 using namespace rbgen;if(source.find("patchDef")!=source.npos)throw std::runtime_error("Capture brush modules; patches are not supported by module capture.");
 V offset=turn==1?V{extent.y,0,0}:turn==2?V{extent.x,extent.y,0}:turn==3?V{0,extent.x,0}:V{};V translation=anchor+offset-rotate(low,turn);
 std::istringstream input(source);std::ostringstream output;std::string line;int depth=0;
 while(std::getline(input,line)){
  auto first=line.find_first_not_of(" \t\r");if(first==line.npos)continue;char c=line[first];
  if(c=='{'){depth++;if(depth>=2)output<<line<<'\n';continue;}if(c=='}'){if(depth>=2)output<<line<<'\n';depth--;continue;}if(depth<2)continue;
  if(c!='(')throw std::runtime_error("Unsupported module geometry.");std::istringstream in(line);char bracket;V points[3];for(auto& p:points){if(!(in>>bracket>>p.x>>p.y>>p.z)||bracket!='('||!(in>>bracket)||bracket!=')')throw std::runtime_error("Invalid captured brush plane.");p=rotate(p,turn)+translation;output<<point(p);}
  std::string tail;std::getline(in,tail);auto at=tail.find('[');
  if(at!=tail.npos){
   V axes[2];double bias[2];size_t start=at;
   for(int axis=0;axis<2;axis++){
    auto end=tail.find(']',start);if(end==tail.npos)throw std::runtime_error("Invalid captured UV axes.");
    std::istringstream uv(tail.substr(start+1,end-start-1));
    if(!(uv>>axes[axis].x>>axes[axis].y>>axes[axis].z>>bias[axis]))throw std::runtime_error("Invalid captured UV coordinates.");
    axes[axis]=rotate(axes[axis],turn);start=end+1;
    if(axis==0){start=tail.find('[',start);if(start==tail.npos)throw std::runtime_error("Missing captured UV axis.");}
   }
   double angle,scale[2];std::istringstream suffix(tail.substr(start));
   if(!(suffix>>angle>>scale[0]>>scale[1])||!std::isfinite(scale[0])||!std::isfinite(scale[1])||std::abs(scale[0])<1e-9||std::abs(scale[1])<1e-9)throw std::runtime_error("Invalid captured UV scale.");
   output<<tail.substr(0,at);
   for(int axis=0;axis<2;axis++){auto v=axes[axis];bias[axis]-=dot(v,translation)/scale[axis];output<<" [ "<<v.x<<' '<<v.y<<' '<<v.z<<' '<<bias[axis]<<" ] ";}
   output<<tail.substr(start)<<'\n';
  }
  else output<<tail<<'\n';
 }if(depth)throw std::runtime_error("Unclosed captured module.");return output.str();
}
struct Build {std::string map;std::vector<float> lines;int brushes=0,modules=0;};
inline Build build(const Config& config,const std::vector<Block>& blocks,const std::vector<State>& states,const Result& result,rbgen::V base,double cell,double height,double doorway,bool ceilings,bool lights,bool player,int profile,const std::string& material){
 using namespace rbgen;Build out;std::ostringstream map,entities;map<<"{\n\"classname\" \"worldspawn\"\n";bool startAdded=false;
 auto add=[&](V lo,V hi){if(++out.brushes>16000)throw std::runtime_error("The level exceeds 16000 brushes. Reduce the grid.");for(int a=0;a<3;a++)if(std::abs(lo[a])>32000||std::abs(hi[a])>32000)throw std::runtime_error("Generated level exceeds map coordinate bounds.");map<<box(lo,hi,material);};
 auto wire=[&](V lo,V hi){if(out.lines.size()+72>16384*6)return;for(int axis=0;axis<3;axis++)for(int side=0;side<4;side++){V a=lo,b=lo;int u=(axis+1)%3,v=(axis+2)%3;a[u]=b[u]=side&1?hi[u]:lo[u];a[v]=b[v]=side&2?hi[v]:lo[v];b[axis]=hi[axis];for(auto p:{a,b})for(int k=0;k<3;k++)out.lines.push_back(p[k]);}};
 for(size_t i=0;i<result.cells.size();i++){
  auto state=states[result.cells[i]];if(state.part!=I{0,0,0})continue;auto block=blocks[state.block];int x=i%config.size[0],y=(i/config.size[0])%config.size[1],z=i/(config.size[0]*config.size[1]);V p=base+V{x*cell,y*cell,z*height},size{state.size[0]*cell,state.size[1]*cell,state.size[2]*height};if(block.kind==2)continue;out.modules++;wire(p,p+size);
  if(!block.source.empty()){std::istringstream source(block.source);std::string marker;V lo,hi;source>>marker>>lo.x>>lo.y>>lo.z>>hi.x>>hi.y>>hi.z;std::string rest((std::istreambuf_iterator<char>(source)),{});V extent{block.size[0]*cell,block.size[1]*cell,block.size[2]*height};if(marker!="BOUNDS"||hi.x-lo.x>extent.x+.01||hi.y-lo.y>extent.y+.01||hi.z-lo.z>extent.z+.01)throw std::runtime_error("Captured geometry does not fit its block footprint: "+block.name);auto bytes=transformBrushes(rest,lo,extent,p,state.rotation);map<<bytes;out.brushes+=int(std::count(bytes.begin(),bytes.end(),'{'));}
  else if(block.kind==1)add(p,p+size);
  else{
   const double t=16,door=std::min(height-32.,160.);add(p,p+V{cell,cell,t});if(ceilings)add(p+V{0,0,height-t},p+V{cell,cell,height});
   for(int d=0;d<4;d++){int a=d%2,b=1-a;V lo=p,hi=p+V{cell,cell,height};lo[a]+=d<2?cell-t:0;hi[a]=lo[a]+t;
    if(!state.sockets[d])add(lo,hi);else{V left=hi;left[b]=p[b]+(cell-doorway)*.5;add(lo,left);V right=lo;right[b]=p[b]+(cell+doorway)*.5;add(right,hi);lo[b]=left[b];hi[b]=right[b];lo.z=p.z+t+door;if(lo.z<hi.z-.01)add(lo,hi);}
   }
  }
  if(out.brushes>16000||map.tellp()>16*1024*1024)throw std::runtime_error("Generated layout exceeds the brush/MAP budget.");
  if(block.group==0){if(player&&!startAdded){entities<<"{\n\"classname\" \""<<(profile==0?"info_player_deathmatch":"info_player_start")<<"\"\n\"origin\" \""<<p.x+cell*.5<<' '<<p.y+cell*.5<<' '<<p.z+48<<"\"\n}\n";startAdded=true;}if(lights)entities<<"{\n\"classname\" \"light\"\n\"origin\" \""<<p.x+cell*.5<<' '<<p.y+cell*.5<<' '<<p.z+height-40<<"\"\n\"light\" \"300\"\n}\n";}
 }map<<"}\n"<<entities.str();out.map=map.str();if(out.map.size()>16*1024*1024)throw std::runtime_error("Generated MAP exceeds the 16 MiB import budget.");return out;
}
}
