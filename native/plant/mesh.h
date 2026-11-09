// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "schema.h"
#include <atomic>
#include <map>
namespace ftebroom::plant {
enum Material { Bark, Leaf, Needle, Petal, Center, Cap, Gills, CactusSkin, Grass, Wood,
                LeafLight, LeafMature, LeafYoung, NeedleLight, NeedleMature, PhotoFoliage, Count };
enum class Photo { Bark, Oak, Spruce, Fern, Grass, Maple, Birch, Pine, Ivy, Lawn, Fescue, Feather, Sedge, None };
inline Photo foliagePhoto(const Project& d) {
  if(d.generator<3||d.p.quality==3||d.p.foliageMode>=2||!d.texture.empty())return Photo::None;
  const auto& species=d.species;
  if(d.family==Family::Broadleaf&&d.p.leafShape==3&&species=="oak")return Photo::Oak;
  if(d.family==Family::Broadleaf&&d.p.leafShape==4&&species=="maple")return Photo::Maple;
  if(d.family==Family::Broadleaf&&(species=="birch"||species=="paper-birch"))return Photo::Birch;
  if(d.family==Family::Conifer&&(species=="pine"||species=="maritime-pine"||species=="stone-pine"))return Photo::Pine;
  if(d.family==Family::Vine&&d.p.organ==0&&species=="ivy")return Photo::Ivy;
  if(d.family==Family::Conifer&&(species=="spruce"||species=="blue-spruce"||species=="fir"))return Photo::Spruce;
  if(d.family==Family::Fern&&(d.p.organ<=3||d.p.organ==7))return Photo::Fern;
  if(d.family==Family::Grass&&species=="meadow-grass")return Photo::Grass;
  if(d.generator>=4&&d.family==Family::Grass){
    if(d.p.organ==1)return Photo::Lawn;
    if(d.p.organ==2)return Photo::Fescue;
    if(d.p.organ==3||d.p.organ==11)return Photo::Feather;
    if(d.p.organ==6)return Photo::Sedge;
  }
  return Photo::None;
}
struct Vertex {V p,n;std::array<double,2> uv;double wind=0,phase=0;};
struct Face {std::array<unsigned,3> i;int material;Part part;unsigned organ;uint64_t branch=0;};
struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<Face> faces;
  V low{1e30,1e30,1e30}, high{-1e30,-1e30,-1e30};
  bool budgetLimited=false;
  void bounds() {low={1e30,1e30,1e30};high={-1e30,-1e30,-1e30};for(auto& v:vertices)for(int a=0;a<3;a++){low[a]=std::min(low[a],v.p[a]);high[a]=std::max(high[a],v.p[a]);}}
};
inline V bezier(V a,V b,V c,V d,double t) {
  double s=1-t;return add(add(mul(a,s*s*s),mul(b,3*s*s*t)),add(mul(c,3*s*t*t),mul(d,t*t*t)));
}
inline std::array<double,2> atlasUV(int material,double u,double v) {
  // Tiles have a four-pixel gutter; sampling never crosses into a neighbour.
  constexpr double inset=4./256.;
  return {(material%4+inset+u*(1-2*inset))/4.,(material/4+inset+v*(1-2*inset))/4.};
}
// Keep each longitudinal repeat inside one atlas tile. A mirrored seam can
// share a ring without blending UVs across another material or splitting normals.
inline constexpr double barkRepeatLength=64.;
inline double barkRepeatV(double distance,double units=barkRepeatLength) {
  return 1.-std::abs(1.-std::fmod(distance/units,2.));
}
struct TubeStation {
  size_t segment;
  double blend,distance;
};
struct WoodJunction {
  V center{},tangent{0,0,1};
  double radius=0.;
  V radial(V point) const {
    auto delta=sub(point,center);
    return sub(delta,mul(tangent,dot(delta,tangent)));
  }
};
inline void addJunctionRing(std::vector<V>& path,const WoodJunction& joint) {
  const double edge=joint.radius;
  for(size_t i=1;i<path.size();i++){
    const double before=length(joint.radial(path[i-1])),after=length(joint.radial(path[i]));
    if(before>=edge||after<edge)continue;
    const double fraction=(edge-before)/(after-before);
    if(fraction>.04&&fraction<.96)
      path.insert(path.begin()+i,add(path[i-1],mul(sub(path[i],path[i-1]),fraction)));
    break;
  }
}
inline std::vector<TubeStation> barkStations(const std::vector<V>& path,double units=barkRepeatLength) {
  std::vector<TubeStation> stations{{0,0.,0.}};
  double distance=0.,nextRepeat=units;
  for(size_t i=0;i+1<path.size();i++){
    const double segment=length(sub(path[i+1],path[i])),end=distance+segment;
    for(;nextRepeat<end-1e-6;nextRepeat+=units)
      stations.push_back({i,(nextRepeat-distance)/segment,nextRepeat});
    if(nextRepeat<=end+1e-6)nextRepeat+=units;
    stations.push_back({i,1.,end});
    distance=end;
  }
  return stations;
}
struct Builder {
  Mesh mesh;
  size_t budget;
  const std::atomic<bool>* cancel;
  unsigned organ=0;
  bool repeatBark=false;
  bool separateWoodCut=false;
  int barkURepeats=1;
  double barkVUnits=barkRepeatLength;
  explicit Builder(size_t b,const std::atomic<bool>* c=nullptr):budget(b),cancel(c){}
  void check() const {if(cancel&&cancel->load())throw std::runtime_error("Plant generation cancelled");}
  bool room(size_t faces) {check();if(mesh.faces.size()+faces>budget){mesh.budgetLimited=true;return false;}return true;}
  unsigned vertex(V p,V n,double u,double v,int mat,double wind=0) {
    auto uv=atlasUV(mat,std::clamp(u,0.,1.),std::clamp(v,0.,1.));
    mesh.vertices.push_back({p,unit(n),uv,wind,double(organ%1024)*.618});return unsigned(mesh.vertices.size()-1);
  }
  void face(unsigned a,unsigned b,unsigned c,int mat,Part part) {
    auto n=cross(sub(mesh.vertices[b].p,mesh.vertices[a].p),sub(mesh.vertices[c].p,mesh.vertices[a].p));
    if(length(n)>1e-9)mesh.faces.push_back({{a,b,c},mat,part,organ});
  }
  void quad(unsigned a,unsigned b,unsigned c,unsigned d,int mat,Part part) {face(a,b,c,mat,part);face(a,c,d,mat,part);}
  bool tube(const std::vector<V>& path,double radius,double tip,int sides,int mat,Part part,double ribs=0,double hold=0,double wood=0,uint64_t seed=0,const WoodJunction* joint=nullptr) {
    if(path.size()<2)return false;
    const bool tiled=repeatBark&&mat==Bark;
    if(tiled)sides=(sides+barkURepeats-1)/barkURepeats*barkURepeats;
    const auto stations=tiled?barkStations(path,barkVUnits):std::vector<TubeStation>{};
    const size_t rings=stations.empty()?path.size():stations.size();
    if(!room((rings-1)*sides*2+sides*2))return false;
    organ++;unsigned first=mesh.vertices.size();V frame{1,0,0};
    double phase=wood?Rng(seed).range(0,2*pi):0;
    auto pathTangent=[&](size_t i){
      return unit(sub(path[std::min(i+1,path.size()-1)],path[i?i-1:0]));
    };
    for(size_t i=0;i<rings;i++) {
      const auto station=stations.empty()?TubeStation{i,0.,0.}:stations[i];
      const size_t a=station.segment;
      V center=path[a],tangent=pathTangent(a);
      if(station.blend>0.){
        center=station.blend==1.?path[a+1]:add(center,mul(sub(path[a+1],center),station.blend));
        const auto endTangent=pathTangent(a+1);
        tangent=station.blend==1.?endTangent:unit(add(mul(tangent,1-station.blend),mul(endTangent,station.blend)));
        if(length(tangent)<1e-6)tangent=endTangent;
      }
      frame=sub(frame,mul(tangent,dot(frame,tangent)));
      if(length(frame)<1e-5)frame=cross(tangent,std::abs(tangent[2])<.9?V{0,0,1}:V{0,1,0});
      frame=unit(frame);V binormal=unit(cross(tangent,frame));double t=(a+station.blend)/(path.size()-1);
      const double textureV=stations.empty()?t:barkRepeatV(station.distance,barkVUnits);
      double u=std::clamp((t-hold)/std::max(.01,1-hold),0.,1.);
      double r=std::max(.002,radius*std::pow(1-u,1.1)+tip*u);
      if(joint){
        const double at=length(joint->radial(center));
        const double collar=std::clamp(1.-std::abs(at-joint->radius)/std::max(radius*2.5,.01),0.,1.);
        r*=1.+.46*collar*collar;
      }
      for(int j=0;j<=sides;j++) {
        double angle=2*pi*j/sides;V radial=add(mul(frame,std::cos(angle)),mul(binormal,std::sin(angle)));
        double ridge=ribs>0?1+.10*std::cos(angle*ribs):1;
        if(wood){
          double flare=(part==Part::Trunk?std::pow(std::max(0.,1.-t*4.),2):0.);
          double ribs=std::cos(angle*3+phase+t*.7)+.42*std::cos(angle*5-phase+t*2.5);
          ridge*=1+wood*(.10*std::cos(2*angle-phase)+(.075+.16*flare)*ribs+.15*flare);
        }
        V normal=unit(add(radial,mul(tangent,(radius-tip)/std::max(.01,length(sub(path.back(),path.front()))))));
        const auto point=add(center,mul(radial,r*ridge));
        if(joint){
          const auto parent=joint->radial(point);
          const double distance=length(parent),reach=std::max(radius*2.,.01);
          const double blend=.7*std::clamp(1.-std::abs(distance-joint->radius)/reach,0.,1.);
          if(distance>1e-6&&dot(mul(parent,1./distance),normal)>-.55)
            normal=unit(add(mul(normal,1.-blend),mul(parent,blend/distance)));
        }
        const double across=double(j)/sides;
        const double textureU=tiled&&barkURepeats>1?barkRepeatV(across*barkURepeats,1.):across;
        vertex(point,normal,textureU,textureV,mat,t);
      }
    }
    for(unsigned i=0;i+1<rings;i++)for(int j=0;j<sides;j++) {
      unsigned a=first+i*(sides+1)+j,b=a+1,c=b+sides+1,d=a+sides+1;
      quad(a,b,c,d,mat,part);
    }
    V startNormal=unit(sub(path.front(),path[1])),endNormal=unit(sub(path.back(),path[path.size()-2]));
    if(mat==Bark&&separateWoodCut){
      const unsigned start=vertex(path.front(),startNormal,.5,.5,Wood);
      const unsigned end=vertex(path.back(),endNormal,.5,.5,Wood,1);
      const unsigned last=first+(rings-1)*(sides+1);
      auto rim=[&](unsigned index,V normal,int j,double wind){
        const double angle=2*pi*j/sides;
        return vertex(mesh.vertices[index+j].p,normal,.5+.5*std::cos(angle),.5+.5*std::sin(angle),Wood,wind);
      };
      const auto baseRim=mesh.vertices.size();
      for(int j=0;j<sides;j++)rim(first,startNormal,j,0.);
      const auto topRim=mesh.vertices.size();
      for(int j=0;j<sides;j++)rim(last,endNormal,j,1.);
      for(int j=0;j<sides;j++){
        const auto next=(j+1)%sides;
        face(start,baseRim+next,baseRim+j,Wood,part);
        face(end,topRim+j,topRim+next,Wood,part);
      }
      return true;
    }
    unsigned start=vertex(path.front(),startNormal,.5,.5,mat),end=vertex(path.back(),endNormal,.5,.5,mat,1);
    for(int j=0;j<sides;j++){face(start,first+j+1,first+j,mat,part);unsigned a=first+(rings-1)*(sides+1)+j;face(end,a,a+1,mat,part);}return true;
  }
  void foliageCard(V origin,V direction,V up,double size,double width,double bend,int segments,double twist) {
    if(!room(segments*2))return;
    direction=unit(direction);V right=unit(cross(direction,up));
    if(length(right)<.01)right=unit(cross(direction,V{1,0,0}));
    V normal=unit(cross(right,direction));
    unsigned first=mesh.vertices.size();organ++;
    for(int i=0;i<=segments;i++) {
      double t=double(i)/segments;
      V center=add(origin,add(mul(direction,t*size),mul(normal,bend*size*t*t)));
      V across=add(right,mul(normal,twist*t));
      V n=unit(sub(sub(normal,mul(direction,2*bend*t)),mul(right,twist*t)));
      vertex(sub(center,mul(across,width*.5)),n,0,t,PhotoFoliage,t);
      vertex(add(center,mul(across,width*.5)),n,1,t,PhotoFoliage,t);
    }
    for(int i=0;i<segments;i++){unsigned a=first+i*2;quad(a,a+1,a+3,a+2,PhotoFoliage,Part::Leaves);}
  }
  void leaf(V origin,V direction,V up,double size,double width,int shape,int mode,int mat,Part part,double curl,double twist,
            const std::array<double,5>& profile) {
    if(shape==7&&mode<2) {
      direction=unit(direction);V right=unit(cross(direction,up));
      for(int i=1;i<=4;i++)for(int side:{-1,1})leaf(add(origin,mul(direction,size*i*.18)),unit(add(mul(direction,.35),mul(right,side))),up,size*(.38-.04*i),width,2,1,mat,part,curl,twist,profile);
      leaf(add(origin,mul(direction,size*.72)),direction,up,size*.35,width,2,1,mat,part,curl,twist,profile);return;
    }
    if(shape==4&&mode<2) {
      int n=mode==1?10:20;if(!room(n))return;organ++;direction=unit(direction);V right=unit(cross(direction,up));up=unit(cross(right,direction));
      V center=add(origin,mul(direction,size*.5));unsigned first=mesh.vertices.size();vertex(center,up,.5,.5,mat,.5);
      for(int i=0;i<=n;i++) {
        double angle=2*pi*i/n,radius=.5*(.69+.31*std::cos(angle*5));
        double longitudinal=std::cos(angle)*radius,lateral=std::sin(angle)*radius;
        V point=add(center,add(mul(direction,size*longitudinal),mul(right,size*width*lateral)));
        point=add(point,mul(up,size*curl*longitudinal*longitudinal));
        vertex(point,unit(sub(up,mul(direction,2*curl*longitudinal))),.5+lateral,.5+longitudinal,mat,.8);
      }
      for(int i=0;i<n;i++)face(first,first+i+2,first+i+1,mat,part);return;
    }
    const int segments=mode>=2?1:mode==1?2:4;
    const int copies=mode==3?2:1;
    if(!room(segments*4*copies))return;
    direction=unit(direction);V right=unit(cross(direction,up));up=unit(cross(right,direction));
    for(int copy=0;copy<copies;copy++) {
      organ++;unsigned first=mesh.vertices.size();V r=copy?up:right,n=copy?mul(right,-1):up;
      for(int i=0;i<=segments;i++) {
        double t=double(i)/segments;
        double silhouette=mode>=2?1:std::max(.012,std::sin(pi*t));
        if(shape==1)silhouette=std::pow(silhouette,.62);
        if(shape==2)silhouette=std::pow(silhouette,1.8);
        if(shape==3)silhouette*=.68+.32*std::cos(t*pi*8);
        if(shape==4)silhouette*=.55+.45*std::abs(std::cos(t*pi*5));
        if(shape==5)silhouette=.08*std::pow(std::max(.01,std::sin(pi*t)),.25);
        if(shape==6)silhouette=std::max(.01,curve(profile,t));
        if(mode>=2)silhouette=1;
        double bend=curl*size*t*t;
        V center=add(origin,add(mul(direction,size*t),mul(n,bend)));
        V normal=unit(sub(n,mul(direction,2*curl*t)));
        for(int j=-1;j<=1;j++) {
          double w=size*width*.5*silhouette*j;
          V p=add(center,add(mul(r,w),mul(n,std::abs(w)*.14+twist*w*t)));
          vertex(p,normal,(j*silhouette+1)*.5,t,mat,t);
        }
      }
      for(int i=0;i<segments;i++)for(int j=0;j<2;j++) {
        unsigned a=first+i*3+j,b=a+3;quad(a,a+1,b+1,b,mat,part);
      }
    }
  }
  void gill(V inner,V outer,double depth) {
    if(!room(2))return;organ++;V radial=unit(sub(outer,inner)),normal=unit(cross(V{0,0,1},radial));
    unsigned a=vertex(inner,normal,0,0,Gills),b=vertex(outer,normal,1,0,Gills),c=vertex(sub(outer,{0,0,depth*.3}),normal,1,1,Gills),d=vertex(sub(inner,{0,0,depth}),normal,0,1,Gills);
    quad(a,b,c,d,Gills,Part::Surface);
  }
  void lathe(V center,const std::vector<std::array<double,2>>& profile,int sides,int mat,Part part,double ripple=0,double asymmetry=0) {
    if(profile.size()<2||!room((profile.size()-1)*sides*2))return;
    organ++;unsigned first=mesh.vertices.size();
    for(size_t k=0;k<profile.size();k++)for(int j=0;j<=sides;j++) {
      double a=2*pi*j/sides,t=double(k)/(profile.size()-1),r=profile[k][0]*(1+ripple*std::sin(a*7+k*.9));
      auto prev=profile[k?k-1:0],next=profile[std::min(k+1,profile.size()-1)];
      V n=unit({(next[1]-prev[1])*std::cos(a),(next[1]-prev[1])*std::sin(a),prev[0]-next[0]});
      vertex(add(center,{r*std::cos(a),r*std::sin(a),profile[k][1]+asymmetry*r*std::cos(a)}),n,double(j)/sides,t,mat,t);
    }
    for(unsigned k=0;k+1<profile.size();k++)for(int j=0;j<sides;j++) {
      unsigned a=first+k*(sides+1)+j;quad(a,a+1,a+sides+2,a+sides+1,mat,part);
    }
  }
};
inline Mesh hierarchy(const Mesh& source,const Project& d,int lod=0) {
  Mesh out;out.budgetLimited=source.budgetLimited;
  const auto budget=size_t(d.p.budget*(lod==0?1:lod==1?d.p.lod1Ratio:d.p.lod2Ratio));
  for(auto& node:d.nodes) {
    std::vector<const Component*> chain{&node};auto parent=node.parent;bool visible=node.visible;
    while(!parent.empty()) {auto it=std::find_if(d.nodes.begin(),d.nodes.end(),[&](auto& n){return n.id==parent;});visible&=it->visible;chain.push_back(&*it);parent=it->parent;}
    if(!visible)continue;
    std::map<unsigned,unsigned> indices;
    auto transform=[&](Vertex v) {
      for(auto c:chain) {
        for(int i=0;i<3;i++){v.p[i]*=c->scale[i];v.n[i]/=c->scale[i];}
        double a=c->rotation*pi/180,cs=std::cos(a),sn=std::sin(a);
        auto rotate=[&](V p){return V{p[0]*cs-p[1]*sn,p[0]*sn+p[1]*cs,p[2]};};
        v.p=add(rotate(v.p),c->offset);v.n=unit(rotate(v.n));
      }return v;
    };
    for(auto& f:source.faces)if(f.part==node.kind) {
      if(out.faces.size()>=budget) {out.budgetLimited=true;break;}
      auto copy=f;
      for(int a=0;a<3;a++){unsigned i=f.i[a];if(!indices.count(i)){indices[i]=out.vertices.size();out.vertices.push_back(transform(source.vertices[i]));}copy.i[a]=indices[i];}
      out.faces.push_back(copy);
    }
  }
  out.bounds();return out;
}
inline void validate(const Mesh& m) {
  if(m.faces.empty()||m.vertices.empty())throw std::runtime_error("Plant contains no visible geometry");
  for(auto& v:m.vertices) {
    for(double x:v.p)if(!std::isfinite(x)||std::abs(x)>100000)throw std::runtime_error("Invalid plant vertex");
    for(double x:v.n)if(!std::isfinite(x))throw std::runtime_error("Invalid plant normal");
    if(std::abs(length(v.n)-1)>.01)throw std::runtime_error("Plant normal is not normalized");
    for(double x:v.uv)if(!std::isfinite(x)||x<0||x>1)throw std::runtime_error("Invalid plant UV");
  }
  for(auto& f:m.faces){for(unsigned i:f.i)if(i>=m.vertices.size())throw std::runtime_error("Invalid plant triangle index");
    if(length(cross(sub(m.vertices[f.i[1]].p,m.vertices[f.i[0]].p),sub(m.vertices[f.i[2]].p,m.vertices[f.i[0]].p)))<1e-10)throw std::runtime_error("Degenerate plant triangle");}
}
inline V windPosition(const Vertex& v,const Params& p,double time,double height) {
  double t=std::clamp(v.p[2]/std::max(1.,height),0.,1.),a=p.windDirection*pi/180;
  double main=std::sin(time*p.windFrequency+v.p[2]*.013)*p.windStrength*(1-p.stiffness)*height*.045*t*t;
  double flutter=std::sin(time*p.windFrequency*5+v.phase)*p.flutter*p.windStrength*v.wind*t*.7;
  return add(v.p,{std::cos(a)*(main+flutter),std::sin(a)*(main+flutter),-.05*std::abs(main)*t});
}
}
