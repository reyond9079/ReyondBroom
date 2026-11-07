// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "schema.h"
#include "mesh.h"
#include <zlib.h>
namespace ftebroom::plant {
struct Image {int width=0,height=0;std::vector<unsigned char> rgba;};
struct Textures {Image albedo,normal,orm;};
inline double noise(int x,int y,uint64_t seed) {
  return double(mix(mix(seed,uint32_t(x)),uint32_t(y))>>11)*(1./9007199254740992.);
}
inline double smoothNoise(double x,double y,uint64_t seed) {
  int ix=int(std::floor(x)),iy=int(std::floor(y));double fx=x-ix,fy=y-iy;
  fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
  return ((1-fx)*noise(ix,iy,seed)+fx*noise(ix+1,iy,seed))*(1-fy)+((1-fx)*noise(ix,iy+1,seed)+fx*noise(ix+1,iy+1,seed))*fy;
}
inline double fractal(double x,double y,uint64_t seed) {
  return smoothNoise(x,y,seed)*.57+smoothNoise(x*2,y*2,seed+1)*.28+smoothNoise(x*4,y*4,seed+2)*.15;
}
inline V blend(V a,V b,double t){return add(mul(a,1-t),mul(b,t));}
struct Texel {V color;double height=0,alpha=1;};
inline Texel texel(const Project& d,int mat,double u,double v) {
  auto& p=d.p;uint64_t seed=mix(d.seed,mat);double n=fractal(u*14,v*18,seed),fine=noise(int(u*256),int(v*256),seed);
  V c=mat==Bark||mat==Wood?p.bark:mat==Petal||mat==Cap?p.flower:mat==Gills?(d.family==Family::Mushroom?p.bark:V{.79,.73,.57}):mat==Center?V{.35,.23,.055}:p.leaf;
  double h=n*.35,alpha=1;
  if(mat==Bark) {
    double crack=std::pow(std::abs(std::sin((u*13+fractal(u*3,v*7,seed)*.65)*pi)),.25);
    h=crack*.62+n*.28;c=mul(c,.48+.68*crack+n*.25);
    if(d.species=="birch") {
      double strip=std::pow(std::abs(std::sin((v*31+smoothNoise(u*15,v*2,seed))*pi)),18);
      c=blend(c,{.07,.065,.055},strip*.75*(.5+.5*std::sin(u*7+v*5)));
    }
    if(d.family==Family::Palm) {double ring=std::pow(std::abs(std::sin(v*pi*18)),.35);c=mul(c,.6+.5*ring);h=ring*.7;}
    if(d.family==Family::Bamboo) {double stripe=std::sin(u*pi*22)*.04;c=add(c,{stripe,stripe,stripe});}
  }else if(mat==Leaf||mat==Needle||mat==Grass) {
    double t=v,x=(u-.5)*2;
    double width=std::max(0.,std::sin(pi*t));
    if(p.leafShape==1)width=std::pow(width,.62);
    if(p.leafShape==2||mat==Grass)width=std::pow(width,1.6);
    if(p.leafShape==3)width*=.68+.32*std::cos(t*pi*8);
    if(p.leafShape==4)width*=.55+.45*std::abs(std::cos(t*pi*5));
    if(p.leafShape==5||mat==Needle)width=std::pow(width,.3)*.32;
    if(p.leafShape==6)width=std::clamp(curve(p.leafProfile,t),0.,1.);
    alpha=std::clamp((width-std::abs(x))*100,0.,1.);
    if(p.leafShape==4&&mat!=Needle){double px=(u-.5)*2,py=(v-.5)*2,angle=std::atan2(px,py);double edge=.69+.31*std::cos(angle*5);alpha=std::clamp((edge-std::hypot(px,py))*80,0.,1.);}
    if(mat==Needle) {
      // A needle shoot atlas: fine needles join a central woody axis, rather
      // than treating conifer foliage as opaque conical primitives.
      double mask=std::abs(x)<.035?1:0;
      for(int i=0;i<6;i++){double base=.08+i*.13,t=(v-base)/.26;if(t>=0&&t<=1&&std::abs(std::abs(x)-t*.90)<.035)mask=1;}
      alpha=mask;
    }
    double mid=std::exp(-std::abs(x)*95),vein=std::exp(-std::abs(std::sin((v*13-std::abs(x)*2.6)*pi))*30);
    c=mul(c,.66+.34*v+n*.26);c=blend(c,{.48,.56,.19},mid*.45+vein*.14);h=.32+n*.17+mid*.25+vein*.10;
    double dryness=p.dryness+(p.season==3?.65:p.season==2?.35:0);
    if(p.season==2&&d.family!=Family::Conifer)c=blend(c,{.73,.32,.06},.65);
    c=blend(c,{.56,.43,.22},std::clamp(dryness,0.,.95));
    if(p.season==0)c=blend(c,{.42,.61,.12},.15);
  }else if(mat==Petal) {
    double center=1-std::abs(u-.5)*2;c=mul(c,.68+.25*v+.12*center+n*.15);
    c=blend(c,{.95,.82,.30},std::pow(1-v,4)*.4);h=.2+n*.12+std::sin(u*pi*30)*.025;
  }else if(mat==Cap) {
    c=mul(c,.65+n*.5+fine*.06);h=n*.55;
    if(d.species=="amanita"||d.species=="fantasy") {
      double cells=smoothNoise(u*25,v*21,seed);double spot=std::clamp((cells-.75)*25,0.,1.);
      c=blend(c,{.90,.85,.70},spot);h+=spot*.2;
    }
    if(d.species=="woodland")c=mul(c,.7+.3*std::abs(std::sin(u*pi*45)));
  }else if(mat==Gills) {
    double ridge=.5+.5*std::sin(u*pi*(12+48*p.gills));h=.12+ridge*.45;c=mul(c,.60+.38*ridge+n*.1);
    if(d.species=="boletus"){double pore=noise(int(u*(32+128*p.gills)),int(v*(32+128*p.gills)),seed);c=mul(c,.7+.3*pore);h=pore*.2;}
  }else if(mat==CactusSkin) {
    double rib=.5+.5*std::cos(u*pi*2*p.ribs);h=rib*.5+n*.15;c=mul(c,.60+.45*rib+n*.13);
    double dots=std::pow(std::max(0.,std::cos(u*pi*2*p.ribs)),24)*std::pow(std::max(0.,std::cos(v*pi*26)),16);
    c=blend(c,{.72,.64,.39},dots*.8);h+=dots*.15;
  }else {c=mul(c,.70+n*.5);h=n*.3;}
  c=mul(c,1+p.variation*(fine-.5));for(double& x:c){x=std::clamp(x,0.,1.);if(p.quality==3)x=std::min(1.,std::round(x*5)/5.+.04);}
  return {c,h,alpha};
}
inline Textures textureMaps(const Project& d,const Image* imported=nullptr,const std::atomic<bool>* cancel=nullptr,int tile=256) {
  if(tile<16||tile>256)throw std::runtime_error("Invalid plant atlas resolution");
  Textures out;const int size=tile*4;
  for(auto image:{&out.albedo,&out.normal,&out.orm}){image->width=image->height=size;image->rgba.assign(size_t(size)*size*4,0);}
  std::vector<double> heights(size_t(tile)*tile);
  for(int mat=0;mat<Count;mat++) {
    for(int y=0;y<tile;y++) {
      if(cancel&&cancel->load())throw std::runtime_error("Plant generation cancelled");
      for(int x=0;x<tile;x++) {
        // Clamp the gutter to the edge of the actual tile contents.
        double u=std::clamp((x-tile*4./256)/(tile*248./256),0.,1.),v=std::clamp((y-tile*4./256)/(tile*248./256),0.,1.);
        auto sample=texel(d,mat,u,v);size_t at=((mat/4*tile+y)*size+mat%4*tile+x)*4;
        for(int a=0;a<3;a++)out.albedo.rgba[at+a]=unsigned(std::round(sample.color[a]*255));
        out.albedo.rgba[at+3]=unsigned(sample.alpha*255);heights[y*tile+x]=sample.height;
        if(imported&&(mat==Leaf||mat==Needle||mat==Grass)&&imported->width>0&&imported->height>0&&imported->rgba.size()==size_t(imported->width)*imported->height*4) {
          int sx=std::clamp(int(u*(imported->width-1)),0,imported->width-1),sy=std::clamp(int(v*(imported->height-1)),0,imported->height-1);
          for(int a=0;a<4;a++)out.albedo.rgba[at+a]=imported->rgba[(sy*imported->width+sx)*4+a];
        }
        out.orm.rgba[at]=unsigned((1-d.p.ao*(1-sample.height)*.65)*255);
        out.orm.rgba[at+1]=unsigned(std::clamp(d.p.roughness+(sample.height-.5)*.12,0.,1.)*255);
        out.orm.rgba[at+2]=0;out.orm.rgba[at+3]=255;
      }
    }
    for(int y=0;y<tile;y++)for(int x=0;x<tile;x++) {
      double dx=heights[y*tile+std::min(tile-1,x+1)]-heights[y*tile+std::max(0,x-1)];
      double dy=heights[std::min(tile-1,y+1)*tile+x]-heights[std::max(0,y-1)*tile+x];
      V n=unit({-dx*3,-dy*3,1});size_t at=((mat/4*tile+y)*size+mat%4*tile+x)*4;
      for(int a=0;a<3;a++)out.normal.rgba[at+a]=unsigned((n[a]*.5+.5)*255);out.normal.rgba[at+3]=255;
    }
  }return out;
}
inline std::string tga(const Image& image) {
  std::string out(18,0);out[2]=2;out[12]=char(image.width);out[13]=char(image.width>>8);out[14]=char(image.height);out[15]=char(image.height>>8);out[16]=32;out[17]=0x28;
  for(size_t i=0;i<image.rgba.size();i+=4){out+=char(image.rgba[i+2]);out+=char(image.rgba[i+1]);out+=char(image.rgba[i]);out+=char(image.rgba[i+3]);}return out;
}
inline void big32(std::string& s,uint32_t n){for(int i=3;i>=0;i--)s+=char(n>>(i*8));}
inline std::string png(const Image& image) {
  std::string raw;raw.reserve((image.width*4+1)*image.height);
  for(int y=0;y<image.height;y++){raw+='\0';raw.append(reinterpret_cast<const char*>(image.rgba.data()+size_t(y)*image.width*4),size_t(image.width)*4);}
  uLongf length=compressBound(raw.size());std::string compressed(length,0);
  if(compress2(reinterpret_cast<Bytef*>(compressed.data()),&length,reinterpret_cast<const Bytef*>(raw.data()),raw.size(),6)!=Z_OK)throw std::runtime_error("Plant PNG compression failed");compressed.resize(length);
  std::string out="\x89PNG\r\n\x1a\n";
  auto chunk=[&](const char* type,const std::string& data){big32(out,data.size());size_t start=out.size();out.append(type,4);out+=data;big32(out,crc32(0,reinterpret_cast<const Bytef*>(out.data()+start),4+data.size()));};
  std::string header;big32(header,image.width);big32(header,image.height);header.append("\x08\x06\0\0\0",5);chunk("IHDR",header);chunk("IDAT",compressed);chunk("IEND",{});return out;
}
}
