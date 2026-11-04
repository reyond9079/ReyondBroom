// SPDX-License-Identifier: MIT
#include "reyondbroom_plugin.h"
#include "../common/geometry.h"
#include <random>
#include <set>
#include <map>
using namespace rbgen;
static const RBHostV1* host;
static int seed=1337,rooms=12,loops=2,ceiling=1,lights=1,player=1;
static float width=384,height=192,hall=128,gap=192,origin[3]={0,0,0};
static std::string preview,message;static int brushCount=0,connectionCount=0;static std::vector<float> previewLines;
static std::string previewKey;
static std::string parameters(){std::ostringstream s;s<<seed<<' '<<rooms<<' '<<loops<<' '<<width<<' '<<height<<' '<<hall<<' '<<gap<<' '<<ceiling<<lights<<player;for(float v:origin)s<<' '<<v;char tex[512]{};host->string("ca_newbrushtexture",tex,sizeof tex);s<<' '<<tex<<' '<<host->number("rb.document_generation");return s.str();}
struct Cell{int x,y;bool open[4]={};};
static std::string generate(){
 for(float v:{width,height,hall,gap,origin[0],origin[1],origin[2]})if(!std::isfinite(v))throw std::runtime_error("Use finite coordinates and dimensions.");
 if(rooms<2||rooms>64||loops<0||loops>64||width<256||width>2048||height<96||height>1024||hall<64||hall>width-64||gap<64||gap>1024)throw std::runtime_error("Use 2..64 rooms, room width 256..2048, height 96..1024, corridor width 64..(room width - 64), gap 64..1024.");
 for(float v:origin)if(std::abs(v)>24000)throw std::runtime_error("Keep the origin within +/-24000 units.");
 std::mt19937 rng(seed);std::vector<Cell> cells{{0,0}};std::map<std::pair<int,int>,int> index{{{0,0},0}};
 const int dx[]={1,0,-1,0},dy[]={0,1,0,-1};std::vector<std::array<int,3>> edges;
 while(int(cells.size())<rooms){int a=int(rng()%cells.size()),side=int(rng()%4);int x=cells[a].x+dx[side],y=cells[a].y+dy[side];if(std::abs(x)>6||std::abs(y)>6||index.count({x,y}))continue;int b=int(cells.size());index[{x,y}]=b;cells.push_back({x,y});cells[a].open[side]=cells[b].open[(side+2)%4]=true;edges.push_back({a,b,side});}
 for(int trial=0,added=0;trial<rooms*8&&added<loops;trial++){int a=int(rng()%cells.size()),side=int(rng()%4);auto it=index.find({cells[a].x+dx[side],cells[a].y+dy[side]});if(it==index.end()||cells[a].open[side])continue;cells[a].open[side]=cells[it->second].open[(side+2)%4]=true;edges.push_back({a,it->second,side});added++;}
 char tex[512]={};host->string("ca_newbrushtexture",tex,sizeof tex);std::string mat=material(tex);std::ostringstream out;out<<"{\n\"classname\" \"worldspawn\"\n";brushCount=0;
 previewLines.clear();V base{origin[0],origin[1],origin[2]};double half=width*.5,t=16,door=std::min(double(height)-16,160.);
 auto add=[&](V lo,V hi){if(++brushCount>2000)throw std::runtime_error("The layout exceeds 2000 brushes.");lo=lo+base;hi=hi+base;for(int a=0;a<3;a++)if(std::abs(lo[a])>32000||std::abs(hi[a])>32000)throw std::runtime_error("Layout exceeds map coordinate bounds.");out<<box(lo,hi,mat);for(int axis=0;axis<3;axis++)for(int side=0;side<4;side++){V a=lo,b=lo;int u=(axis+1)%3,v=(axis+2)%3;a[u]=b[u]=side&1?hi[u]:lo[u];a[v]=b[v]=side&2?hi[v]:lo[v];b[axis]=hi[axis];if(previewLines.size()<16384*6)for(V p:{a,b}){previewLines.push_back(float(p.x));previewLines.push_back(float(p.y));previewLines.push_back(float(p.z));}}};
 auto center=[&](Cell c){return V{c.x*(width+gap),c.y*(width+gap),0};};
 for(auto c:cells){V p=center(c);add(p+V{-half-t,-half-t,-t},p+V{half+t,half+t,0});if(ceiling)add(p+V{-half-t,-half-t,height},p+V{half+t,half+t,height+t});
  for(int s=0;s<4;s++){
   int a=s%2,b=1-a;double sign=s<2?1:-1;V lo=p,hi=p;lo[a]+=sign>0?half:-half-t;hi[a]=lo[a]+t;lo[b]-=half+t;hi[b]+=half+t;lo.z=0;hi.z=height;
   if(!c.open[s])add(lo,hi);else{V left=hi;left[b]=p[b]-hall*.5;add(lo,left);V right=lo;right[b]=p[b]+hall*.5;add(right,hi);lo[b]=p[b]-hall*.5;hi[b]=p[b]+hall*.5;lo.z=door;add(lo,hi);}
  }
 }
 for(auto e:edges){V a=center(cells[e[0]]),b=center(cells[e[1]]);int axis=e[2]%2,other=1-axis;V lo=a,hi=b;if(lo[axis]>hi[axis])std::swap(lo,hi);lo[axis]+=half;hi[axis]-=half;lo[other]=a[other]-hall*.5;hi[other]=a[other]+hall*.5;lo.z=-t;hi.z=0;add(lo,hi);if(ceiling){lo.z=door;hi.z=door+t;add(lo,hi);}for(int side:{-1,1}){V l=lo,h=hi;l.z=0;h.z=door+t;l[other]=a[other]+side*hall*.5-(side<0?t:0);h[other]=l[other]+t;add(l,h);}}
 out<<"}\n";
 if(player)out<<"{\n\"classname\" \""<<(host->number("rb.profile")==0?"info_player_deathmatch":"info_player_start")<<"\"\n\"origin\" \""<<base.x<<' '<<base.y<<' '<<base.z+48<<"\"\n}\n";
 if(lights)for(auto c:cells){V p=center(c)+base;out<<"{\n\"classname\" \"light\"\n\"origin\" \""<<p.x<<' '<<p.y<<' '<<p.z+height-32<<"\"\n\"light\" \"350\"\n}\n";}
 connectionCount=int(edges.size());return out.str();
}
static void draw(){host->text("Generates a connected layout at the chosen origin. Existing map geometry is retained. New objects are selected; Undo removes the complete import.");
 if(!preview.empty()&&previewKey!=parameters()){preview.clear();host->preview_lines(nullptr,0);message="Settings or document changed. Generate the preview again.";}
 if(host->button("Generate preview"))try{preview=generate();previewKey=parameters();host->preview_lines(previewLines.data(),previewLines.size()/6);message="Ready: "+std::to_string(rooms)+" connected rooms, "+std::to_string(connectionCount)+" corridors, "+std::to_string(brushCount)+" brushes. Apply to insert.";}catch(const std::exception& e){preview.clear();host->preview_lines(nullptr,0);message=e.what();}
 host->same_line();bool cannotApply=preview.empty();if(cannotApply)host->disabled(1);if(host->button("Apply to map")){message=host->import_map(preview.data(),preview.size())?"Import queued. All generated objects will be selected.":"Cannot import now. Return to MAP editing first.";preview.clear();host->preview_lines(nullptr,0);}if(cannotApply)host->disabled(0);
 host->input_int("Seed",&seed);host->input_int("Rooms",&rooms);host->input_int("Extra connections",&loops);host->input_float("Room width",&width);host->input_float("Room height",&height);host->input_float("Corridor width",&hall);host->input_float("Corridor length",&gap);
 for(int a=0;a<3;a++)host->input_float(a==0?"Origin X":a==1?"Origin Y":"Origin Z",&origin[a]);
 host->checkbox("Ceilings / sealed rooms",&ceiling);host->checkbox("Add lights",&lights);host->checkbox("Add player start",&player);
 if(!message.empty())host->text(message.c_str());
}
extern "C" RB_PLUGIN_EXPORT const RBPluginV1* ReyondBroomPluginInit(const RBHostV1* api){if(!api||api->api_version!=1||api->struct_size<sizeof(RBHostV1))return nullptr;host=api;static RBPluginV1 plugin{1,sizeof(RBPluginV1),"org.reyondbroom.levelgen",draw,nullptr,nullptr,nullptr};return &plugin;}
