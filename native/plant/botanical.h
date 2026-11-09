// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "botanical_shapes.h"
// Included after the generator-1 implementation. The old generator stays intact
// so existing editable sources reproduce their original geometry on opening.
namespace ftebroom::plant {
struct Botanical : Generator {
  struct Span {std::vector<V> path;double radius,tip;Part part;uint64_t id;};
  struct Shoot {V p,aim;uint64_t id;double size;};
  std::vector<Span> skeleton;
  std::vector<Shoot> foliage;
  Botanical(const Project& d,int l,const std::atomic<bool>* c):Generator(d,l,c){}
  bool compact() const {return doc.generator>=3&&p.quality!=3;}
  double leafAngle(int i) const {
    if(p.phyllotaxy==1)return (i%2)*pi+(i/2)*.27;
    if(p.phyllotaxy==2)return (i%p.clusterLeaves)*2*pi/p.clusterLeaves+(i/p.clusterLeaves)*.19;
    return i*2.399963;
  }
  double phenology() const {
    if(p.evergreen==1)return 1;
    if(p.evergreen==2)return p.season==3?.55:1;
    if(p.evergreen==3)return p.season==3?.55:1;
    return p.season==3?.015:p.season==2?.70:p.season==0?.78:1;
  }
  double envelope(double t) const {
    t=std::clamp(t,0.,1.);
    switch(p.crown){
      case 2:return .58+.42*std::sin(pi*t);
      case 4:return .8+.2*std::sin(pi*t);
      case 5:return std::pow(1-t,.75)*1.25+.05;
      case 6:return .35+.65*std::sin(pi*t*.7);
      case 8:return .65+.35*std::abs(std::cos(t*pi*3));
      default:return .25+.75*std::pow(std::max(0.,std::sin(pi*t)),.55);
    }
  }
  int foliageMaterial(uint64_t id,bool needle=false) const {
    static const int leaves[]={Leaf,LeafLight,LeafMature,LeafYoung};
    static const int needles[]={Needle,NeedleLight,NeedleMature};
    return needle?needles[mix(id,doc.seed)%3]:leaves[mix(id,doc.seed)%4];
  }
  void lamina(V origin,V direction,V up,double size,double width,uint64_t id,
              int shape=-1,int material=-1,Part part=Part::Leaves) {
    b.check();Rng r(mix(id,doc.seed));direction=unit(direction);
    if(length(cross(direction,up))<.01)up={0,1,0};
    shape=shape<0?p.leafShape:shape;material=material<0?foliageMaterial(id):material;
    const int mode=lod==2?2:p.foliageMode>=2?p.foliageMode:p.quality==2?1:lod==1?1:p.foliageMode;
    // A grass blade defines plant height, unlike a leaf at the tip of a twig.
    // Compensate sparse distant blades sideways, never by growing the plant.
    bool bladeHeight=doc.family==Family::Grass||material==Grass;
    double scale=lod==2?1.85:lod==1?1.18:1;
    if(bladeHeight){width*=std::min(1.65,scale);scale=1;}
    size*=scale*r.range(.86,1.13);
    auto first=b.mesh.vertices.size();
    if(material==Needle||material==NeedleLight||material==NeedleMature){
      b.leaf(origin,direction,up,size,std::max(.65,width),6,lod==2?3:1,material,part,p.leafCurl*.35,p.leafTwist*.2,{.05,.85,1.,.80,.1});
    }else if(shape==4&&mode<2&&!compact()){
      int lobes=std::clamp(p.leaflets,3,9),n=lobes*4+2;
      if(!b.room(n))return;b.organ++;
      V right=unit(cross(direction,up)),normal=unit(cross(right,direction));
      V center=add(origin,mul(direction,size*.40));
      unsigned pivot=b.vertex(center,normal,.5,.40,material,.4);
      for(int i=0;i<=n;i++){
        auto uv=botanicalPalmatePoint(i,lobes);double u=uv[0]-.5,v=uv[1];
        auto q=add(origin,add(mul(right,u*size*width),mul(direction,v*size)));
        q=add(q,mul(normal,size*(p.leafCurl*.28*v*v+.045*std::abs(u))));
        b.vertex(q,normal,uv[0],uv[1],material,v);
      }
      for(int i=0;i<n;i++)b.face(pivot,pivot+i+2,pivot+i+1,material,part);
    }else if(mode>=2){
      b.leaf(origin,direction,up,size,width,shape,mode,material,part,p.leafCurl*.45,p.leafTwist*r.range(-.6,.6),p.leafProfile);
    }else {
      int steps=compact()?(lod?3:4):lod==1?3:mode==1?4:8,columns=compact()||mode==1?1:2;
      if(!b.room(steps*columns*2))return;b.organ++;
      auto first=b.mesh.vertices.size();V right=unit(cross(direction,up)),normal=unit(cross(right,direction));
      double curl=(p.leafCurl+r.range(-.06,.06)*p.irregularity)*.48,twist=p.leafTwist*r.range(-.6,.6),asym=r.range(-.055,.055)*p.irregularity;
      for(int i=0;i<=steps;i++){
        double t=double(i)/steps,profile=botanicalLeafWidth(shape,t,p.leafProfile,p.serration);
        // A loose contour follows the selected morphology without clipping
        // the finer lobes/teeth between compact ribbon rows.
        double w=(compact()||mode==1)?.80+.20*std::clamp(profile,0.,1.):std::max(.005,profile);
        auto center=add(origin,add(mul(direction,size*t),mul(normal,size*curl*t*t)));
        center=add(center,mul(right,size*asym*std::sin(pi*t)));
        for(int k=0;k<=columns;k++){
          double j=2.*k/columns-1;
          double lateral=size*width*.5*w*j*(1+(j<0?-.035:.035)*p.irregularity);
          auto q=add(center,add(mul(right,lateral),mul(normal,-std::abs(lateral)*.11+twist*lateral*t)));
          // A central fold and gently rolled margin give the lamina volume.
          auto n=unit(add(sub(normal,mul(direction,2*curl*t)),mul(right,j*.11-twist*t)));
          b.vertex(q,n,(j*w+1)*.5,t,material,t);
        }
      }
      for(int i=0;i<steps;i++)for(int j=0;j<columns;j++){unsigned a=first+i*(columns+1)+j,c=a+columns+1;b.quad(a,a+1,c+1,c,material,part);}
    }
    // Keep geometric normals in the same hemisphere. Mild canopy smoothing
    // avoids black paper-like undersides without painting fake vertex shadows.
    for(size_t i=first;i<b.mesh.vertices.size();i++){
      auto& v=b.mesh.vertices[i];auto sky=V{0,0,v.n[2]>=0?1.:-1.};
      v.n=unit(add(mul(v.n,.84),mul(sky,.16)));
    }
  }
  void blade(V origin,V aim,double size,uint64_t id,double weight=1) {
    auto material=foliageMaterial(id);Rng r(mix(id,doc.seed));
    if(p.leafShape!=7){lamina(origin,aim,{0,0,1},size,p.leafWidth,id,-1,material);return;}
    // A compound leaf is one connected rachis with actual separate leaflets.
    int leaflets=std::max(3,p.leaflets-(lod==2?p.leaflets/3:0));
    V axis=unit(aim),right=unit(cross(axis,V{0,0,1}));if(length(right)<.01)right={1,0,0};
    if((doc.family==Family::Broadleaf&&p.organ==2)||(doc.family==Family::Flower&&(p.organ==8||p.organ==11))||(doc.family==Family::Vine&&p.organ==1)){
      auto node=add(origin,mul(axis,size*.18));
      b.tube({origin,node},size*.012,.01,3,Bark,Part::Twigs);
      for(int i=0;i<leaflets;i++){
        double a=(i-(leaflets-1)*.5)*pi/(leaflets+1);
        V dir=unit(add(mul(axis,std::cos(a)),mul(right,std::sin(a))));
        lamina(node,dir,{0,0,1},size*(.65-.17*std::abs(a)),p.leafWidth,mix(id,i+1),2,material);
      }
    }else{
      auto end=add(origin,mul(axis,size*.86));b.tube({origin,end},size*.009,.008,3,Bark,Part::Twigs);
      int pairs=(leaflets-1)/2;
      for(int i=0;i<pairs;i++)for(int side:{-1,1}){
        double t=.16+.65*(i+.5)/pairs;
        V node=add(origin,mul(axis,size*t)),dir=unit(add(mul(axis,.30),mul(right,side)));
        double leaflet=size*(.34-.12*t);
        if(doc.family==Family::Broadleaf&&p.organ==3){
          auto finish=add(node,mul(dir,leaflet));b.tube({node,finish},size*.004,.002,3,Bark,Part::Twigs);
          for(int k=0;k<4;k++)for(int s:{-1,1})lamina(add(node,mul(dir,leaflet*(k+.3)/4)),unit(add(mul(dir,.3),mul(axis,s))),{0,0,1},leaflet*.35,.25,mix(id,i*21+k*2+s+9),2,material);
        }else lamina(node,dir,{0,0,1},leaflet,p.leafWidth,mix(id,i*3+side+5),2,material);
      }
      lamina(end,axis,{0,0,1},size*.29,p.leafWidth,mix(id,97),2,material);
    }
  }
  size_t leafCost() const {
    size_t cost=lod==2?4:p.foliageMode>=2?4:lod==1?6:p.foliageMode==1?8:32;
    if(compact())cost=lod==2?4:lod==1?6:8;
    else if(p.leafShape==4&&p.foliageMode<2&&lod!=2)cost=std::clamp(p.leaflets,3,9)*4+2;
    if(p.leafShape==7)cost=cost*p.leaflets*(doc.family==Family::Broadleaf&&p.organ==3?4:1)+24;
    return cost;
  }
  void planBranch(V origin,V end,double radius,int order,uint64_t id) {
    b.check();if(skeleton.size()>16384||foliage.size()>262144)throw std::runtime_error("Botanical plan exceeds the preview limit; reduce stems, branching depth or leaves per shoot");Rng r(mix(id,doc.seed));
    double droop=p.gravity*(p.crown==3?1.1:.50);
    auto path=arc(origin,end,droop,std::max(4,p.segments),id);
    skeleton.push_back({path,radius,std::max(.015,radius*(1-p.taper)*.45),order==0?Part::Primary:order==1?Part::Secondary:Part::Twigs,id});
    double span=length(sub(end,origin));int depth=doc.family==Family::Bush?std::min(3,p.depth):p.depth;
    if(order+1<depth){
      int children=order==0?4:3;
      for(int i=0;i<children;i++){
        double t=.36+.59*(i+.5)/children;
        auto at=std::min(path.size()-2,size_t(t*(path.size()-1)));V anchor=path[at],axis=unit(sub(path.back(),anchor));
        V right=unit(cross(axis,V{0,0,1}));if(length(right)<.01)right={1,0,0};V up=unit(cross(right,axis));
        double a=i*2.399963+r.range(-.25,.25)*p.irregularity;
        double angle=p.branchAngle*pi/180*(order==0?.65:.8);
        auto aim=unit(add(mul(axis,std::cos(angle)),mul(add(mul(right,std::cos(a)),mul(up,std::sin(a))),std::sin(angle))));
        aim[2]+=p.light*.12;if(p.crown==3)aim[2]-=.25*(order+1);
        auto target=add(anchor,mul(unit(aim),span*p.branchLength*r.range(.55,.83)));
        planBranch(anchor,target,radius*p.branchTaper*.72,order+1,mix(id,i+11));
      }
    }else if(r.real()>=p.dead){
      int leaves=std::max(4,int(p.leaves*p.density*p.crownFill*(p.crown==3?1.6:1.)));
      std::vector<V> drape;
      if(p.crown==3){auto anchor=path.back();auto tip=add(anchor,{r.range(-.04,.04)*height,r.range(-.04,.04)*height,-height*r.range(.18,.34)*std::max(.3,p.gravity)});drape=arc(anchor,tip,.12,std::max(4,p.segments),mix(id,907));skeleton.push_back({drape,std::max(.03,radius*.6),.012,Part::Twigs,mix(id,908)});}
      for(int i=0;i<leaves;i++){
        auto& shoot=!drape.empty()&&i>=leaves/4?drape:path;
        double t=!drape.empty()&&i>=leaves/4?double(i-leaves/4+.5)/(leaves-leaves/4):.24+.75*(i+.5)/leaves;
        auto at=std::min(shoot.size()-2,size_t(t*(shoot.size()-1)));
        double f=t*(shoot.size()-1)-at;V anchor=add(shoot[at],mul(sub(shoot[at+1],shoot[at]),f));
        V axis=unit(sub(shoot[at+1],shoot[at]));double az=leafAngle(i);
        az+=r.range(-.25,.25)*p.irregularity;V radial=unit({std::cos(az),std::sin(az),p.crown==3?r.range(-1.8,-.5):r.range(-.35,1.0)});
        foliage.push_back({anchor,unit(add(mul(radial,.75),mul(axis,.25))),mix(id,i+200),r.range(.75,1.2)});
      }
    }
  }
  void emitSkeleton() {
    // Plan the entire canopy before spending triangles. Reduce every branch's
    // tessellation together; never discard late limbs by traversal order.
    int segments=std::max(1,count(p.segments,1)),radial=std::max(3,sides());
    size_t allowance=std::max<size_t>(100,b.budget*.31);
    auto rings=[&](Part part){return std::max(3,int(radial*(part==Part::Twigs?.35:part==Part::Secondary?.5:part==Part::Primary?.72:1.)));};
    auto steps=[&](Part part){return std::max(1,int(segments*(part==Part::Twigs?.27:part==Part::Secondary?.45:part==Part::Primary?.65:1.)));};
    auto estimate=[&](){size_t sum=0;for(auto& span:skeleton){if(span.part==Part::Twigs&&(lod>0||compact()))sum+=doc.family==Family::Conifer&&lod==2?0:2;else sum+=size_t(std::min<int>(steps(span.part),span.path.size()-1)+1)*rings(span.part)*2;}return sum;};
    while(estimate()>allowance&&(segments>1||radial>3)){if(segments>2)--segments;else if(radial>3)--radial;else --segments;}
    const auto fullBudget=b.budget;
    if(compact()){
      // Keep structural branches ahead of ribbons and leave space for foliage,
      // including when a small distant LOD exhausts its skeleton allowance.
      std::stable_sort(skeleton.begin(),skeleton.end(),[](const Span& a,const Span& c){
        auto rank=[](Part part){return part==Part::Trunk?0:part==Part::Roots?1:int(part);};
        return rank(a.part)<rank(c.part);
      });
      b.budget=std::max<size_t>(fullBudget/2,b.mesh.faces.size());
    }
    for(auto& span:skeleton){
      if(span.part==Part::Twigs&&(lod>0||compact())){
        if(doc.family==Family::Conifer&&lod==2)continue; // needle cards carry their shoot axis
        auto from=span.path.front(),to=span.path.back(),axis=unit(sub(to,from));V side=unit(cross(axis,V{0,0,1}));if(length(side)<.01)side={1,0,0};side=mul(side,std::max(.02,span.radius));auto normal=unit(cross(side,axis));
        if(b.room(2)){b.organ++;auto a=b.vertex(sub(from,side),normal,0,0,Bark),c=b.vertex(add(from,side),normal,1,0,Bark),d=b.vertex(add(to,mul(side,.12)),normal,1,1,Bark),e=b.vertex(sub(to,mul(side,.12)),normal,0,1,Bark);b.quad(a,c,d,e,Bark,Part::Twigs);}continue;
      }
      int n=std::max(1,std::min<int>(steps(span.part),span.path.size()-1));std::vector<V> path;
      for(int i=0;i<=n;i++)path.push_back(span.path[size_t(double(i)*(span.path.size()-1)/n)]);
      b.tube(path,span.radius,span.tip,rings(span.part),Bark,span.part);
    }
    b.budget=fullBudget;
  }
  void emitFoliage(bool needles=false) {
    // Stable spatial sampling covers the whole planned crown at all LODs.
    std::stable_sort(foliage.begin(),foliage.end(),[](auto& a,auto& b){return mix(a.id,13)<mix(b.id,13);});
    auto photo=foliagePhoto(doc);
    if(photo==Photo::Oak||photo==Photo::Spruce||photo==Photo::Maple||photo==Photo::Birch||photo==Photo::Pine){
      int strips=lod?1:2;
      size_t groups=(foliage.size()+(lod?5:3))/(lod?6:4);
      size_t count=std::min((b.budget-b.mesh.faces.size())/(strips*2),size_t(groups*phenology()));
      for(size_t i=0;i<count;i++){
        auto& shoot=foliage[i*foliage.size()/count];
        Rng rng(mix(shoot.id,doc.seed));
        V up=unit({rng.range(-.85,.85),rng.range(-.85,.85),rng.range(.5,1.)});
        double size=p.leafSize*(.5+.5*p.age)*shoot.size*(needles?1.65:2.15)*(lod==2?1.30:1.);
        double width=size*(needles?.70:.74)*std::clamp(std::sqrt(p.leafWidth/.45),.60,1.50);
        b.foliageCard(shoot.p,shoot.aim,up,size,width,p.leafCurl*.45,strips);
      }
      return;
    }
    auto capacity=(b.budget-b.mesh.faces.size())/std::max<size_t>(1,needles?8:leafCost()+2);
    size_t survivors=std::min(capacity,foliage.size()),count=size_t(survivors*std::min(1.,phenology()));
    double compensation=survivors?std::clamp(std::sqrt(double(foliage.size())/survivors)/(lod==2?1.85:lod==1?1.18:1.),1.,lod?1.65:1.35):1.;
    for(size_t i=0;i<count;i++){
      auto& s=foliage[i*foliage.size()/count];double size=p.leafSize*(.5+.5*p.age)*s.size*(needles?1.:1.20)*compensation;
      if(needles){Rng r(mix(s.id,doc.seed));V tilt=unit({r.range(-.65,.65),r.range(-.65,.65),1});lamina(s.p,s.aim,tilt,size,p.leafWidth,s.id,5,foliageMaterial(s.id,true));continue;}
      V end=add(s.p,mul(s.aim,size*p.petiole));
      V side=unit(cross(s.aim,V{0,0,1}));if(length(side)<.01)side={1,0,0};side=mul(side,std::max(.015,size*.005));
      if(b.room(2)){b.organ++;auto a=b.vertex(sub(s.p,side),{0,0,1},.2,.2,Bark),c=b.vertex(add(s.p,side),{0,0,1},.3,.2,Bark),d=b.vertex(add(end,side),{0,0,1},.3,.3,Bark),e=b.vertex(sub(end,side),{0,0,1},.2,.3,Bark);b.quad(a,c,d,e,Bark,Part::Twigs);}
      blade(end,s.aim,size,s.id);
    }
  }
  void woody(bool shrub=false) {
    int stems=std::max(1,p.clumps),primaries=std::max(3,p.branches);
    for(int stem=0;stem<stems;stem++){
      double a=stem*2.399963;V offset=stems>1?V{std::cos(a)*height*.09,std::sin(a)*height*.09,0}:V{};
      if(shrub&&p.organ==5)offset={height*(stem-(stems-1)*.5)*.22,0,0};
      auto origin=trunk(0,offset);std::vector<V> path;for(int j=0;j<=p.segments;j++)path.push_back(trunk(double(j)/p.segments,offset));
      skeleton.push_back({path,p.radius*(1+p.rootFlare*.20),p.radius*(1-p.taper)*.35,Part::Trunk,mix(doc.seed,stem)});
      for(int k=0;k<5;k++){
        double az=2*pi*k/5+a;auto from=add(origin,{std::cos(az)*p.radius*.3,std::sin(az)*p.radius*.3,p.radius*1.4});
        auto to=add(origin,{std::cos(az)*p.radius*(1.5+p.rootFlare),std::sin(az)*p.radius*(1.5+p.rootFlare),0});
        skeleton.push_back({arc(from,to,0,3,mix(stem,k)),p.radius*.38,.025,Part::Roots,mix(stem,k+19)});
      }
      for(int i=0;i<primaries;i++){
        b.check();Rng r(mix(doc.seed,stem*71+i+13));double u=(i+.5)/primaries;
        double t=p.crownBase+(1-p.crownBase)*u*.9;
        double az=i*2.399963+stem+r.range(-.32,.32)*p.irregularity;
        double reach=height*p.spread*envelope(u)*(.32+.48*curve(p.growth,t))*r.range(.88,1.1);
        double angle=p.branchAngle*pi/180;
        double top=p.crown==4?.88+u*.08:p.crown==3?.84+u*.10:std::min(1.02,t+reach/height*std::cos(angle)*.55);
        V end=add(offset,{std::cos(az)*reach*std::sin(angle),std::sin(az)*reach*std::sin(angle),height*top});
        end[2]=height*(p.crownBase+(top-p.crownBase)*p.crownAspect)+offset[2];
        planBranch(trunk(t,offset),end,p.radius*.38*(1-t*.65),0,mix(stem+7,i+100));
      }
    }
    emitSkeleton();auto total=b.budget;if(shrub&&p.organ>=4)b.budget=size_t(total*.88);emitFoliage();b.budget=total;
    if(shrub&&p.organ>=4&&p.organ<=10)inflorescences();
  }
  void evergreenTree() {
    auto origin=trunk(0);std::vector<V> trunkPath;for(int i=0;i<=p.segments;i++)trunkPath.push_back(trunk(double(i)/p.segments));
    skeleton.push_back({trunkPath,p.radius,.035,Part::Trunk,doc.seed});
    int levels=p.branches,per=p.needleForm==0?4:5;
    for(int level=0;level<levels;level++)for(int j=0;j<per;j++){
      b.check();uint64_t id=mix(doc.seed,level*31+j+500);Rng r(id);
      double u=(level+r.range(.18,.65))/levels,t=p.crownBase+(1-p.crownBase)*u;
      double profile=p.crown==5?std::pow(1-u,.9):p.crown==2?.24+.16*std::sin(u*pi):p.crown==8?.75:envelope(u);
      double span=height*p.spread*profile*r.range(.82,1.08);
      double az=j*2*pi/per+level*1.73+r.range(-.16,.16)*p.irregularity;
      V base=trunk(t),end=add(base,{std::cos(az)*span,std::sin(az)*span,span*(p.needleForm==0?.30:.12)});
      auto path=arc(base,end,p.gravity,std::max(4,p.segments),id);
      skeleton.push_back({path,p.radius*.25*(1-u*.8),.025,Part::Primary,id});
      int shoots=std::max(4,p.leaves/2);
      for(int k=0;k<shoots;k++)for(int side:{-1,1}){
        double v=.16+.80*(k+.5)/shoots;auto node=path[std::min(path.size()-1,size_t(v*(path.size()-1)))];
        V dir=unit({std::cos(az+side*.72),std::sin(az+side*.72),p.needleForm==1?-.30:.22});
        auto tip=add(node,mul(dir,span*.32*(1-v*.7)));
        if(k%3==0)skeleton.push_back({{node,tip},std::max(.025,p.radius*.025),.01,Part::Twigs,mix(id,k+side+17)});
        for(int n=0;n<3;n++){
          double f=(n+.2)/3;auto at=add(node,mul(sub(tip,node),f));
          double turn=leafAngle(n)+az;
          V aim=p.needleForm==0?unit(add(dir,V{std::cos(turn)*.45,std::sin(turn)*.45,.45})):p.needleForm==3?unit({std::cos(turn),std::sin(turn),.4}):unit(add(dir,V{std::cos(turn)*.14,std::sin(turn)*.14,0}));
          foliage.push_back({at,aim,mix(id,k*21+n+side+100),std::max(1.8,length(sub(tip,node))/p.leafSize*1.5)});
        }
      }
    }
    emitSkeleton();emitFoliage(true);
  }
  // Family-specific organ generators are defined below, independently of IDs.
  void inflorescences();
  void herbs();void ferns();void palms();void blossoms();void fungi();
  void succulents();void reedsAndBamboo();void climbers();
  Mesh run() {
    switch(doc.family){
      case Family::Broadleaf:case Family::Custom:woody();break;
      case Family::Bush:woody(true);break;case Family::Conifer:evergreenTree();break;
      case Family::Grass:herbs();break;case Family::Fern:ferns();break;
      case Family::Palm:palms();break;case Family::Flower:blossoms();break;
      case Family::Mushroom:fungi();break;case Family::Cactus:succulents();break;
      case Family::Bamboo:case Family::Riparian:reedsAndBamboo();break;case Family::Vine:climbers();break;
    }
    // All generators use the same outward-normal convention, including organ
    // profiles that fold back on themselves (trumpets, funnels and fleshy leaves).
    for(auto& face:b.mesh.faces){auto& a=b.mesh.vertices[face.i[0]];auto& c=b.mesh.vertices[face.i[1]];auto& d=b.mesh.vertices[face.i[2]];
      if(dot(cross(sub(c.p,a.p),sub(d.p,a.p)),add(add(a.n,c.n),d.n))<0)std::swap(face.i[1],face.i[2]);}
    b.mesh.bounds();validate(b.mesh);return std::move(b.mesh);
  }
};
}
