// SPDX-License-Identifier: MIT
#include "../plugins/autolevel/al_fill.h"
#include <iostream>
#include <stdexcept>
using namespace al;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
 for(int a=0;a<7;a++)for(int d=0;d<6;d++)check(near(linear(vec(delta[d]),BlockAction(a)),vec(delta[faceMap[a][d]])),"faceMap agrees with matrix");
 for(int f=0;f<256;f++){auto v=uint8_t(f);for(int i=0;i<4;i++)v=applyAction(v,BlockAction::RotateZ);check(v==f,"rotation fourth power");for(int a=3;a<7;a++)check(applyAction(applyAction(uint8_t(f),BlockAction(a)),BlockAction(a))==f,"reflection/flip square");
 for(int a=0;a<7;a++)for(int d=0;d<6;d++){uint8_t only=0;int axis=d%3==0?0:d%3==1?2:1;for(int i=0;i<8;i++)if(((i>>axis)&1)==(d>=3)&&(f&(1<<i)))only|=1<<i;auto transformed=applyAction(only,BlockAction(a));int td=faceMap[a][d];if(a==6)transformed=uint8_t(~only);check(sideFill(applyAction(uint8_t(f),BlockAction(a)),td)==sideFill(transformed,td),"all fill side permutations");}
 for(int d=0;d<6;d++){int axis=d%3==0?0:d%3==1?2:1;uint8_t b=0;for(int i=0;i<8;i++){int j=i^(1<<axis);if(f&(1<<i))b|=1<<j;}check(sideFill(f,d)==sideFill(b,opposite(d)),"touching face corner convention");}}
 check(sideKey(0,255,3,0,true)==sideKey(0,255,0,0,true),"Solid meets Solid");check(sideKey(0,0,3,0,true)==sideKey(0,0,0,0,true),"Empty meets Empty");check(sideKey(0,255,3,0,true)!=sideKey(0,0,0,0,true),"Solid differs Empty");
 check(Hash64().value==14695981039346656037ull,"FNV empty reference");check(Hash64().byte('a').value==12638187200555641996ull,"FNV a reference");check(Hash64().byte('h').byte('e').byte('l').byte('l').byte('o').value==11831194018420276491ull,"FNV hello reference");
 std::cout<<"PASS math: 42 directions, 256x7x6 fill permutations, adjacency, action identities, three pinned hash values\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
