# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the actual patched indexed-UI C entry point with a captured backend.

The capture harness is C++; -fpermissive permits the original C entry point's
implicit void-pointer conversions without rewriting the implementation tested.
"""
import argparse, pathlib, subprocess, tempfile, os
p=argparse.ArgumentParser();p.add_argument('engine',type=pathlib.Path);p.add_argument('--compiler',default=os.environ.get('CXX','g++'));a=p.parse_args()
s=(a.engine/'engine/client/r_2d.c').read_text();code=s[s.index('static int rb_ui_draw('):s.index('void *RB_IndexedUIInterface')]
fixture=r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
using std::max;using std::isfinite;
using vecV_t=float[3];using vec2_t=float[2];using avec4_t=float[4];using index_t=unsigned short;
struct shader_t{};shader_t one,*r_shaders[]={&one};int r_numshaders=1,qrenderer=1;const int QR_NONE=0;
struct{float fbvwidth=1600,fbvheight=1000;}vid;
struct mesh_t{unsigned numvertexes,numindexes;vecV_t*xyz_array;vec2_t*st_array;avec4_t*colors4f_array[1];index_t*indexes;};
struct srect_t{float x,y,width,height,dmin,dmax;};srect_t clipped;int scissors=0,draws=0;unsigned int r2d_be_flags=0;void(*R2D_Flush)()=nullptr;
void*BZ_Realloc(void*p,size_t n){return std::realloc(p,n);}
void BE_Scissor(srect_t*r){if(r){clipped=*r;scissors++;}}
void BE_DrawMesh_Single(shader_t*,mesh_t*m,void*,unsigned){draws++;assert(m->numvertexes==3&&m->numindexes==3);assert(m->indexes[0]==0&&m->indexes[1]==2&&m->indexes[2]==1);assert(m->xyz_array[2][0]==10&&m->st_array[2][1]==.75f);assert(std::abs(m->colors4f_array[1-1][2][0]-128/255.f)<1e-6);}
'''
main=r'''
struct V{float p[2],uv[2];unsigned char rgba[4];};
int main(){static_assert(sizeof(V)==20);V v[]={{{0,0},{0,0},{255,0,0,255}},{{0,10},{0,1},{0,255,0,255}},{{10,0},{1,.75f},{128,64,255,80}}};unsigned short ix[]={0,1,2};float clip[]={100,200,900,700};
assert(!rb_ui_draw(nullptr,0,nullptr,0,2,1,clip));assert(!rb_ui_draw(v,65537,ix,3,2,1,clip));assert(!rb_ui_draw(v,3,ix,3000003,2,1,clip));assert(!rb_ui_draw(v,3,ix,3,1,1,clip));assert(!rb_ui_draw(v,3,ix,3,2,2,clip));assert(draws==0);
unsigned short bad[]={0,3,1};assert(!rb_ui_draw(v,3,bad,3,2,1,clip));assert(draws==0);
assert(rb_ui_draw(v,3,ix,3,2,1,clip));assert(draws==1&&scissors==1);assert(std::abs(clipped.x-.0625f)<1e-6&&std::abs(clipped.y-.3f)<1e-6&&std::abs(clipped.width-.5f)<1e-6&&std::abs(clipped.height-.5f)<1e-6);
unsigned ix32[]={0,1,2};assert(rb_ui_draw(v,3,ix32,3,4,1,clip));assert(draws==2);
float empty[]={10,10,0,0};assert(rb_ui_draw(v,3,ix,3,2,1,empty)&&draws==2);
clip[0]=NAN;assert(!rb_ui_draw(v,3,ix,3,2,1,clip)&&draws==2);clip[0]=100;v[1].p[0]=INFINITY;assert(!rb_ui_draw(v,3,ix,3,2,1,clip)&&draws==2);
std::cout<<"PASS indexed UI: bounds/NaNs, 16/32-bit indices, winding, UV/colors, clip rectangle and empty clips\n";}
'''
with tempfile.TemporaryDirectory(prefix='reyond-indexed-ui-') as folder:
    tmp=pathlib.Path(folder);source=tmp/'test.cpp';source.write_text(fixture+code+main);exe=tmp/('test.exe' if os.name=='nt' else 'test')
    subprocess.run([a.compiler,'-std=c++17','-fpermissive','-O2',str(source),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
