// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.h"
#include <chrono>
namespace ftebroom::plant {
struct Bud {V origin,direction;uint64_t seed;double size=1;};
struct Generator {
  const Project& doc;
  const Params& p;
  int lod;
  double detail,height;
  Builder b;
  std::vector<Bud> buds;
  Generator(const Project& d,int l,const std::atomic<bool>* cancel):doc(d),p(d.p),lod(l),
    detail((l==0?1:l==1?.55:.23)*(p.quality==1?.7:p.quality==2?.4:1)),
    height(p.height*(.4+.6*p.age)),b(size_t(p.budget*(l==0?1:l==1?p.lod1Ratio:p.lod2Ratio)),cancel) {}
  int count(int n,int minimum=1)const{return std::max(minimum,int(n*detail));}
  int sides()const{return std::max(3,count(p.sides,3));}
  double seasonDensity()const {
    if(doc.family==Family::Conifer||doc.family==Family::Palm)return p.season==3?.85:1;
    return p.season==3?.035:p.season==2?.58:p.season==0?.76:1;
  }
  V trunk(double t,V offset={})const {
    auto c=p.controls;for(auto& v:c){v[0]*=height;v[1]*=height;v[2]*=height;}
    auto q=bezier(c[0],c[1],c[2],c[3],t);Rng r(mix(doc.seed,77));double a=r.range(0,2*pi);
    q=add(q,{std::cos(a)*height*p.curvature*.20*t*t,std::sin(a)*height*p.curvature*.20*t*t,0});
    return add(q,offset);
  }
  std::vector<V> arc(V start,V end,double droop,int steps,uint64_t seed,double lateral=1)const {
    Rng r(seed);V delta=sub(end,start);double size=length(delta);
    V sideways=unit(cross(delta,V{0,0,1}));
    V c1=add(start,add(mul(delta,.33),V{0,0,size*(.16+p.light*.12)}));
    V c2=add(start,add(mul(delta,.72),V{0,0,size*(.18-droop*.28)}));
    c2=add(c2,mul(sideways,r.range(-1,1)*p.irregularity*size*.13*lateral));
    std::vector<V> path;for(int i=0;i<=steps;i++){double t=double(i)/steps;auto q=bezier(start,c1,c2,end,t);q[2]-=size*droop*.30*t*t;path.push_back(q);}return path;
  }
  void addLeaf(V origin,V direction,V up,uint64_t seed,double sizeScale=1,int material=Leaf,Part part=Part::Leaves) {
    Rng r(seed);double size=p.leafSize*sizeScale*r.range(.75,1.2)*(.5+.5*p.age);
    int mode=lod==2?3:p.foliageMode;
    if(p.quality==2&&lod<2)mode=1;
    if(material==Needle)mode=lod==2?3:2;
    if(mode==4)size*=1.65;
    if(lod==2)size*=1.25;
    b.leaf(origin,direction,up,size,p.leafWidth,p.leafShape,mode,material,part,
      p.leafCurl+r.range(-.1,.1)*p.irregularity,p.leafTwist*r.range(-1,1),p.leafProfile);
  }
  void roots(V origin,double radius) {
    int n=std::max(3,count(7));
    for(int i=0;i<n;i++) {
      double a=2*pi*i/n;V start=add(origin,{std::cos(a)*radius*.4,std::sin(a)*radius*.4,radius*2});
      V end=add(origin,{std::cos(a)*radius*(2+p.rootFlare),std::sin(a)*radius*(2+p.rootFlare),0});
      b.tube(arc(start,end,.05,4,mix(doc.seed,i)),radius*.4,.04,sides(),Bark,Part::Roots);
    }
  }
  void branch(V origin,V direction,double size,double radius,int depth,uint64_t id) {
    b.check();Rng r(mix(doc.seed,id));
    V end=add(origin,mul(direction,size));
    auto path=arc(origin,end,p.gravity,std::max(3,count(p.segments)),id);
    Part part=depth==0?Part::Primary:depth==1?Part::Secondary:Part::Twigs;
    if(!b.tube(path,radius,std::max(.025,radius*(1-p.taper)*.25),std::max(3,sides()-depth),Bark,part))return;
    const int depthLimit=doc.family==Family::Bush?std::min(2,p.depth):p.depth;
    if(depth+1<depthLimit) {
      int children=depth==0?3:2;
      for(int i=0;i<children;i++) {
        double t=.42+.45*(i+1)/children;
        V anchor=path[std::min(path.size()-1,size_t(t*(path.size()-1)))];
        V tangent=unit(sub(path.back(),anchor));V normal=unit(cross(tangent,V{0,0,1})),up=unit(cross(normal,tangent));
        double a=r.range(0,2*pi),angle=(.4+.25*r.real())*p.branchAngle*pi/90;
        V aim=unit(add(mul(tangent,std::cos(angle)),mul(add(mul(normal,std::cos(a)),mul(up,std::sin(a))),std::sin(angle))));
        aim[2]+=p.light*.3;
        branch(anchor,unit(aim),size*p.branchLength*r.range(.48,.74),radius*p.branchTaper,depth+1,mix(id,i+1));
      }
    }
    if(depth+1>=depthLimit&&r.real()>p.dead) {
      int n=std::max(1,int(p.leaves*p.density*seasonDensity()*detail));
      if(p.foliageMode==4)n=std::max(1,n/3);
      for(int i=0;i<n;i++) {
        double t=.30+.70*(i+.5)/n;auto q=path[std::min(path.size()-1,size_t(t*(path.size()-1)))];
        double a=i*2.399963+r.range(-.2,.2);
        V aim=unit({std::cos(a),std::sin(a),r.range(-.15,.65)});
        buds.push_back({q,aim,mix(id,i+42),1});
      }
    }
  }
  void broadleaf(bool shrub=false) {
    const auto totalBudget=b.budget;b.budget=std::max<size_t>(1,totalBudget*.45);
    int stems=shrub?p.clumps:1;std::vector<V> offsets;
    for(int stem=0;stem<stems;stem++) {
      Rng r(mix(doc.seed,stem+13));double a=stem*2.399963;
      V offset=shrub?V{std::cos(a)*height*.17,std::sin(a)*height*.17,0}:V{};
      if(doc.species=="hedge")offset={height*(stem-(stems-1)*.5)*.30,0,0};
      offsets.push_back(offset);
      auto origin=trunk(0,offset);roots(origin,p.radius);
      std::vector<V> path;for(int j=0;j<=p.segments;j++)path.push_back(trunk(double(j)/p.segments,offset));
      b.tube(path,p.radius*(1+p.rootFlare*.25),p.radius*(1-p.taper)*.15,sides(),Bark,Part::Trunk);
    }
    for(int i=0;i<p.branches;i++)for(int stem=0;stem<stems;stem++) {
        Rng r(mix(doc.seed,stem*71+i+13));auto offset=offsets[stem];
        double t=.22+.68*(i+.5)/p.branches,angle=p.branchAngle*pi/180;
        double az=i*2.399963+r.range(-.4,.4)*p.irregularity+stem;
        double size=height*p.spread*(.2+.55*curve(p.growth,t))*r.range(.82,1.15);
        if(doc.species=="willow")size*=1.15;
        V aim={std::cos(az)*std::sin(angle),std::sin(az)*std::sin(angle),std::cos(angle)};
        branch(trunk(t,offset),aim,size,p.radius*.30*(1-t*.65),0,mix(stem+7,i+100));
    }
    b.budget=totalBudget;
    // Skeletons are emitted before foliage, so budget reduction cannot remove
    // a late trunk while keeping only the leaves of an earlier branch.
    size_t cost=lod==2?8:p.foliageMode>=2?4:p.leafShape==7?72:p.leafShape==4?(p.foliageMode==1?10:20):p.foliageMode==1?8:16;
    size_t capacity=(b.budget-b.mesh.faces.size())/cost;
    size_t selected=std::min(capacity,buds.size());
    for(size_t j=0;j<selected;j++) {size_t i=j*buds.size()/selected;
      auto& q=buds[i];addLeaf(q.origin,q.direction,{0,0,1},q.seed,q.size);
      if(doc.species=="flowering-shrub"&&i%9==0)flower(q.origin,p.flowerSize*.45,mix(q.seed,91));
    }
  }
  void conifer() {
    const auto totalBudget=b.budget;b.budget=std::max<size_t>(1,totalBudget*.40);
    std::vector<V> path;for(int i=0;i<=p.segments;i++)path.push_back(trunk(double(i)/p.segments));
    roots({},p.radius);b.tube(path,p.radius,.04,sides(),Bark,Part::Trunk);
    int whorls=std::max(4,p.branches),per=doc.species=="spruce"?6:5;
    struct Shoot{std::vector<V> path;double span,a;uint64_t id;};std::vector<Shoot> shoots;
    for(int level=0;level<whorls;level++) {
      double t=.12+.84*(level+.5)/whorls,span=height*p.spread*std::pow(1-t,.8)*(.65+.35*curve(p.growth,t));
      for(int j=0;j<per;j++) {
        uint64_t id=mix(level+500,j+doc.seed);Rng r(id);double a=j*2*pi/per+level*1.27+r.range(-.12,.12);
        V base=trunk(t),end=add(base,{std::cos(a)*span,std::sin(a)*span,span*(doc.species=="spruce"?-.08:.18)});
        auto branchPath=arc(base,end,p.gravity,count(p.segments,3),id);
        if(b.tube(branchPath,p.radius*.28*(1-t),.02,std::max(3,sides()-3),Bark,Part::Primary))shoots.push_back({std::move(branchPath),span,a,id});
      }
    }
    for(auto& shoot:shoots){auto& branchPath=shoot.path;auto span=shoot.span,a=shoot.a;auto id=shoot.id;
        int twigs=std::max(3,count(p.leaves));
        for(int k=1;k<=twigs;k++)for(int side:{-1,1}) {
          double u=double(k)/(twigs+1);V q=branchPath[std::min(branchPath.size()-1,size_t(u*(branchPath.size()-1)))];
          V target=add(q,{std::cos(a+side*.65)*span*.27*(1-u*.7),std::sin(a+side*.65)*span*.27*(1-u*.7),span*.07});
          auto twig=arc(q,target,p.gravity,3,mix(id,k+side+30));
          b.tube({twig.front(),twig.back()},.13,.015,3,Bark,Part::Twigs);
          for(int n=0;n<std::max(2,count(5));n++) {
            double az=n*2.399963+k;
            buds.push_back({add(q,mul(sub(target,q),(n+1.)/6)),unit({std::cos(az)*.7,std::sin(az)*.7,.6}),mix(id,side*100+k*9+n),1});
          }
        }
    }
    b.budget=totalBudget;
    size_t capacity=size_t((b.budget-b.mesh.faces.size())/(lod==2?8:4)*std::clamp(p.density*seasonDensity(),0.,1.));
    size_t selected=std::min(capacity,buds.size());
    for(size_t j=0;j<selected;j++){auto& q=buds[j*buds.size()/selected];addLeaf(q.origin,q.direction,{0,0,1},q.seed,q.size,Needle);}
  }
  void grass() {
    int n=std::max(8,int(p.branches*p.clumps*detail*p.density));
    for(int i=0;i<n;i++) {
      Rng r(mix(doc.seed,i+1000));double a=i*2.399963,rad=height*.23*std::sqrt(r.real());
      V base{std::cos(a)*rad,std::sin(a)*rad,0};double h=height*r.range(.9-p.irregularity*.5,1),turn=r.range(0,2*pi);
      V aim=unit({std::cos(turn)*p.curvature*.45,std::sin(turn)*p.curvature*.45,1});
      b.leaf(base,aim,{std::cos(turn),std::sin(turn),.2},h,p.leafWidth,2,p.foliageMode,Grass,Part::Leaves,p.leafCurl+p.gravity*.22,p.leafTwist*r.range(-1,1),p.leafProfile);
      if(doc.species=="dry-grass"&&i%7==0) {
        V tip=add(base,mul(aim,h));b.leaf(tip,unit({std::cos(turn)*.2,std::sin(turn)*.2,1}),{1,0,0},p.flowerSize,.23,2,1,Wood,Part::Flowers,.1,.1,p.leafProfile);
      }
    }
  }
  void frond(V base,double az,double size,uint64_t seed,bool palm) {
    V end=add(base,{std::cos(az)*size*.87,std::sin(az)*size*.87,size*(palm?-.18:.2+p.curvature*.6)});
    auto path=arc(base,end,p.gravity,count(p.segments+6,5),seed);
    b.tube(path,std::max(.12,p.radius*(palm?.055:.18)),.02,3,Bark,Part::Twigs);
    int n=count(p.leaves,5);
    for(int j=1;j<=n;j++)for(int side:{-1,1}) {
      double t=double(j)/(n+1);auto q=path[std::min(path.size()-1,size_t(t*(path.size()-1)))];
      double lengthScale=std::pow(std::sin(pi*t),.65)*(palm?1.4:1);
      V aim=unit({std::cos(az+side*1.05),std::sin(az+side*1.05),palm?-.45:-.08});
      addLeaf(q,aim,{0,0,1},mix(seed,j*5+side),lengthScale,Leaf);
    }
  }
  void fern() {
    for(int i=0;i<p.branches;i++) {Rng r(mix(doc.seed,i+25));frond({0,0,height*.05},i*2.399963,height*r.range(.75,1.12),mix(doc.seed,i+13),false);}
  }
  void palm() {
    std::vector<V> path;for(int i=0;i<=p.segments*2;i++)path.push_back(trunk(double(i)/(p.segments*2)));
    roots({},p.radius);b.tube(path,p.radius*1.1,p.radius*.65,sides(),Bark,Part::Trunk);
    for(int i=0;i<p.branches;i++) {
      Rng r(mix(doc.seed,i+90));V top=trunk(.98);
      frond(add(top,{0,0,(i%3)*height*.018}),i*2.399963,height*p.spread*r.range(.7,1),mix(doc.seed,i),true);
    }
  }
  void flower(V center,double radius,uint64_t seed) {
    Rng r(seed);int petals=std::max(3,count(p.petals,3));
    for(int layer=0;layer<p.flowerLayers;layer++)for(int i=0;i<petals;i++) {
      double a=2*pi*(i+.47*layer)/petals+r.range(-.1,.1)*p.irregularity;
      double radiusScale=1-.10*layer;
      V aim=unit({std::cos(a),std::sin(a),(.15+layer*.15)*(1-p.bloom*.65)});
      b.leaf(add(center,{0,0,layer*radius*.045}),aim,{0,0,1},radius*radiusScale*(.3+.7*p.bloom),doc.species=="rose"?.85:.34,
        0,lod==2?1:0,Petal,Part::Flowers,doc.species=="rose"?.65:.08,.16*r.range(-1,1),p.leafProfile);
    }
    double disk=radius*(doc.species=="sunflower"?.42:.22);
    b.lathe(center,{{disk,-.03*radius},{disk,.02*radius},{disk*.6,.10*radius},{.005,.13*radius}},std::max(8,count(24)),Center,Part::Flowers,.025);
    for(int i=0;i<5;i++)b.leaf(add(center,{0,0,-.08*radius}),{std::cos(i*2.399),std::sin(i*2.399),-.45},{0,0,1},radius*.35,.3,2,1,Leaf,Part::Flowers,.05,0,p.leafProfile);
  }
  void flowers() {
    for(int i=0;i<p.clumps;i++) {
      Rng r(mix(doc.seed,i+92));double a=i*2.399963;
      V base{std::cos(a)*height*.18*std::sqrt(double(i)/p.clumps),std::sin(a)*height*.18*std::sqrt(double(i)/p.clumps),0};
      V tip=add(base,{r.range(-1,1)*height*p.curvature*.17,r.range(-1,1)*height*p.curvature*.17,height*r.range(.8,1)});
      auto path=arc(base,tip,0,count(p.segments,4),mix(doc.seed,i));b.tube(path,p.radius,p.radius*.4,std::max(4,sides()),Bark,Part::Trunk);
      for(int j=1;j<=4;j++){V q=path[std::min(path.size()-1,size_t(j*(path.size()-1)/5))];double az=j*2.399963;addLeaf(q,{std::cos(az),std::sin(az),.3},{0,0,1},mix(i,j));}
      flower(path.back(),p.flowerSize*(.6+.4*p.age),mix(doc.seed,i+43));
    }
  }
  void mushroom() {
    for(int stem=0;stem<p.clumps;stem++) {
      Rng r(mix(doc.seed,stem+49));double az=stem*2.399963,h=height*r.range(.65,1);
      V base{std::cos(az)*height*.32*std::sqrt(double(stem)/p.clumps),std::sin(az)*height*.32*std::sqrt(double(stem)/p.clumps),0};
      bool oyster=doc.species=="oyster",trumpet=doc.species=="chanterelle",ink=doc.species=="woodland";
      auto stemPath=arc(base,add(base,{oyster?h*.30:0,0,h*.72}),.02,count(p.segments,4),mix(doc.seed,stem));
      b.tube(stemPath,p.radius*(doc.species=="boletus"?1.15:.65),p.radius*.6,sides(),Gills,Part::Trunk);
      V center=stemPath.back();double radius=h*p.spread*(.4+.25*p.age),thick=radius*p.capThickness;
      std::vector<std::array<double,2>> cap;
      if(trumpet)cap={{radius*.3,-thick},{radius*.7,0},{radius,thick*.8},{radius*.92,thick*1.5},{radius*.55,0},{.02,-radius*.18}};
      else if(ink)cap={{radius,-radius*.30},{radius*.92,radius*.32},{radius*.55,radius*.8},{.002,radius*.9}};
      else cap={{radius,-thick*.12},{radius*.96,thick*.5},{radius*.75,radius*(oyster?.12:.34)},{radius*.42,radius*.5},{.002,radius*.54}};
      b.lathe(center,cap,std::max(12,count(48)),Cap,Part::Surface,.015+p.irregularity*.035,oyster?.22:0);
      if(doc.species=="boletus")b.lathe(center,{{.005,-thick*.6},{radius*.5,-thick*.5},{radius,-thick*.12}},std::max(12,count(48)),Gills,Part::Surface,.01);
      int gills=doc.species=="boletus"?0:std::max(8,int((12+48*p.gills)*detail));
      for(int i=0;i<gills;i++) {
        double a=2*pi*i/gills;V inner=add(center,{std::cos(a)*p.radius*.5,std::sin(a)*p.radius*.5,-thick*.4});
        V outer=add(center,{std::cos(a)*radius*.94,std::sin(a)*radius*.94,-thick*.1});
        b.gill(inner,outer,radius*(.04+.08*p.gills));
      }
      if(doc.species=="amanita"||doc.species=="fantasy") {
        int spots=count(22,4);
        for(int i=0;i<spots;i++) {
          double t=std::sqrt((i+.5)/spots)*.88,a=i*2.399963;
          V q=add(center,{std::cos(a)*radius*t,std::sin(a)*radius*t,radius*.54*std::sqrt(1-t*t)});
          b.leaf(q,{std::cos(a),std::sin(a),.1},{0,0,1},radius*.12*r.range(.6,1.4),.9,1,1,Gills,Part::Surface,.15,.05,p.leafProfile);
        }
      }
    }
  }
  void cactus() {
    bool barrel=doc.species=="barrel-cactus";
    auto stalk=[&](std::vector<V> path,double radius,uint64_t id) {
      b.tube(path,radius,radius*.10,std::max(8,int(p.ribs)*2),CactusSkin,Part::Trunk,p.ribs,.78);
      int rings=count(14,4),around=std::max(3,count(int(p.ribs)));
      for(int k=1;k<rings;k++)for(int j=0;j<around;j++)if(p.spines>0) {
        double t=double(k)/rings,a=j*2*pi/around;V q=path[std::min(path.size()-1,size_t(t*(path.size()-1)))];
        V radial{std::cos(a),std::sin(a),.1};q=add(q,mul(radial,radius*(1-t*.65)));
        b.leaf(q,radial,{0,0,1},radius*p.spines*.45,.035,2,1,Wood,Part::Leaves,0,0,p.leafProfile);
      }
      if(p.bloom>.5)flower(add(path.back(),{0,0,radius*.1}),p.flowerSize,mix(id,32));
    };
    if(barrel) {
      std::vector<std::array<double,2>> profile;
      for(int i=0;i<=p.segments*2;i++){double t=double(i)/(p.segments*2);profile.push_back({std::max(.01,p.radius*(p.spread/.72)*std::pow(std::sin(pi*t),.40)),height*t});}
      b.lathe({},profile,std::max(12,int(p.ribs)*2),CactusSkin,Part::Trunk,.04+p.irregularity*.18);
      for(int i=0;i<count(100,12);i++) {double t=(i+.5)/count(100,12),a=i*2.399963;double r=p.radius*std::pow(std::sin(pi*t),.40);
        b.leaf({std::cos(a)*r,std::sin(a)*r,height*t},{std::cos(a),std::sin(a),.3},{0,0,1},p.radius*p.spines*.28,.035,2,1,Wood,Part::Leaves,0,0,p.leafProfile);}
      flower({0,0,height},p.flowerSize,doc.seed);
    }else {
      std::vector<V> path;for(int i=0;i<=p.segments;i++)path.push_back(trunk(double(i)/p.segments));stalk(path,p.radius,doc.seed);
      for(int i=0;i<std::min(6,p.branches);i++) {
        double t=.35+.40*(i+.5)/p.branches,a=i*2.399963;V base=trunk(t),out=add(base,{std::cos(a)*height*.20*p.spread,std::sin(a)*height*.20*p.spread,height*.07});
        auto end=add(out,{0,0,height*.27});std::vector<V> arm;
        auto c1=add(base,{std::cos(a)*height*.25,std::sin(a)*height*.25,0});
        auto c2=sub(end,{0,0,height*.25});int n=count(12,6);
        for(int j=0;j<=n;j++)arm.push_back(bezier(base,c1,c2,end,double(j)/n));
        stalk(arm,p.radius*.62,mix(doc.seed,i+1));
      }
    }
  }
  void bamboo() {
    bool reed=doc.species=="reeds";
    for(int i=0;i<p.clumps;i++) {
      Rng r(mix(doc.seed,i+69));double a=i*2.399963;V base{std::cos(a)*height*.16,std::sin(a)*height*.16,0};
      double h=height*r.range(.95-p.irregularity*.7,1);int nodes=p.branches;V last=base;
      for(int j=0;j<nodes;j++) {
        double t=double(j+1)/nodes;V tip=add(base,{std::cos(a)*h*p.curvature*.16*t*t,std::sin(a)*h*p.curvature*.16*t*t,h*t});
        b.tube({last,tip},p.radius*(1-.4*t),p.radius*(1-.45*t),sides(),Bark,Part::Trunk);
        b.lathe(tip,{{p.radius*.9,-p.radius*.2},{p.radius*1.13,0},{p.radius*.86,p.radius*.25}},sides(),Wood,Part::Primary);
        double az=j*2.399963+a;
        if(j>nodes/4) {
          V aim=unit({std::cos(az),std::sin(az),.3});addLeaf(tip,aim,{0,0,1},mix(i,j),reed?1.1:.8);
          if(!reed)for(int k=0;k<2;k++)addLeaf(add(tip,mul(aim,p.leafSize*.45)),{std::cos(az+k*.6),std::sin(az+k*.6),-.05},{0,0,1},mix(j,k+11),.6);
        }last=tip;
      }
      if(reed)for(int j=0;j<count(8,3);j++){double az=j*2.399963;addLeaf(last,{std::cos(az)*.15,std::sin(az)*.15,1},{1,0,0},mix(i,j),.4,Wood,Part::Flowers);}
    }
  }
  void vine() {
    std::vector<V> path;int n=std::max(12,p.leaves);
    for(int i=0;i<=n;i++){double t=double(i)/n;V q=trunk(t);q[0]+=height*p.spread*.20*std::sin(t*pi*3);q[1]+=height*p.spread*.15*std::cos(t*pi*2)+height*p.irregularity*.04*std::sin(t*13);path.push_back(q);}
    b.tube(path,p.radius,p.radius*.25,sides(),Bark,Part::Trunk);
    for(int i=1;i<n;i++) {
      double a=i*2.399963;V aim=unit({std::cos(a),std::sin(a),-.18});
      auto q=path[i],tip=add(q,mul(aim,p.leafSize*.32));b.tube({q,tip},p.radius*.25,.03,3,Bark,Part::Twigs);
      if(i%std::max(1,lod+1)==0)addLeaf(tip,aim,{0,0,1},mix(doc.seed,i));
    }
  }
  Mesh runBase() {
    switch(doc.family) {
      case Family::Broadleaf:case Family::Custom:broadleaf();break;
      case Family::Bush:broadleaf(true);break;case Family::Conifer:conifer();break;
      case Family::Grass:grass();break;case Family::Fern:fern();break;case Family::Flower:flowers();break;
      case Family::Mushroom:mushroom();break;case Family::Cactus:cactus();break;case Family::Palm:palm();break;
      case Family::Bamboo:bamboo();break;case Family::Vine:vine();break;
    }
    b.mesh.bounds();validate(b.mesh);return std::move(b.mesh);
  }
};
inline Mesh generateBase(const Project& d,int lod=0,const std::atomic<bool>* cancel=nullptr) {
  validate(d);if(lod<0||lod>2)throw std::runtime_error("Invalid plant LOD");return Generator(d,lod,cancel).runBase();
}
inline Mesh generate(const Project& d,int lod=0,const std::atomic<bool>* cancel=nullptr) {
  auto result=hierarchy(generateBase(d,lod,cancel),d);
  if(result.faces.empty()){result.low=result.high={0,0,0};return result;}
  validate(result);return result;
}
}
