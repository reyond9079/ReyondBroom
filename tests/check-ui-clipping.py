"""Compare actual renderer output with the previous clipping algorithm."""
from pathlib import Path
import subprocess, tempfile, os
root = Path(__file__).resolve().parents[1]
source = (root/'native/ftebroom_imgui_draw.h').read_text()
vertex = source[source.index('struct Vertex'):source.index('// Clip triangles')]
clip = source[source.index('static void triangle('):source.index('static double imguiRenderMs')]
reference = (root/'tests/reference-imgui-clip.h').read_text()
fixture = r"""
#include <algorithm>
#include <array>
#include <vector>
#include <random>
#include <cstring>
#include <iostream>
#include <chrono>
using qhandle_t=int;
using vec2_t=float[2]; using vec4_t=float[4];
struct ImVec4 {float x,y,z,w;};
static std::vector<float> output;
struct Draw {
 void Image2dQuad(vec2_t *xy,vec2_t *uv,vec4_t *color,int) {
  for(int i=0;i<4;i++){output.insert(output.end(),xy[i],xy[i]+2);output.insert(output.end(),uv[i],uv[i]+2);output.insert(output.end(),color[i],color[i]+4);}
 }
} sink,*draw=&sink;
"""
main = r"""
int main() {
 std::mt19937 random(202635);std::uniform_real_distribution<float> coord(-150,150),unit(0,1);
 ImVec4 bounds{-50,-40,60,70};
 for(int n=0;n<100000;n++) {
  std::array<Vertex,8> p{};
  for(int i=0;i<3;i++)p[i]={coord(random),coord(random),unit(random),unit(random),unit(random),unit(random),unit(random),unit(random)};
  if(n%10==0)p[2]=p[1];
  if(n%11==0)for(int i=0;i<3;i++)p[i].x=bounds.x;
  output.clear();referenceTriangle({p[0],p[1],p[2]},bounds,1);auto expected=output;
  output.clear();triangle(p,bounds,1);
  if(expected!=output){std::cerr<<"Mismatch "<<n<<"\n";return 1;}
 }
 std::cout<<"PASS: 100000 triangles, exact clipped vertices/UV/colors including degenerate and boundary cases\n";
}
"""
with tempfile.TemporaryDirectory(prefix='ftebroom-clip-') as tmp:
 path=Path(tmp); (path/'test.cpp').write_text(fixture+vertex+reference+clip+main)
 env=dict(os.environ);env['PATH']='C:/msys64/mingw64/bin;'+env['PATH']
 subprocess.run(['C:/msys64/mingw64/bin/g++.exe','-std=c++17','-O2',str(path/'test.cpp'),'-o',str(path/'test.exe')],check=True,env=env)
 subprocess.run([str(path/'test.exe')],check=True,env=env)
