// SPDX-License-Identifier: GPL-2.0-or-later
#include <climits>
#include "../native/ftebroom_md3.h"
#include "../native/ftebroom_wad_import.h"
#include "../native/ftebroom_tga.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace ftebroom;
 std::string palette;for(int i=0;i<256;i++)for(int c=0;c<3;c++)palette+=char(i);
 quake1::Texture first{"first",16,16,std::vector<unsigned char>(256,64)},second{"second",32,16,std::vector<unsigned char>(512,128)};
 auto wad=addWadTexture({},first,palette);auto oldTable=quake1::word(wad,8);auto oldEntry=wad.substr(oldTable,32);auto oldPixels=wad.substr(12,oldTable-12);auto next=addWadTexture(wad,second,palette);
 assert(next.substr(12,oldPixels.size())==oldPixels);assert(next.substr(quake1::word(next,8),32)==oldEntry);auto textures=quake1::wad(next);assert(textures.size()==2&&textures[1].pixels==second.pixels);
 bool rejected=false;try{addWadTexture(next,second,palette);}catch(...){rejected=true;}assert(rejected);
 second.name="SECOND";rejected=false;try{addWadTexture(next,second,palette);}catch(...){rejected=true;}assert(rejected);
 auto put=[](std::string& b,size_t at,uint32_t n){for(int i=0;i<4;i++)b[at+i]=char(n>>(i*8));};
 std::string md3(108+236,0);md3.replace(0,4,"IDP3");put(md3,4,15);put(md3,84,1);put(md3,100,108);put(md3,104,md3.size());size_t s=108;md3.replace(s,4,"IDP3");md3.replace(s+4,4,"body");for(auto pair:{std::pair<int,int>{72,1},{76,1},{80,3},{84,1},{88,108},{92,120},{96,188},{100,212},{104,236}})put(md3,s+pair.first,pair.second);put(md3,s+112,1);put(md3,s+116,2);md3.replace(s+120,15,"models/test/red");md3[s+220]=64;md3[s+230]=64;
 auto surfaces=readMd3(md3);assert(surfaces.size()==1&&surfaces[0].triangles.size()==1&&surfaces[0].shader=="models/test/red");assert(surfaces[0].triangles[0][1].point[0]==1);
 auto animated=md3+md3.substr(s+212,24);put(animated,104,animated.size());put(animated,s+72,2);put(animated,s+104,260);
 animated[s+244]=char(128);
 assert(readMd3(animated,1)[0].triangles[0][1].point[0]==2);
 assert(readMd3(animated,500)[0].triangles[0][1].point[0]==2);
 for(size_t n=0;n<md3.size();n++){bool fail=false;try{readMd3(md3.substr(0,n));}catch(...){fail=true;}assert(fail);}
 put(md3,s+112,3);rejected=false;try{readMd3(md3);}catch(...){rejected=true;}assert(rejected);
 std::string tga(18,0);tga[2]=10;tga[12]=2;tga[14]=1;tga[16]=24;tga[17]=32;tga+=char(129);tga+=char(0);tga+=char(0);tga+=char(255);auto image=readTga(tga);assert(image.rgba[0]==255&&image.rgba[4]==255);tga.pop_back();rejected=false;try{readTga(tga);}catch(...){rejected=true;}assert(rejected);
 std::cout<<"PASS: WAD mip import, existing lump preservation, collision rejection, MD3 bounds/UV/indices and TGA RLE.\n";
}
