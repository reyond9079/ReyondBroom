// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_quake1.h"
#include <cstring>
namespace ftebroom {
struct SpritePreview {int width=0,height=0,originX=0,originY=0;bool additive=false;std::string image,tga;};
// Read one static thumbnail frame. Animated/oriented viewport sprites use FTE's
// renderer. No model or game code is executed by the thumbnail reader.
inline SpritePreview readSprite(const std::string& bytes,const std::string& palette,int selectedFrame=0){
 using namespace quake1;range(bytes,0,12);SpritePreview result;
 auto countCheck=[](uint32_t n){if(!n||n>4096)throw std::runtime_error("Invalid sprite frame count.");};
 auto dimensions=[&](size_t at){auto w=word(bytes,at),h=word(bytes,at+4);if(!w||!h||w>4096||h>4096||uint64_t(w)*h>4194304)throw std::runtime_error("Sprite dimensions exceed the thumbnail budget.");return std::array<uint32_t,2>{w,h};};
 if(bytes.compare(0,4,"IDS2")==0){
  if(word(bytes,4)!=2)throw std::runtime_error("Unsupported SP2 version.");auto count=word(bytes,8);countCheck(count);range(bytes,12,uint64_t(count)*80);
  size_t at=12+size_t(std::clamp(selectedFrame,0,int(count)-1))*80;auto size=dimensions(at);result.width=size[0];result.height=size[1];result.originX=int32_t(word(bytes,at+8));result.originY=int32_t(word(bytes,at+12));result.image=name(bytes,at+16,64);if(result.image.empty()||result.image.find("..")!=result.image.npos||result.image.find_first_of(":\\\r\n\"")!=result.image.npos||result.image[0]=='/')throw std::runtime_error("Invalid SP2 image path.");return result;
 }
 if(bytes.compare(0,4,"IDSP"))throw std::runtime_error("Not a sprite.");
 unsigned version=word(bytes,4);if(version!=1&&version!=2&&version!=32)throw std::runtime_error("Unsupported SPR version.");
 size_t at=version==2?40:36;range(bytes,0,at);unsigned mode=version==2?word(bytes,12):3;auto count=word(bytes,version==2?28:24);countCheck(count);selectedFrame=std::clamp(selectedFrame,0,int(count)-1);
 std::string colors=palette;if(version==2){range(bytes,at,770);if((unsigned char)bytes[at]!=0||(unsigned char)bytes[at+1]!=1||mode>3)throw std::runtime_error("Invalid GoldSrc sprite palette/mode.");colors=bytes.substr(at+2,768);at+=770;}
 if(version!=32&&colors.size()!=768)throw std::runtime_error("Sprite palette is missing.");
 uint64_t totalPixels=0;int totalFrames=0;size_t chosen=0;unsigned width=0,height=0;
 for(unsigned frame=0;frame<count;frame++){
  auto group=word(bytes,at);at+=4;unsigned frames=1;
  if(group==1){frames=word(bytes,at);at+=4;countCheck(frames);range(bytes,at,uint64_t(frames)*4);float previous=0;for(unsigned i=0;i<frames;i++){auto bits=word(bytes,at+4*i);float duration;std::memcpy(&duration,&bits,4);if(!std::isfinite(duration)||duration<=previous)throw std::runtime_error("Invalid sprite group interval.");previous=duration;}at+=size_t(frames)*4;}
  else if(group!=0)throw std::runtime_error("Unsupported sprite frame type.");
  for(unsigned sub=0;sub<frames;sub++){
   range(bytes,at,16);auto size=dimensions(at+8);auto pixels=uint64_t(size[0])*size[1];totalPixels+=pixels;
   if(++totalFrames>4096||totalPixels>33554432)throw std::runtime_error("Sprite exceeds the thumbnail frame/pixel budget.");
   if(frame==unsigned(selectedFrame)&&sub==0){chosen=at+16;width=size[0];height=size[1];result.originX=int32_t(word(bytes,at));result.originY=int32_t(word(bytes,at+4));}
   at+=16;auto length=pixels*(version==32?4:1);range(bytes,at,length);at+=size_t(length);
  }
 }
 result.width=width;result.height=height;result.additive=version==2&&mode==1;
 std::string tga(18+size_t(width)*height*4,0);tga[2]=2;tga[12]=width&255;tga[13]=width>>8;tga[14]=height&255;tga[15]=height>>8;tga[16]=32;tga[17]=0x28;
 for(size_t i=0;i<size_t(width)*height;i++){
  unsigned r,g,b,a;if(version==32){r=(unsigned char)bytes[chosen+i*4];g=(unsigned char)bytes[chosen+i*4+1];b=(unsigned char)bytes[chosen+i*4+2];a=(unsigned char)bytes[chosen+i*4+3];}
  else{unsigned index=(unsigned char)bytes[chosen+i];r=(unsigned char)colors[index*3];g=(unsigned char)colors[index*3+1];b=(unsigned char)colors[index*3+2];a=mode==2?index:mode==3&&index==255?0:255;}
  tga[18+i*4]=b;tga[19+i*4]=g;tga[20+i*4]=r;tga[21+i*4]=a;
 }result.tga=std::move(tga);return result;
}
}
