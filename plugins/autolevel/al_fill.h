// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include "al_math.h"
#include "al_hash.h"
namespace al {
inline uint8_t calculateFill(uint8_t f,BlockAction a){if(a==BlockAction::Flip)return uint8_t(~f);uint8_t out=0;for(int i=0;i<8;i++)if(f&(1<<i)){auto p=linear({double(i&1)-.5,double((i>>1)&1)-.5,double((i>>2)&1)-.5},a)+Vec3{.5,.5,.5};int k=int(std::lround(p.x))|int(std::lround(p.y))<<1|int(std::lround(p.z))<<2;out|=1<<k;}return out;}
inline uint8_t applyAction(uint8_t f,BlockAction a){static const auto table=[](){std::array<std::array<uint8_t,7>,256> t{};for(int f=0;f<256;f++)for(int a=0;a<7;a++)t[f][a]=calculateFill(f,BlockAction(a));return t;}();return table[f][int(a)];}
inline int sideFill(uint8_t f,int d){int axis=d%3==0?0:d%3==1?2:1;int u=(axis==0?1:0),v=(axis==2?1:2),out=0;for(int k=0;k<4;k++){int p[3]{};p[axis]=d>=3;p[u]=k&1;p[v]=(k>>1)&1;int i=p[0]|p[1]<<1|p[2]<<2;if(f&(1<<i))out|=1<<k;}return out;}
inline uint64_t sideKey(uint32_t id,uint8_t f,int d,int layer,bool filling){return filling?hash64(id,layer==0?sideFill(f,d):0):id;}
struct SideIds {std::array<uint32_t,6> v{};uint32_t& operator[](int d){return v[d];}uint32_t operator[](int d)const{return v[d];}};
inline SideIds applyAction(SideIds in,BlockAction a){SideIds out;for(int d=0;d<6;d++)out[faceMap[int(a)][d]]=in[d];return out;}
}
