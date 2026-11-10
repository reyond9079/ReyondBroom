// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "schema.h"
#include "mesh.h"
#include "botanical_shapes.h"
#include <zlib.h>
namespace ftebroom::plant {
struct Image {int width=0,height=0;std::vector<unsigned char> rgba;};
struct Textures {Image albedo,normal,orm;};
enum class PhotoDetail {
  Lawn, Fescue, Feather, Sedge, OakBark, PineBark, BirchBark, SmoothBark,
  Amanita, Bolete, Chanterelle, Morel, Oyster, CrackedCap, Gills, Stipe, DottedCap,
  Leaf, GrassBlade, Needle, YellowPetal, RosePetal, VioletPetal, Cactus, Bamboo, Palm, None
};
struct PhotoAtlas {
  static constexpr int side=192, layers=int(Photo::Lawn);
  std::vector<unsigned char> rgba,details;
  static std::vector<unsigned char> decodeLayers(const char* bytes,size_t size,int count) {
    std::vector<unsigned char> data(size_t(side)*side*count*4);uLongf length=data.size();
    if(uncompress(data.data(),&length,reinterpret_cast<const Bytef*>(bytes),size)!=Z_OK||length!=data.size())
      throw std::runtime_error("Invalid photographic plant textures");
    return data;
  }
  static PhotoAtlas decode(const char* bytes,size_t size) {
    PhotoAtlas atlas;atlas.rgba=decodeLayers(bytes,size,layers);return atlas;
  }
  void loadDetails(const char* bytes,size_t size) {details=decodeLayers(bytes,size,int(PhotoDetail::None));}
  bool has(Photo layer) const {return layer!=Photo::None&&(int(layer)<layers?!rgba.empty():!details.empty());}
  std::array<double,4> sample(Photo layer,double u,double v) const {
    return int(layer)<layers?sampleLayer(rgba,int(layer),u,v):sampleLayer(details,int(layer)-layers,u,v);
  }
  std::array<double,4> sample(PhotoDetail layer,double u,double v) const {
    return sampleLayer(details,int(layer),u,v);
  }
private:
  static std::array<double,4> sampleLayer(const std::vector<unsigned char>& data,int layer,double u,double v) {
    double x=u*(side-1),y=v*(side-1);int ix=int(x),iy=int(y);
    int nx=std::min(ix+1,side-1),ny=std::min(iy+1,side-1);
    auto texel=[&](int sx,int sy,int c){return data[((layer*side+sy)*side+sx)*4+c];};
    std::array<double,4> result{};
    for(int c=0;c<4;c++){
      double top=texel(ix,iy,c)*(1-x+ix)+texel(nx,iy,c)*(x-ix);
      double bottom=texel(ix,ny,c)*(1-x+ix)+texel(nx,ny,c)*(x-ix);
      result[c]=(top*(1-y+iy)+bottom*(y-iy))/255.;
    }
    return result;
  }
};
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
inline Texel legacyTexel(const Project& d,int mat,double u,double v) {
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
    c=blend(c,{.52,.43,.25},std::clamp(p.dryness+(p.season==3?.20:p.season==2?.10:0),0.,1.)*.7);
    if(p.season==0)c=blend(c,{.37,.51,.22},.10);
  }else {c=mul(c,.70+n*.5);h=n*.3;}
  c=mul(c,1+p.variation*(fine-.5));for(double& x:c){x=std::clamp(x,0.,1.);if(p.quality==3)x=std::min(1.,std::round(x*5)/5.+.04);}
  return {c,h,alpha};
}
// Paired sprays on an axial twig. Offset their bases and lengths so the
// silhouette stays open between branchlets, even in the distant atlas mips.
inline double scaleSprayCoverage(double x,double v,double grain,double& relief) {
  x-=.035*v*(1.-v)*4.;
  double edge=.032*(1.-.75*v)-std::abs(x);
  for(int i=0;i<7;i++){
    const double base=.10+.105*i+(x>0.?.023:0.);
    const double rise=.37-.027*i;
    const double t=(v-base)/rise;
    if(t<0.||t>1.)continue;
    const double reach=(.76-.053*i)*(x>0.?.93:1.);
    const double center=reach*t*(1.42-.42*t);
    const double scale=1.-std::abs(2.*(t*8.-std::floor(t*8.))-1.);
    const double radius=(.088+.081*scale)*(1.-t)*(.78+.22*grain);
    const double distance=std::abs(std::abs(x)-center);
    edge=std::max(edge,radius-distance);
    relief=std::max(relief,std::clamp((radius-distance)*20.,0.,1.)*(.45+.55*scale));
  }
  return std::clamp(edge*52.+.5,0.,1.);
}
inline Texel texel(const Project& d,int material,double u,double v) {
  if(d.generator==1)return legacyTexel(d,material,u,v);
  int mat=material>=LeafLight&&material<=LeafYoung?Leaf:material>=NeedleLight?Needle:material;
  auto& p=d.p;uint64_t seed=mix(d.seed,mat);Texel sample{};
  // Only the remaining material types need the legacy basis. Computing a full
  // legacy leaf/bark sample just to overwrite it doubles atlas generation cost.
  bool authored=mat==Bark||mat==Wood||mat==Leaf||mat==Needle||mat==Grass||mat==Petal||
    (d.family==Family::Mushroom&&(mat==Cap||mat==Gills))||(d.family==Family::Flower&&mat==Center)||
    (d.family==Family::Cactus&&mat==CactusSkin&&p.organ==2);
  if(!authored)sample=legacyTexel(d,mat,u,v);
  double grain=fractal(u*27,v*33,seed);
  if(mat==Bark||mat==Wood){
    if(d.family==Family::Mushroom){
      // A stipe has fine axial fibres and mottling, never the exposed lamellae
      // texture. Decurrent chanterelle stems share the cap's ochre pigment.
      V base=p.organ==3?blend(p.bark,p.flower,.65):p.bark;
      double fibre=smoothNoise(u*88,v*6,seed+20),mottle=fractal(u*9,v*11,seed+29);
      sample.color=mul(base,.82+.16*fibre+.14*mottle);sample.height=.30+.045*fibre+.025*grain;
      if(p.organ==1){double net=std::pow(std::max(0.,std::sin((u*17+smoothNoise(u*8,v*7,seed)*.6)*pi)*std::sin((v*22+u)*pi)),8);sample.color=blend(sample.color,mul(base,.58),net*.30);sample.height+=net*.045;}
    }else{
      double a=u*2*pi,broad=fractal(std::cos(a)*2.4,std::sin(a)*2.4+v*4,seed+20);
      double warp=smoothNoise(std::cos(a)*3,std::sin(a)*3+v*7,seed+49);
      double fissure=std::pow(1-std::abs(std::sin((u*17+warp*.8)*pi)),8);
      double crossBreak=std::pow(1-std::abs(std::sin((v*23+smoothNoise(u*14,v*3,seed+3)*1.4)*pi)),18);
      double relief=.46+.16*broad+.08*grain-.35*fissure-.06*crossBreak;
      V base=p.bark;sample.color=mul(base,.82+.28*broad+.10*grain);sample.height=relief;
      switch(p.barkPattern){
        case 1:{ // smooth beech/cherry bark, subtle pores and small lenticels
          double pore=std::pow(std::max(0.,smoothNoise(u*67,v*93,seed)-.64)*2.8,3);
          sample.color=mul(base,.88+.13*broad+.07*grain-pore*.10);sample.height=.35+.025*grain-.03*pore;break;
        }
        case 2:{ // birch: discontinuous horizontal lenticels and peeling sheets
          double scar=std::pow(std::max(0.,smoothNoise(u*11,v*87,seed+7)-.59)*2.7,2);
          double peel=std::pow(1-std::abs(std::sin((v*19+warp*.13)*pi)),22)*std::clamp((broad-.42)*5,0.,1.);
          sample.color=blend(mul(base,.90+.12*grain),{.12,.115,.09},std::clamp(scar*.84+peel*.23,0.,.9));sample.height=.36+.035*grain-.10*scar+.045*peel;break;
        }
        case 3:{ // irregular, interlocking scaly plates on pine/alder/chestnut
          double seam=std::max(fissure*.82,crossBreak*.72),plate=fractal(u*31,v*37,seed+81);
          sample.color=blend(mul(base,.86+.30*plate),mul(base,.35),seam*.76);sample.height=.45+.16*plate-.30*seam;break;
        }
        case 4:sample.color=blend(mul(base,.85+.28*broad),mul(base,.32),fissure*.85);sample.height=.48+.08*grain-.37*fissure;break;
        case 5:{
          double scar=std::pow(1-std::abs(std::sin((v*17+warp*.09)*pi)),16);
          sample.color=mul(base,.87+.14*grain-scar*.24);sample.height=.32+.035*grain-.10*scar;break;
        }
        case 6:{double cork=fractal(u*43,v*48,seed+72);sample.color=blend(mul(base,.72+.50*cork),mul(base,.27),fissure*.70);sample.height=.25+.38*cork-.14*fissure;break;}
        case 7:{double patch=std::clamp((broad-.43)*7,0.,1.);sample.color=blend(mul(base,.76+.24*grain),blend(base,{.61,.60,.48},.55),patch);sample.height=.30+grain*.035+patch*.025;break;}
        default:sample.color=blend(sample.color,mul(base,.30),fissure*.82+crossBreak*.12);
      }
      // Lichen forms sparse cool patches, not an overall green tint.
      if(p.barkPattern!=2&&d.family!=Family::Bamboo&&d.family!=Family::Palm){double lichen=std::clamp((fractal(u*18,v*28,seed+307)-.69)*6,0.,.34);sample.color=blend(sample.color,{.44,.46,.36},lichen);}
    }
  }else if(mat==Leaf||mat==Needle||mat==Grass){
    double x=2*(u-.5),width=botanicalLeafWidth(mat==Grass?2:p.leafShape,v,p.leafProfile,p.serration);
    sample.alpha=std::clamp((width-std::abs(x))*100,0.,1.);
    if(p.leafShape==4&&mat!=Grass)sample.alpha=botanicalPalmateMask(u,v,p.leaflets)?1:0;
    const bool scales=mat==Needle&&treeScaleFoliage(d);
    double scaleRelief=0.;
    if(scales){
      sample.alpha=scaleSprayCoverage(x,v,grain,scaleRelief);
    }else if(mat==Needle){
      double mask=std::abs(x)<.024?1:0;
      if(p.needleForm==0||p.needleForm==3){
        int needles=p.needleForm==0?9:12;
        for(int i=0;i<needles;i++){
          double base=.10+i*.055,t=(v-base)/.37;
          if(t>=0&&t<=1&&std::abs(std::abs(x)-t*(.60+.25*(i%3))*(1-.15*t))<.036)mask=1;
        }
      }else if(p.needleForm==2){
        double edge=(1-v)*(.58+.18*std::sin(v*pi*12));mask=std::abs(x)<edge?.95:mask;
      }else{
        for(int i=0;i<9;i++){double base=.04+i*.095,t=(v-base)/.17;if(t>=0&&t<=1&&std::abs(std::abs(x)-t*.9)<(p.needleForm==4?.14:.075))mask=1;}
      }sample.alpha=mask;
    }
    // Pigment variation follows leaf age, tissue and veins. It is deliberately
    // low frequency so an individual leaf does not resemble painted static.
    V base=p.leaf;double age=material==LeafLight||material==NeedleLight?.14:material==LeafYoung?.23:material==LeafMature||material==NeedleMature?-.12:0;
    if(age>0)base=blend(base,{std::min(1.,base[0]*1.36+.025),std::min(1.,base[1]*1.15+.018),base[2]*.87},age*2);
    else base=mul(base,1+age);
    sample.color=mul(base,.84+.12*v+.12*grain);
    double mid=std::exp(-std::abs(x-.014*std::sin(v*pi))*130),vein=0;
    if(mat!=Needle&&mat!=Grass){for(int i=0;i<8;i++){double y=.10+i*.105+std::pow(std::abs(x),.85)*.21;vein=std::max(vein,std::exp(-std::abs(v-y)*210));}vein*=std::pow(std::max(0.,1-std::abs(x)),.5);}
    if(p.leafShape==4&&mat!=Needle&&mat!=Grass){
      vein=0;
      for(int i=0;i<std::clamp(p.leaflets,3,9);i++){
        auto tip=botanicalPalmatePoint(3+i*4,p.leaflets);double dx=tip[0]-.5,dy=tip[1]-.10,t=((u-.5)*dx+(v-.10)*dy)/(dx*dx+dy*dy);
        if(t>0&&t<1)vein=std::max(vein,std::exp(-std::hypot(u-.5-dx*t,v-.10-dy*t)*220));
      }
    }
    sample.color=blend(sample.color,mul(base,1.24),mid*.38+vein*.16);
    double margin=std::clamp((std::abs(x)/std::max(.025,width)-.82)*5,0.,1.);
    sample.color=mul(sample.color,1-margin*.065);sample.height=.32+.025*grain+mid*.085+vein*.026;
    if(scales){
      sample.color=blend(mul(base,.74+.38*grain+.24*scaleRelief),{.34,.26,.16},mid*.27);
      sample.height=.31+.12*scaleRelief+.06*mid;
    }
    double dry=p.dryness+((p.evergreen==1||p.evergreen==3)?0:p.season==3?.65:p.season==2?.35:0);
    if(p.season==2&&p.evergreen!=1&&p.evergreen!=3){
      const V autumn=d.generator>=5&&d.species=="lombardy-poplar"?V{.76,.55,.13}:V{.55,.28,.085};
      sample.color=blend(sample.color,autumn,.53+.12*grain);
    }
    sample.color=blend(sample.color,{.47,.39,.23},std::clamp(dry,0.,.94));
    if(p.season==0)sample.color=blend(sample.color,mul(base,1.18),.18);
  }else if(mat==Petal){
    double rib=.5+.5*std::sin((u*19+.23*std::sin(v*pi))*pi),base=std::pow(1-v,3);
    sample.color=mul(p.flower,.88+.10*v+.06*grain+rib*.025);
    V throat=p.organ==2&&p.petals<=4?V{.10,.075,.07}:blend(p.flower,{.66,.48,.20},.35);
    sample.color=blend(sample.color,throat,base*.45);sample.height=.25+.025*grain+.018*rib;
    if(p.organ==10){double spot=std::pow(std::max(0.,smoothNoise(u*21,v*17,seed)-.66)*3,2);sample.color=blend(sample.color,{.25,.045,.15},spot*(1-v));}
  }else if(mat==Cap&&d.family==Family::Mushroom){
    double radial=smoothNoise(u*84,v*12,seed+30),mottle=fractal(u*13,v*11,seed+48);
    sample.color=mul(p.flower,.80+.28*mottle+.06*radial);sample.height=.32+.055*mottle+.02*radial;
    if(p.organ==0){ // fine cuticle fibres; the warts are small raised organs
      sample.color=mul(sample.color,1-.06*std::pow(1-v,5));
    }else if(p.organ==3){sample.color=mul(p.flower,.85+.13*mottle+.05*radial);sample.height=.32+.025*mottle;}
    else if(p.organ==4){double scale=std::pow(std::max(0.,smoothNoise(u*34,v*49,seed)-.49)*2,2);sample.color=blend(mul(p.bark,.91),mul(p.flower,.73),std::clamp(scale*2,0.,1.));sample.height=.28+.16*scale;}
    else if(p.organ==6){double ring=.5+.5*std::sin(v*pi*31+grain*.8);sample.color=mul(p.flower,.75+.35*ring+.08*mottle);sample.height=.28+.04*ring;}
    else if(p.organ==7){
      // Jittered cellular pits: unequal rounded honeycomb cells rather than a
      // regular wire grid. The raised pale walls also drive the normal map.
      double px=u*22,py=v*14,nearest=1e9,second=1e9;int ix=int(px),iy=int(py);
      for(int y=iy-1;y<=iy+1;y++)for(int x=ix-1;x<=ix+1;x++){
        double dx=px-(x+.18+.64*noise(x,y,seed+83)),dy=(py-(y+.18+.64*noise(x,y,seed+91)))*1.22;
        double distance=dx*dx+dy*dy;if(distance<nearest){second=nearest;nearest=distance;}else second=std::min(second,distance);
      }
      double wall=std::clamp((.15-(std::sqrt(second)-std::sqrt(nearest)))*8,0.,1.);
      sample.color=blend(mul(p.flower,.43+.10*grain),mul(p.flower,1.50+.12*grain),wall);sample.height=.12+.62*wall;
    }
  }else if(mat==Gills&&d.family==Family::Mushroom){
    V base=p.organ==3?blend(p.bark,p.flower,.70):p.bark;
    double ridge=.5+.5*std::sin(u*pi*(16+56*p.gills));
    sample.color=mul(base,.86+.09*grain+.05*ridge);sample.height=.32+.035*ridge;
    if(p.organ==1){double pore=std::pow(std::max(0.,smoothNoise(u*97,v*103,seed)-.50)*2,2);sample.color=blend(mul(base,.93),mul(base,.53),pore*.64);sample.height=.34-pore*.15;}
  }else if(mat==CactusSkin&&d.family==Family::Cactus&&p.organ==2){
    double column=std::floor(u*11),x=u*11-column-.5,y=v*9+(int(column)%2)*.5;y-=std::floor(y);y-=.5;
    double areole=std::exp(-(x*x+y*y)*110);
    sample.color=blend(mul(p.leaf,.88+.16*grain),{.54,.48,.31},areole*.72);sample.height=.30+grain*.035+areole*.20;
  }else if(mat==Center&&d.family==Family::Flower){
    V base=p.organ==2&&p.petals<=4?V{.12,.105,.075}:p.organ==12?V{.27,.20,.075}:V{.73,.49,.10};
    double cell=std::pow(.5+.5*std::sin(u*pi*47)*std::sin(v*pi*53),2);
    sample.color=mul(base,.80+.20*grain+.12*cell);sample.height=.25+.09*cell;
  }
  for(auto& c:sample.color){c=std::clamp(c,0.,1.);if(p.quality==3)c=std::min(1.,std::round(c*5)/5.+.04);}
  return sample;
}
inline PhotoDetail materialPhoto(const Project& d,int mat) {
  auto& p=d.p;using D=PhotoDetail;
  if(d.generator<4||p.quality==3)return D::None;
  if(d.family==Family::Mushroom){
    if(mat==Bark||mat==Wood)return D::Stipe;
    if(mat==Gills)return p.organ==1?D::DottedCap:D::Gills;
    if(mat==Cap){
      switch(p.organ){
        case 0:return d.species=="amanita"?D::Amanita:D::Oyster;
        case 1:return D::Bolete;
        case 2:case 4:return D::Oyster;
        case 3:return D::Chanterelle;
        case 5:case 6:return D::CrackedCap;
        case 7:return D::Morel;
      }
    }
  }
  if(mat==Bark||mat==Wood){
    if(d.family==Family::Bamboo)return D::Bamboo;
    if(d.family==Family::Conifer)return D::PineBark;
    if(d.family==Family::Broadleaf||d.family==Family::Bush||d.family==Family::Custom||d.family==Family::Palm)
      return p.barkPattern==2?D::BirchBark:p.barkPattern==1||p.barkPattern==5||p.barkPattern==7?D::SmoothBark:D::OakBark;
    return D::GrassBlade;
  }
  if(mat==Needle||mat==NeedleLight||mat==NeedleMature)return treeScaleFoliage(d)?D::None:D::Needle;
  if(mat==Grass||d.family==Family::Riparian)return D::GrassBlade;
  if(mat==CactusSkin)return D::Cactus;
  if(mat==Petal||mat==Center){
    if(p.flower[2]>p.flower[0]*.85&&p.flower[2]>p.flower[1]*1.20)return D::VioletPetal;
    return p.flower[0]>p.flower[1]*1.30?D::RosePetal:D::YellowPetal;
  }
  if(mat==Leaf||mat==LeafLight||mat==LeafMature||mat==LeafYoung)return d.family==Family::Palm?D::Palm:D::Leaf;
  return D::None;
}
inline void applyMaterialPhoto(Texel& texel,const Project& d,int mat,PhotoDetail layer,
                              const PhotoAtlas& photos,double u,double v){
  auto mirror=[](double t){return 1.-std::abs(2.*(t-std::floor(t))-1.);};
  bool wraps=mat==Bark||mat==Wood||mat==Cap||mat==Gills||mat==CactusSkin;
  if(layer==PhotoDetail::Morel){u*=2.;v=mirror(v*1.35);}
  auto pixel=photos.sample(layer,wraps?mirror(u+double(d.seed%47)/47.):u,v);
  double light=.23*pixel[0]+.67*pixel[1]+.10*pixel[2];
  V original{pixel[0],pixel[1],pixel[2]};
  if(d.family==Family::Mushroom&&mat==Cap){
    // Keep genuine cap markings, while still accepting a custom cap hue.
    double tint=d.species=="fantasy"?.88:.22;
    auto base=layer==PhotoDetail::Morel?texel.color:mul(d.p.flower,.45+light*.90);
    texel.color=blend(original,base,tint);
  }else{
    auto shade=mul(texel.color,std::clamp(.42+light*1.40,.55,1.30));
    double natural=mat==Bark&&layer==PhotoDetail::BirchBark?.64:
      mat==Bark&&d.family==Family::Broadleaf?.28:mat==Gills?.48:.14;
    texel.color=blend(shade,original,natural);
  }
  texel.height=texel.height*.55+light*.45;
}
inline Textures textureMaps(const Project& d,const Image* imported=nullptr,const std::atomic<bool>* cancel=nullptr,int tile=256,const PhotoAtlas* photos=nullptr,
                            const Image* bark=nullptr,const Image* cut=nullptr) {
  if(tile<16||tile>256)throw std::runtime_error("Invalid plant atlas resolution");
  Textures out;const int size=tile*4;
  for(auto image:{&out.albedo,&out.normal,&out.orm}){image->width=image->height=size;image->rgba.assign(size_t(size)*size*4,0);}
  std::vector<double> heights(size_t(tile)*tile);
  auto foliage=foliagePhoto(d);
  bool roughBark=photos&&d.generator>=3&&d.p.quality!=3&&
    (d.family==Family::Broadleaf||d.family==Family::Conifer||d.family==Family::Bush)&&
    (d.p.barkPattern==0||d.p.barkPattern==3||d.p.barkPattern==4||d.p.barkPattern==6);
  int materialCount=d.generator==1?10:foliage==Photo::None?int(PhotoFoliage):int(Count);
  for(int mat=0;mat<materialCount;mat++) {
    auto detail=photos&&!photos->details.empty()?materialPhoto(d,mat):PhotoDetail::None;
    const bool foliageTile=mat==Leaf||mat==Needle||mat==Grass||mat>=LeafLight;
    const auto* override=mat==Bark?bark:mat==Wood?cut:foliageTile?imported:nullptr;
    const bool custom=override&&override->width>0&&override->height>0&&override->rgba.size()==size_t(override->width)*override->height*4;
    for(int y=0;y<tile;y++) {
      if(cancel&&cancel->load())throw std::runtime_error("Plant generation cancelled");
      for(int x=0;x<tile;x++) {
        // Clamp the gutter to the edge of the actual tile contents.
        double u=std::clamp((x-tile*4./256)/(tile*248./256),0.,1.),v=std::clamp((y-tile*4./256)/(tile*248./256),0.,1.);
        auto sample=texel(d,mat==PhotoFoliage?int(Leaf):mat,u,v);
        if(photos&&mat==PhotoFoliage&&photos->has(foliage)){
          auto pixel=photos->sample(foliage,u,v);
          double light=.25*pixel[0]+.65*pixel[1]+.10*pixel[2];
          double dry=std::clamp(d.p.dryness*.75+(d.p.season==2&&d.p.evergreen!=1?.58:0.),0.,1.);
          V original{pixel[0],pixel[1],pixel[2]};
          V tint=mul(d.p.leaf,std::clamp(.55+light*1.55,.60,1.65));
          const bool detailedGrass=foliage>=Photo::Lawn;
          auto color=blend(original,tint,detailedGrass?.32:d.generator>=5?.73:.52);
          sample.color=blend(color,mul({.52,.32,.13},.65+light),dry);
          sample.alpha=pixel[3];sample.height=.25+light*.32;
        }else if(detail!=PhotoDetail::None&&mat!=PhotoFoliage){
          applyMaterialPhoto(sample,d,mat,detail,*photos,u,v);
        }else if(roughBark&&mat==Bark){
          // The generated bark runs along the branch axis. Vary its placement
          // per seed while keeping the U seam continuous.
          double phase=double(d.seed%71)/71.;
          auto wrap=[](double t){return t-std::floor(t);};
          double x=wrap(u+phase),y=wrap(v*1.5+phase);
          auto pixel=photos->sample(Photo::Bark,x,y);
          double light=.25*pixel[0]+.65*pixel[1]+.10*pixel[2];
          sample.color=blend(sample.color,mul(d.p.bark,.47+light*1.35),.74);
          sample.height=sample.height*.38+light*.62;
        }
        for(auto& c:sample.color)c=std::clamp(c,0.,1.);
        size_t at=((mat/4*tile+y)*size+mat%4*tile+x)*4;
        for(int a=0;a<3;a++)out.albedo.rgba[at+a]=unsigned(std::round(sample.color[a]*255));
        out.albedo.rgba[at+3]=unsigned(sample.alpha*255);heights[y*tile+x]=sample.height;
        if(custom) {
          const int sx=int(u*(override->width-1)),sy=int(v*(override->height-1));
          const auto* pixel=override->rgba.data()+(size_t(sy)*override->width+sx)*4;
          for(int a=0;a<3;a++)out.albedo.rgba[at+a]=pixel[a];
          out.albedo.rgba[at+3]=foliageTile?pixel[3]:255;
          if(!foliageTile)heights[y*tile+x]=(.21*pixel[0]+.72*pixel[1]+.07*pixel[2])/255.;
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
