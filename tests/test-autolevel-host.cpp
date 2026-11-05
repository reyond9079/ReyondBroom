// SPDX-License-Identifier: MIT
#include "../sdk/reyondbroom_plugin.h"
#include "../native/ftebroom_plugin_projection.h"
#include <iostream>
#include <stdexcept>
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
 check(offsetof(RBHostV1,preview_colored_lines)==offsetof(RBHostV1,replace_selection)+sizeof(void*),"tail append preserves old ABI");
 check(offsetof(RBHostV1,number)==8,"v1 header ABI");
 rbviewport::Projection p;p.x=50;p.y=100;p.w=1000;p.h=600;p.origin={100,200,300};p.forward={0,1,0};p.right={1,0,0};p.up={0,0,1};p.fov=90;
 for(int type=0;type<4;type++){p.type=type;for(float x:{100.f,550.f,1000.f})for(float y:{150.f,400.f,650.f}){rbviewport::V start,ray;p.ray(x,y,start,ray);rbviewport::V point;for(int a=0;a<3;a++)point[a]=start[a]+ray[a]*(type?65536:1000);float px,py;check(p.project(point,px,py)&&std::abs(px-x)<.02f&&std::abs(py-y)<.02f,"project/unproject inverse in all views");}}
 std::cout<<"PASS host ABI tail offsets and perspective/orthographic projection round trips\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
