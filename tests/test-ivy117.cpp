// SPDX-License-Identifier: MIT
#include "../plugins/ivy/coverage.h"
#include "../plugins/ivy/stems.h"
#include "../plugins/ivy/model.h"
#include <iostream>
using namespace ivy;
void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);std::cout<<"PASS "<<name<<'\n';}
Growth wall(double size=256){Growth g;double x=size*.5;g.surface.triangles={{{-x,0,0},{x,0,0},{x,0,size},{0,-1,0},1},{{-x,0,0},{x,0,size},{-x,0,size},{0,-1,0},1}};g.surface.build();return g;}
int main(){try{
 Settings s;s.maxNodes=6000;s.offset=2;s.step=8;auto g=wall();auto spacing=cover(g,s,18);check(g.nodes.size()>300&&g.nodes.size()<6000,"face_coverage_bounded");
 int roots=0;double low=1e30,high=-1e30;for(size_t i=0;i<g.nodes.size();i++){auto n=g.nodes[i];if(n.parent<0)roots++;else check(n.parent<int(i),"parent_order");low=std::min(low,n.p.z);high=std::max(high,n.p.z);if(std::abs(n.p.y+2)>1e-6||std::abs(n.p.x)>128.001)throw std::runtime_error("coverage escaped selected face");}
 check(roots==1&&low<.01&&high>255.99,"connected_top_to_bottom_coverage");
 for(int z=12;z<256;z+=24)for(int x=-120;x<=120;x+=24){double nearest=1e30;for(auto n:g.nodes)nearest=std::min(nearest,length(n.p-V{double(x),-2,double(z)}));if(nearest>spacing*1.3)throw std::runtime_error("uncovered face region");}check(true,"whole_face_coverage");
 auto frames=stemFrames(g.nodes,1.2);Model m;size_t rawVertices=0;for(size_t i=1;i<g.nodes.size();i++){int p=g.nodes[i].parent;if(p<0)continue;auto a=stem(g.nodes,frames,i);m.append(a,false,g.nodes[p].parent<0,frames[i].children==0,frames[p].distance/40,frames[i].distance/40);rawVertices+=a.size();
  if(p>0){auto b=stem(g.nodes,frames,p);for(int k=0;k<6;k++)if(length(a[k]-b[k+6])>1e-8)throw std::runtime_error("joint gap");}}
 m.weldStems();check(m.vertices()<rawVertices*.8,"shared_stem_vertices_welded");
 for(auto& surface:m.surfaces)for(auto t:surface.triangles)if(length(cross(surface.vertices[t[1]].p-surface.vertices[t[0]].p,surface.vertices[t[2]].p-surface.vertices[t[0]].p))<1e-8)throw std::runtime_error("degenerate tube triangle");check(true,"no_degenerate_stem_triangles");
 auto huge=wall(8192);cover(huge,s,18);check(huge.nodes.size()<=6000,"large_face_adapts_without_truncation");double h=0;for(auto n:huge.nodes)h=std::max(h,n.p.z);check(h>8191,"large_face_reaches_top");
 auto hanging=wall(512);s.up=-.175;s.gravity=.4;s.iterations=100;s.maxNodes=1800;hanging.start({0,-10,480},s);while(hanging.advance()){}low=480;for(auto n:hanging.nodes)low=std::min(low,n.p.z);check(low<280,"natural_hanging_growth_descends");check(std::abs(hanging.nodes[0].p.z-480)<.001,"explicit_root_position_preserved");
 auto horizontal=wall();for(auto& t:horizontal.surface.triangles){for(auto p:{&t.a,&t.b,&t.c})std::swap(p->y,p->z);t.normal={0,0,1};}horizontal.surface.build();cover(horizontal,s,18);check(horizontal.nodes.size()>100,"horizontal_face_coverage");
 auto texture=ivy::texture(true);double saturation=0;for(size_t i=18;i<texture.size();i+=4){double a=(unsigned char)texture[i],b=(unsigned char)texture[i+1],c=(unsigned char)texture[i+2];double hi=std::max({a,b,c}),lo=std::min({a,b,c});saturation+=(hi-lo)/std::max(1.,hi);}saturation/=256*256;check(saturation<.55,"muted_default_leaf_palette");
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
