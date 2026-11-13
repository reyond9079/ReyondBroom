// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_browser_paths.h"
#include "../native/ftebroom_preview_raster.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace ftebroom;
int main(){
  assert(browser::typedPath(" \t\"C:/Map projects/Карты тест/e1m1.map\"  ")=="C:/Map projects/Карты тест/e1m1.map");
  assert(browser::typedPath("maps/test room.map \t")=="maps/test room.map");
  assert(browser::typedPath(" \t ").empty());
  auto parts=browser::ancestors(std::filesystem::u8path("/games/my mod/pak0.pk3/maps"));
  assert(parts.size()==5&&parts.front().path=="/"&&parts[3].path=="/games/my mod/pak0.pk3"&&parts.back().path=="/games/my mod/pak0.pk3/maps");
  auto relative=browser::ancestors(std::filesystem::u8path("models/trees/oak"));assert(relative.size()==3&&relative[1].path=="models/trees");
  assert(browser::zoom(72,1,48,180)==80&&browser::zoom(72,-1,48,180)==64);
  assert(browser::zoom(176,1,48,180)==180&&browser::zoom(48,-1,48,180)==48);
  assert(browser::zoom(72,std::numeric_limits<float>::infinity(),48,180)==72);
  preview::Part far;far.pixels={1,1,{255,0,0,255}};
  far.triangles={{{{{.1f,.1f,0},0,0},{{.9f,.1f,0},1,0},{{.5f,.9f,0},.5f,1}}}};
  auto near=far;near.pixels.rgba={0,255,0,255};for(auto& v:near.triangles[0])v.point[2]=1;
  auto tga=preview::raster({near,far},32);size_t middle=18+(16*32+16)*4;
  assert(tga.size()==18+32*32*4&&(unsigned char)tga[middle+1]==255&&(unsigned char)tga[middle+2]==0&&(unsigned char)tga[middle+3]==255);
  assert(tga[18+3]==0);near.pixels.rgba[3]=0;tga=preview::raster({near,far},32);assert((unsigned char)tga[middle+2]==255);
  near.triangles[0][0].point[0]=std::numeric_limits<float>::quiet_NaN();assert(preview::raster({near},32)[middle+3]==0);
  // Benchmark a varied 10k-triangle model once; cached UI work is a single quad.
  preview::Part large=far;large.triangles.clear();for(int i=0;i<10000;i++){auto tri=far.triangles[0];float x=(i%100)*.009f,y=(i/100)*.009f;for(auto& v:tri){v.point[0]=v.point[0]*.01f+x;v.point[1]=v.point[1]*.01f+y;}large.triangles.push_back(tri);}
  auto start=std::chrono::steady_clock::now();auto cached=preview::raster({large});auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  assert(cached.size()==18+192*192*4);std::cout<<"PASS: breadcrumb disk/archive ancestors, zoom bounds, textured raster depth/alpha/invalid data. 10k-triangle raster once="<<ms<<"ms; steady visible model: 10000 triangles -> 2 triangles (quad).\n";
}
