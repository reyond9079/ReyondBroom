// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_sprite.h"
#include "../native/ftebroom_entity_defs.h"
#include <fstream>
#include <iostream>
using namespace ftebroom;
static void put(std::string& b,size_t at,uint32_t value){for(int i=0;i<4;i++)b[at+i]=char(value>>(i*8));}
static void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);std::cout<<"PASS "<<name<<'\n';}
static std::string sprite(unsigned version=1,unsigned mode=3,bool grouped=false){
 std::string b(version==2?40+770:36,0);b.replace(0,4,"IDSP");put(b,4,version);put(b,8,2);if(version==2){put(b,12,mode);b[40]=0;b[41]=1;for(int i=0;i<256;i++)for(int c=0;c<3;c++)b[42+i*3+c]=char(i);}
 put(b,version==2?20:16,2);put(b,version==2?24:20,1);put(b,version==2?28:24,1);
 auto at=b.size();b.resize(at+(grouped?12:4)+16+(version==32?8:2),0);put(b,at,grouped?1:0);at+=4;
 if(grouped){put(b,at,1);put(b,at+4,0x3f800000);at+=8;}
 put(b,at,uint32_t(-1));put(b,at+4,1);put(b,at+8,2);put(b,at+12,1);at+=16;
 if(version==32){b[at]=10;b[at+1]=20;b[at+2]=30;b[at+3]=40;b[at+4]=50;b[at+5]=60;b[at+6]=70;b[at+7]=80;}else{b[at]=7;b[at+1]=char(255);}return b;
}
int main(int argc,char** argv){try{
 std::string palette(768,0);for(int i=0;i<256;i++)for(int c=0;c<3;c++)palette[i*3+c]=char(i);
 auto image=readSprite(sprite(),palette);check(image.width==2&&image.height==1&&image.originX==-1&&image.originY==1,"quake_sprite_size_and_origin");check((unsigned char)image.tga[20]==7&&(unsigned char)image.tga[21]==255&&(unsigned char)image.tga[25]==0,"quake_palette_and_transparent_index");
 check(readSprite(sprite(1,3,true),palette).tga==image.tga,"grouped_sprite_thumbnail");auto rgba=readSprite(sprite(32),{});check((unsigned char)rgba.tga[18]==30&&(unsigned char)rgba.tga[20]==10&&(unsigned char)rgba.tga[21]==40,"rgba_sprite_channels_and_alpha");
 auto hl=readSprite(sprite(2),{});check((unsigned char)hl.tga[25]==0,"goldsrc_embedded_palette_alphatest");check(readSprite(sprite(2,0),{}).tga[25]==char(255),"goldsrc_opaque_mode");check(readSprite(sprite(2,1),{}).additive,"goldsrc_additive_mode");check(readSprite(sprite(2,2),{}).tga[21]==7,"goldsrc_index_alpha");
 for(const auto& broken:{sprite().substr(0,37),sprite().substr(0,56),std::string("IDSP"),sprite(32).substr(0,61)}){bool rejected=false;try{readSprite(broken,palette);}catch(...){rejected=true;}check(rejected,"truncated_sprite_rejected");}
 auto oversized=sprite();put(oversized,48,0x7fffffff);bool rejected=false;try{readSprite(oversized,palette);}catch(...){rejected=true;}check(rejected,"sprite_dimensions_bounded");
 std::string sp2(92,0);sp2.replace(0,4,"IDS2");put(sp2,4,2);put(sp2,8,1);put(sp2,12,32);put(sp2,16,16);sp2.replace(28,18,"sprites/example.pcx");auto external=readSprite(sp2,{});check(external.width==32&&external.image=="sprites/example.pcx","quake2_sp2_external_frame");
 auto definitions=entityRead(R"FGD(
 @BaseClass iconsprite("progs/inherited.spr") = BaseIcon []
 @PointClass base(BaseIcon) = icon_child []
 @PointClass ICONSPRITE("progs/direct.spr") = icon_direct []
 @PointClass sprite("sprites/fixed.spr") = sprite_fixed []
 @PointClass sprite() = sprite_dynamic [model(studio):"Sprite":"sprites/default.spr"]
 @PointClass model("progs/model.mdl") iconsprite("progs/icon.spr") = prefer_model []
 @PointClass iconsprite("progs/icon.spr") model("progs/model.mdl") = prefer_model_reverse []
 )FGD",".fgd");
 check(definitions.at("icon_child").model=="progs/inherited.spr","fgd_inherited_icon_sprite");check(definitions.at("icon_direct").model=="progs/direct.spr","fgd_iconsprite_case_insensitive");check(definitions.at("sprite_fixed").model=="sprites/fixed.spr","fgd_sprite_literal");check(definitions.at("sprite_dynamic").modelKey=="model"&&definitions.at("sprite_dynamic").model=="sprites/default.spr","fgd_sprite_property_and_default");check(definitions.at("prefer_model").model=="progs/model.mdl"&&definitions.at("prefer_model_reverse").model=="progs/model.mdl","explicit_model_precedes_icon");
 if(argc>1){int count=0;for(int arg=1;arg<argc;arg++){std::ifstream in(argv[arg],std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});auto offset=quake1::word(bytes,4),length=quake1::word(bytes,8);quake1::range(bytes,offset,length);for(size_t at=offset;at<offset+length;at+=64){auto name=quake1::name(bytes,at,56);if(name.size()<4||name.substr(name.size()-4)!=".spr")continue;auto pos=quake1::word(bytes,at+56),len=quake1::word(bytes,at+60);quake1::range(bytes,pos,len);auto decoded=readSprite(bytes.substr(pos,len),palette);if(decoded.tga.empty())throw std::runtime_error("Empty AD sprite: "+name);count++;}}check(count>50,"actual_ad_pak_sprite_frames_decode");std::cout<<"Decoded "<<count<<" real mod sprites\n";}
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
