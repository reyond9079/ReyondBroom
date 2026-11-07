// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace ftebroom::plant {
inline void fanOrgan(Builder& b,V base,V aim,double size,int fingers,int mat,Part part) {
  aim=unit(aim);V right=unit(cross(aim,V{0,0,1}));if(length(right)<.01)right={1,0,0};V up=unit(cross(right,aim));
  fingers=std::max(5,fingers);
  for(int i=0;i<fingers;i++){
    double a=-1.15+2.3*i/fingers,c=-1.15+2.3*(i+1)/fingers;
    V d1=unit(add(mul(aim,std::cos(a)),mul(right,std::sin(a)))),d2=unit(add(mul(aim,std::cos(c)),mul(right,std::sin(c))));
    V q=add(base,mul(add(d1,d2),size*.31));V tip=add(base,add(mul(unit(add(d1,d2)),size),mul(up,-size*.10)));
    if(!b.room(4))return;b.organ++;
    auto center=b.vertex(base,up,.5,.03,mat),l=b.vertex(add(base,mul(d1,size*.65)),up,(i+.1)/fingers,.65,mat,.6);
    auto r=b.vertex(add(base,mul(d2,size*.65)),up,(i+.9)/fingers,.65,mat,.6),t=b.vertex(tip,up,(i+.5)/fingers,.98,mat,1);
    b.face(center,r,l,mat,part);b.face(l,r,t,mat,part);
  }
}
inline void Botanical::inflorescences() {
  if(p.bloom<.15||foliage.empty())return;
  int n=std::min<int>(20,foliage.size()/12+1);
  for(int i=0;i<n;i++){
    auto& at=foliage[size_t(i)*foliage.size()/n];
    if(p.organ==6){
      b.tube({at.p,add(at.p,mul(at.aim,p.leafSize*.65))},.12,.002,3,Wood,Part::Twigs);continue;
    }
    if(p.organ==10){
      for(int k=0;k<4;k++){auto q=add(at.p,{std::cos(k*2.4)*p.flowerSize*.3,std::sin(k*2.4)*p.flowerSize*.3,k*p.flowerSize*.12});
        b.lathe(q,{{.002,-p.flowerSize*.3},{p.flowerSize*.3,0},{.002,p.flowerSize*.3}},6,Center,Part::Flowers);}
      continue;
    }
    int flowers=p.organ==8?12:p.organ==7?9:4;
    for(int k=0;k<flowers;k++){
      double t=(k+.5)/flowers,a=k*2.399963;
      auto q=add(at.p,{std::cos(a)*p.flowerSize*.42,std::sin(a)*p.flowerSize*.42,p.organ==7?t*p.flowerSize:p.flowerSize*.22*std::sin(t*pi)});
      for(int petal=0;petal<5;petal++){double turn=petal*2*pi/5;
        lamina(q,{std::cos(turn),std::sin(turn),.25},{0,0,1},p.flowerSize*(p.organ==4?.50:.22),.65,mix(at.id,k*7+petal),0,Petal,Part::Flowers);}
    }
  }
}
inline void Botanical::herbs() {
  int blades=std::max(12,int(p.branches*p.clumps*(lod==0?1:lod==1?.7:.42)*p.density));
  for(int i=0;i<blades;i++){
    b.check();Rng r(mix(doc.seed,i+1000));double az=i*2.399963,rad=height*(p.organ==1?.55:.20)*std::sqrt(r.real());
    V base{std::cos(az)*rad,std::sin(az)*rad,0};double h=height*r.range(.62,1),a=r.range(0,2*pi);
    V dir=unit({std::cos(a)*p.curvature*.65,std::sin(a)*p.curvature*.65,1});
    lamina(base,dir,{std::cos(a),std::sin(a),.15},h,p.leafWidth,mix(doc.seed,i+41),2,Grass);
    if(p.organ==0||p.organ==1||p.organ==2||p.organ==9||i%std::max(3,blades/14)!=0)continue;
    V tip=add(base,mul(dir,h*.96));auto stem=arc(base,tip,p.gravity*.1,4,mix(doc.seed,i));
    b.tube(stem,.18,.055,3,Bark,Part::Trunk);
    if(p.organ==8||p.organ==10){
      double length=p.flowerSize*(p.organ==10?1.7:1);
      b.lathe(tip,{{.05,0},{length*.075,length*.15},{length*.075,length*.85},{.01,length}},lod?5:8,Wood,Part::Flowers,.12);
      for(int k=0;k<(lod?7:16);k++){
        double turn=k*2.4;auto q=add(tip,{0,0,length*(k+.2)/(lod?7:16)});
        lamina(q,{std::cos(turn)*.4,std::sin(turn)*.4,1},{0,1,0},length*.45,.015,mix(i,k),2,Wood,Part::Flowers);
      }
    }else{
      int plumes=lod?7:14;double length=p.flowerSize*(p.organ==5?2.3:1.3);
      for(int k=0;k<plumes;k++){
        double t=(k+.5)/plumes,turn=k*2.399963;
        V q=add(tip,{0,0,length*t}),aim=unit({std::cos(turn)*.6,std::sin(turn)*.6,.5});
        double span=length*(p.organ==3?.22:p.organ==7?.55:.35)*std::sin(pi*t);
        auto end=add(q,mul(aim,span));b.tube({q,end},.09,.01,3,Wood,Part::Flowers);
        for(int n=0;n<(lod?2:4);n++)lamina(add(q,mul(sub(end,q),(n+.4)/4)),aim,{0,0,1},length*(p.organ==3?.65:.18),.06,mix(i,k*9+n),2,Wood,Part::Flowers);
      }
    }
  }
}
inline void Botanical::ferns() {
  bool tree=p.organ==7;V crown{0,0,tree?height*.65:0};
  if(tree){std::vector<V> stem;for(int i=0;i<=p.segments;i++)stem.push_back({0,0,crown[2]*i/p.segments});b.tube(stem,p.radius,p.radius*.72,sides(),Bark,Part::Trunk);}
  for(int i=0;i<p.branches;i++){
    b.check();Rng r(mix(doc.seed,i+42));double a=i*2.399963,length=height*(tree?.60:.94)*r.range(.78,1.1);
    V base=add(crown,{std::cos(a)*height*.025,std::sin(a)*height*.025,0});
    auto tip=add(base,{std::cos(a)*length*(p.organ==1?.4:.75),std::sin(a)*length*(p.organ==1?.4:.75),length*(p.organ==1?.8:.28)});
    auto path=arc(base,tip,p.gravity,std::max(5,count(p.segments+4,4)),mix(doc.seed,i));
    b.tube(path,tree?.6:p.radius*.3,.025,std::max(3,sides()-2),Bark,Part::Twigs);
    if(p.organ==5){lamina(base,unit(sub(tip,base)),{0,0,1},length,.26,mix(doc.seed,i),2,foliageMaterial(i));continue;}
    int pinnae=std::max(6,int(p.leaves*(lod==2?.55:1)));
    for(int j=0;j<pinnae;j++)for(int side:{-1,1}){
      double t=.11+.85*(j+.5)/pinnae;auto q=path[std::min(path.size()-2,size_t(t*(path.size()-1)))];
      V dir=unit({std::cos(a+side*1.05),std::sin(a+side*1.05),-.10});
      double span=length*(tree?.24:.28)*std::pow(std::sin(pi*t),.75),turn=a+side*1.05;
      if(p.organ==4){
        auto end=add(q,mul(dir,span));b.tube({q,end},.07,.01,3,Bark,Part::Twigs);
        for(int k=0;k<3;k++)fanOrgan(b,add(q,mul(dir,span*(k+.6)/3)),dir,p.leafSize*.55,6,foliageMaterial(i*91+j),Part::Leaves);
      }else if((p.organ==0||p.organ==2||p.organ==3||tree)&&lod<2){
        auto end=add(q,mul(dir,span));b.tube({q,end},.09,.01,3,Bark,Part::Twigs);
        int pinnules=p.organ==3?9:6;
        for(int k=0;k<pinnules;k++)for(int s:{-1,1}){
          double u=(k+.5)/pinnules;auto at=add(q,mul(dir,span*u));
          lamina(at,{std::cos(turn+s*.8),std::sin(turn+s*.8),-.08},{0,0,1},span*.30*(1-u*.7),p.leafWidth,mix(i*71+j,k*3+s+8),2);
        }
      }else lamina(q,dir,{0,0,1},span,p.leafWidth,mix(i,j*3+side+9),2);
    }
  }
}
inline void Botanical::palms() {
  for(int stem=0;stem<p.clumps;stem++){
    double az=stem*2.399963;V offset{std::cos(az)*height*.08*stem,std::sin(az)*height*.08*stem,0};
    std::vector<V> path;for(int i=0;i<=p.segments*2;i++)path.push_back(trunk(double(i)/(p.segments*2),offset));
    b.tube(path,p.radius*1.12,p.radius*.72,sides(),Bark,Part::Trunk);
    for(int ring=1;ring<12;ring++){
      auto at=trunk(ring/13.,offset);b.lathe(at,{{p.radius*.84,-.2},{p.radius*1.02,0},{p.radius*.84,.6}},std::max(5,sides()),Wood,Part::Primary);
    }
    auto top=path.back();
    for(int i=0;i<p.branches;i++){
      double a=i*2.399963,span=height*p.spread*(p.organ==1?.5:.75);
      V end=add(top,{std::cos(a)*span,std::sin(a)*span,span*(.1-.12*(i%3))});
      auto rachis=arc(top,end,p.gravity,std::max(5,count(p.segments+4,4)),mix(doc.seed,i+stem*53));
      b.tube(rachis,p.radius*.065,.02,3,Bark,Part::Twigs);
      if(p.organ==1){fanOrgan(b,rachis[rachis.size()/3],unit(sub(end,top)),span*.85,std::max(9,count(22,9)),foliageMaterial(i),Part::Leaves);continue;}
      int leaflets=std::max(9,count(p.leaves,8));
      for(int j=1;j<=leaflets;j++)for(int side:{-1,1}){
        double t=double(j)/(leaflets+1);auto at=rachis[std::min(rachis.size()-1,size_t(t*(rachis.size()-1)))];
        auto aim=unit({std::cos(a+side*1.05),std::sin(a+side*1.05),-.45});
        lamina(at,aim,{0,0,1},p.leafSize*std::pow(std::sin(pi*t),.5),p.leafWidth,mix(i*71+j,side+3),2);
      }
    }
  }
}
inline void botanicalCorolla(Botanical& g,V center,double radius,uint64_t id,int form) {
  auto& b=g.b;auto& p=g.p;radius*=.4+.6*p.bloom;Rng r(mix(id,g.doc.seed));
  if(p.bloom<.22){b.lathe(center,{{radius*.15,0},{radius*.42,radius*.5},{.01,radius}},8,Leaf,Part::Flowers);return;}
  if(form==9){
    int rays=g.lod==2?28:g.lod==1?50:84;
    b.lathe(center,{{.01,-radius*.15},{radius*.2,0},{.01,radius*.2}},8,Center,Part::Flowers);
    for(int i=0;i<rays;i++){
      double z=1-2*(i+.5)/rays,a=i*2.399963,q=std::sqrt(1-z*z);V aim{std::cos(a)*q,std::sin(a)*q,z};auto tip=add(center,mul(aim,radius));
      b.tube({center,tip},.035,.008,3,Gills,Part::Flowers);
      for(int k=0;k<(g.lod?3:6);k++){double t=k*2*pi/(g.lod?3:6);g.lamina(tip,unit(add(mul(aim,.3),V{std::cos(t),std::sin(t),.3})),{0,0,1},radius*.22,.025,mix(id,i*9+k),2,Gills,Part::Flowers);}
    }return;
  }
  if(form==7||form==10){
    double tall=radius*(form==10?1.5:.9);
    b.lathe(center,{{radius*.13,0},{radius*.20,tall*.35},{radius*.48,tall*.85},{radius*.68,tall},{radius*.60,tall*.92},{radius*.30,tall*.2}},g.lod?8:15,Petal,Part::Flowers,.12);
    for(int k=0;k<5;k++){double a=k*2*pi/5;g.lamina(add(center,{std::cos(a)*radius*.5,std::sin(a)*radius*.5,tall*.94}),{std::cos(a),std::sin(a),.3},{0,0,1},radius*.35,.65,mix(id,k),0,Petal,Part::Flowers);}return;
  }
  int petals=std::max(3,g.count(p.petals,3));
  for(int layer=0;layer<p.flowerLayers;layer++)for(int i=0;i<petals;i++){
    double a=(i+.47*layer)*2*pi/petals,tilt=form==1?.5+layer*.12:form==2?1.1:form==4?(i%2?-.55:1.4):form==5?.65:.12;
    auto dir=unit({std::cos(a),std::sin(a),tilt});
    double curl=form==1?.65:form==2?.55:form==4?-.25:form==5?.35:.08;
    b.leaf(add(center,{0,0,layer*radius*.045}),dir,{0,0,1},radius*(1-layer*.10),form==1?.9:form==2?.95:form==5?.55:.33,0,g.lod?1:0,Petal,Part::Flowers,curl,r.range(-.1,.1),p.leafProfile);
  }
  if(form==3)b.lathe(center,{{radius*.18,0},{radius*.22,radius*.5},{radius*.38,radius*.7},{radius*.34,radius*.75},{radius*.18,radius*.3}},g.lod?8:15,Center,Part::Flowers,.1);
  else b.lathe(center,{{.01,-radius*.05},{radius*(form==0?.26:.12),0},{radius*.10,radius*.12},{.01,radius*.15}},g.lod?8:16,Center,Part::Flowers,.03);
  if(form==5||form==4)for(int i=0;i<6;i++){
    double a=i*2*pi/6;auto tip=add(center,{std::cos(a)*radius*.2,std::sin(a)*radius*.2,radius*.42});b.tube({center,tip},.055,.025,3,Gills,Part::Flowers);
    b.lathe(tip,{{.01,0},{radius*.045,radius*.035},{.01,radius*.08}},5,Center,Part::Flowers);
  }
  for(int i=0;i<5;i++){double a=i*2.399963;g.lamina(add(center,{0,0,-radius*.06}),{std::cos(a),std::sin(a),-.5},{0,0,1},radius*.32,.3,mix(id,i+17),2,Leaf,Part::Flowers);}
}
inline void Botanical::blossoms() {
  for(int i=0;i<p.clumps;i++){
    Rng r(mix(doc.seed,i+92));double a=i*2.399963;V base{std::cos(a)*height*.16*std::sqrt(double(i)/p.clumps),std::sin(a)*height*.16*std::sqrt(double(i)/p.clumps),0};
    V tip=add(base,{r.range(-1,1)*height*p.curvature*.18,r.range(-1,1)*height*p.curvature*.18,height*r.range(.82,1)});
    auto stem=arc(base,tip,0,std::max(4,count(p.segments)),mix(doc.seed,i));b.tube(stem,p.radius,p.radius*.35,std::max(3,sides()-2),Bark,Part::Trunk);
    for(int j=1;j<=5;j++){
      auto q=stem[std::min(stem.size()-1,size_t(j*(stem.size()-1)/6))];double turn=j*2.399963;
      blade(q,{std::cos(turn),std::sin(turn),.2},p.leafSize,mix(i,j));
    }
    if(p.organ==6||p.organ==10||p.organ==11){
      int flowers=lod==2?9:lod==1?15:24;double spike=height*.30;
      for(int k=0;k<flowers;k++){
        double t=(k+.5)/flowers,az=k*2.399963,rad=p.flowerSize*(.25+.4*(1-t));
        auto at=add(stem.back(),{std::cos(az)*rad,std::sin(az)*rad,spike*t});
        if(p.organ==6)b.lathe(at,{{.01,0},{p.flowerSize*.18,1},{p.flowerSize*.14,p.flowerSize*.7},{.01,p.flowerSize*.8}},6,Petal,Part::Flowers);
        else if(p.organ==11){
          lamina(at,{std::cos(az),std::sin(az),.9},{0,0,1},p.flowerSize,.8,mix(i,k),0,Petal,Part::Flowers);
          lamina(at,{std::cos(az),std::sin(az),-.1},{0,0,1},p.flowerSize*.65,.7,mix(i,k+59),0,Petal,Part::Flowers);
        }else botanicalCorolla(*this,at,p.flowerSize*.6,mix(i,k),10);
      }
      b.tube({stem.back(),add(stem.back(),{0,0,spike})},p.radius*.4,.03,3,Bark,Part::Trunk);
    }else if(p.organ==8){
      int florets=lod?15:30;
      for(int k=0;k<florets;k++){double z=1-2*(k+.5)/florets,az=k*2.399963,span=std::sqrt(1-z*z)*p.flowerSize*.5;
        auto at=add(stem.back(),{std::cos(az)*span,std::sin(az)*span,z*p.flowerSize*.5});botanicalCorolla(*this,at,p.flowerSize*.30,mix(i,k),10);}
    }else if(p.organ==7){
      for(int k=0;k<3;k++){auto at=add(stem.back(),{std::cos(a+k*2.4)*p.flowerSize*.8,std::sin(a+k*2.4)*p.flowerSize*.8,-k*p.flowerSize*.7});b.tube({stem.back(),at},.12,.025,3,Bark,Part::Twigs);botanicalCorolla(*this,at,p.flowerSize*.7,mix(i,k),7);}
    }else botanicalCorolla(*this,stem.back(),p.flowerSize,mix(doc.seed,i),p.organ);
  }
}
inline void Botanical::fungi() {
  if(p.organ==6)b.tube({{0,0,0},{0,0,height*.9}},height*.12,height*.10,std::max(6,sides()),Bark,Part::Trunk);
  for(int stem=0;stem<p.clumps;stem++){
    b.check();Rng r(mix(doc.seed,stem+49));double az=stem*2.399963,h=height*r.range(.68,1),radius=h*p.spread*.64;
    V base{std::cos(az)*height*.28*std::sqrt(double(stem)/p.clumps),std::sin(az)*height*.28*std::sqrt(double(stem)/p.clumps),0};
    if(p.organ==6)base={height*.13,0,stem*height*.26};
    auto stalk=arc(base,add(base,{p.organ==2?h*.25:0,0,h*(p.organ==6?.13:.68)}),.03,std::max(3,count(p.segments)),mix(doc.seed,stem));
    b.tube(stalk,p.radius*(p.organ==1?1.2:.7),p.radius*.55,sides(),Gills,Part::Trunk);
    V top=stalk.back();double thickness=radius*p.capThickness;
    std::vector<std::array<double,2>> cap;
    switch(p.organ){
      case 3:cap={{radius*.28,-thickness},{radius*.72,0},{radius,thickness*.8},{radius*.92,thickness*1.5},{radius*.5,0},{.01,-radius*.22}};break;
      case 4:cap={{radius,-radius*.3},{radius*.92,radius*.35},{radius*.52,radius*.85},{.01,radius*.95}};break;
      case 6:cap={{radius,-thickness},{radius*.98,0},{radius*.8,radius*.2},{radius*.25,radius*.48},{.01,radius*.5}};break;
      case 7:cap={{radius*.55,0},{radius*.8,radius*.55},{radius*.7,radius*1.2},{radius*.4,radius*1.9},{.01,radius*2.1}};break;
      default:cap={{radius,-thickness*.12},{radius*.96,thickness*.5},{radius*.74,radius*(p.organ==2?.12:.34)},{radius*.40,radius*.5},{.01,radius*.54}};
    }
    size_t first=b.mesh.vertices.size();b.lathe(top,cap,std::max(10,count(40,8)),Cap,Part::Surface,p.organ==7?.10:.02+p.irregularity*.025,p.organ==2?.20:0);
    if(p.organ==6)for(size_t i=first;i<b.mesh.vertices.size();i++){b.mesh.vertices[i].p[1]*=.55;b.mesh.vertices[i].n=unit({b.mesh.vertices[i].n[0],b.mesh.vertices[i].n[1]/.55,b.mesh.vertices[i].n[2]});}
    if(p.organ==1)b.lathe(top,{{.01,-thickness*.6},{radius*.5,-thickness*.5},{radius,-thickness*.12}},std::max(10,count(36,8)),Gills,Part::Surface);
    else if(p.organ!=7)for(int i=0;i<std::max(10,count(44,10));i++){
      double a=2*pi*i/std::max(10,count(44,10));b.gill(add(top,{std::cos(a)*p.radius*.5,std::sin(a)*p.radius*.5,-thickness*.4}),add(top,{std::cos(a)*radius*.94,std::sin(a)*radius*.94,-thickness*.1}),radius*.07);
    }
    if(p.organ==0||p.organ==5){
      auto ring=stalk[stalk.size()*2/3];b.lathe(ring,{{p.radius*.8,-.4},{p.radius*1.7,-1},{p.radius*.7,.3}},std::max(7,sides()),Gills,Part::Trunk);
      if(p.organ==0){b.lathe(base,{{p.radius*1.4,0},{p.radius*1.15,p.radius},{p.radius*.6,p.radius*1.3}},std::max(7,sides()),Gills,Part::Trunk);
        int spots=std::max(4,count(22,4));for(int i=0;i<spots;i++){double t=std::sqrt((i+.5)/spots)*.85,a=i*2.399963;auto q=add(top,{std::cos(a)*radius*t,std::sin(a)*radius*t,radius*.54*std::sqrt(1-t*t)});lamina(q,{std::cos(a),std::sin(a),.1},{0,0,1},radius*.10,.9,mix(stem,i),1,Gills,Part::Surface);}}
    }
  }
}
inline void Botanical::succulents() {
  if(p.organ>=4){
    int leaves=std::max(6,int(p.leaves*(lod==0?1:lod==1?.80:.55)));double crown=p.organ==7?height*.22:0;
    if(crown)b.tube({{0,0,0},{0,0,crown}},p.radius,p.radius*.65,sides(),Bark,Part::Trunk);
    for(int i=0;i<leaves;i++){
      Rng r(mix(doc.seed,i+81));double u=(i+.5)/leaves,az=i*2.399963+r.range(-.12,.12),span=height*(p.organ==6?.9:.85)*(1-.35*u)*r.range(.92,1.08),raise=.15+.7*u;
      V origin{0,0,crown+height*.10*u},dir=unit({std::cos(az),std::sin(az),raise});
      if(p.organ==7){lamina(origin,dir,{0,0,1},span,p.leafWidth,mix(doc.seed,i),2,CactusSkin);continue;}
      int steps=std::max(3,count(7,3));if(!b.room(steps*8))continue;b.organ++;unsigned first=b.mesh.vertices.size();
      V right{-std::sin(az),std::cos(az),0},up=unit(cross(right,dir));
      for(int k=0;k<=steps;k++){
        double t=double(k)/steps,width=span*p.leafWidth*std::max(.003,std::sin(pi*t));
        auto center=add(origin,add(mul(dir,span*t),mul(up,-span*(p.organ==4?.25:.12)*t*t)));
        const V normals[]={right,up,mul(right,-1),mul(up,-1)};
        for(int j=0;j<4;j++){double a=j*pi/2;b.vertex(add(center,add(mul(right,std::cos(a)*width*.5),mul(up,std::sin(a)*width*.15))),normals[j],j*.25,t,CactusSkin,t);}
      }
      for(int k=0;k<steps;k++)for(int j=0;j<4;j++){unsigned a=first+k*4+j,c=first+k*4+(j+1)%4;b.quad(a,c,c+4,a+4,CactusSkin,Part::Leaves);}
    }return;
  }
  if(p.organ==2){
    int pads=std::max(4,p.leaves);
    for(int i=0;i<pads;i++){
      Rng r(mix(doc.seed,i+39));double level=std::sqrt(double(i))*r.range(.94,1.06),az=i*2.399963+r.range(-.2,.2);V q{std::cos(az)*height*.16*level,std::sin(az)*height*.16*level,height*.15*level};
      auto first=b.mesh.vertices.size();double radius=p.radius*(1+.3*std::sin(i));
      b.lathe(q,{{.01,0},{radius*.7,radius*.4},{radius,radius*1.2},{radius*.8,radius*2},{.01,radius*2.3}},lod==0?12:lod==1?8:5,CactusSkin,Part::Trunk,.02);
      for(size_t v=first;v<b.mesh.vertices.size();v++){auto& x=b.mesh.vertices[v];x.p[1]=q[1]+(x.p[1]-q[1])*.20;x.n=unit({x.n[0],x.n[1]/.2,x.n[2]});}
    }return;
  }
  if(p.organ==1){
    std::vector<std::array<double,2>> profile;for(int i=0;i<=p.segments*2;i++){double t=double(i)/(p.segments*2);profile.push_back({std::max(.01,p.radius*std::pow(std::sin(pi*t),.40)),height*t});}
    b.lathe({},profile,std::max(12,int(p.ribs)*2),CactusSkin,Part::Trunk,.10);
    for(int i=0;i<std::max(12,count(100,12));i++){double t=(i+.5)/std::max(12,count(100,12)),a=i*2.399963,rad=p.radius*std::pow(std::sin(pi*t),.4);lamina({std::cos(a)*rad,std::sin(a)*rad,height*t},{std::cos(a),std::sin(a),.25},{0,0,1},p.radius*p.spines*.3,.025,mix(doc.seed,i),2,Wood);}
    botanicalCorolla(*this,{0,0,height},p.flowerSize,doc.seed,0);return;
  }
  // Columnar cactus arms use smooth elbow splines; areoles are not leaf cards.
  std::vector<V> path;for(int i=0;i<=p.segments;i++)path.push_back(trunk(double(i)/p.segments));
  auto stalk=[&](const std::vector<V>& path,double radius,uint64_t id){
    b.tube(path,radius,radius*.12,std::max(8,int(p.ribs)*2),CactusSkin,Part::Trunk,p.ribs,.80);
    int rings=std::max(4,count(15,4));for(int k=1;k<rings;k++)for(int j=0;j<int(p.ribs);j++){
      double t=double(k)/rings,a=j*2*pi/p.ribs;auto q=add(path[std::min(path.size()-1,size_t(t*(path.size()-1)))],{std::cos(a)*radius*(1-t*.15),std::sin(a)*radius*(1-t*.15),0});
      for(int n=0;n<(lod?1:3);n++)lamina(q,{std::cos(a),std::sin(a),.2*(n-1)},{0,0,1},radius*p.spines*.25,.025,mix(id,k*51+j*3+n),2,Wood);
    }
  };stalk(path,p.radius,doc.seed);
  if(p.organ==0)for(int i=0;i<std::min(6,p.branches);i++){
    Rng r(mix(doc.seed,i+100));double t=.35+.40*(i+.5)/std::max(1,p.branches)+r.range(-.05,.05)*p.irregularity,a=i*2.399963;
    auto base=trunk(t),end=add(base,{std::cos(a)*height*.2,std::sin(a)*height*.2,height*.33});std::vector<V> arm;
    auto c1=add(base,{std::cos(a)*height*.25,std::sin(a)*height*.25,0}),c2=sub(end,{0,0,height*.25});int steps=std::max(4,count(p.segments));
    for(int k=0;k<=steps;k++)arm.push_back(bezier(base,c1,c2,end,double(k)/steps));stalk(arm,p.radius*.6,mix(doc.seed,i+7));
  }
}
inline void Botanical::reedsAndBamboo() {
  bool bamboo=doc.family==Family::Bamboo;
  for(int stem=0;stem<p.clumps;stem++){
    b.check();Rng r(mix(doc.seed,stem+69));double a=stem*2.399963,h=height*r.range(.72,1);
    V base{std::cos(a)*height*.16,std::sin(a)*height*.16,0},last=base;
    int nodes=p.branches;
    for(int j=0;j<nodes;j++){
      double t=double(j+1)/nodes;auto tip=add(base,{std::cos(a)*h*p.curvature*.13*t*t,std::sin(a)*h*p.curvature*.13*t*t,h*t});
      auto path=arc(last,tip,0,std::max(2,count(p.segments,2)),mix(stem,j));
      b.tube(path,p.radius*(1-t*.30),p.radius*(1-t*.34),bamboo?sides():p.organ==2?3:std::max(3,sides()-3),Bark,Part::Trunk);
      b.lathe(tip,{{p.radius*.9,-p.radius*.2},{p.radius*1.12,0},{p.radius*.86,p.radius*.25}},std::max(4,sides()),Wood,Part::Primary);
      double az=j*2.399963+a;
      if(!bamboo&&p.organ==3){
        for(int k=0;k<7;k++){
          double turn=k*2*pi/7;auto end=add(tip,{std::cos(turn)*height*.17,std::sin(turn)*height*.17,height*.08});
          b.tube({tip,end},p.radius*.18,.008,3,Leaf,Part::Leaves);
        }
      }else if(j>nodes/4&&(bamboo||p.organ==0)){
        for(int side:{-1,1}){
          V aim{std::cos(az+side*.6),std::sin(az+side*.6),.12};auto end=add(tip,mul(aim,p.leafSize*.5));
          b.tube({tip,end},p.radius*.18,.012,3,Bark,Part::Twigs);
          for(int k=0;k<(bamboo?3:1);k++)lamina(add(tip,mul(sub(end,tip),(k+.5)/3)),{std::cos(az+side*.6+k*.3),std::sin(az+side*.6+k*.3),-.15},{0,0,1},p.leafSize,p.leafWidth,mix(stem*71+j,k+side+5),2);
        }
      }last=tip;
    }
    if(!bamboo){
      if(p.organ==1){
        for(int k=0;k<5;k++){double turn=k*2.399963;lamina(base,{std::cos(turn)*.22,std::sin(turn)*.22,1},{1,0,0},h*.85,p.leafWidth,mix(stem,k),2,Grass);}
        double size=p.flowerSize;b.lathe(last,{{.01,0},{size*.13,size*.1},{size*.13,size*.95},{.01,size}},8,Wood,Part::Flowers);b.tube({add(last,{0,0,size}),add(last,{0,0,size*1.45})},.10,.01,3,Wood,Part::Flowers);
      }else if(p.organ==0)for(int k=0;k<(lod?5:10);k++){
        double turn=k*2.399963;lamina(add(last,{0,0,p.flowerSize*k/12}),{std::cos(turn)*.25,std::sin(turn)*.25,1},{1,0,0},p.flowerSize*.7,.20,mix(stem,k),2,Wood,Part::Flowers);
      }else if(p.organ==2)for(int k=0;k<5;k++){double turn=k*2.399963;auto at=add(last,{std::cos(turn)*3,std::sin(turn)*3,0});b.lathe(at,{{.01,0},{1.,1.},{.02,3.}},5,Wood,Part::Flowers);}
    }
  }
}
inline void Botanical::climbers() {
  std::vector<V> stem;int steps=std::max(12,count(p.segments*4,12));
  for(int i=0;i<=steps;i++){double t=double(i)/steps;auto at=trunk(t);at[0]+=height*p.spread*.22*std::sin(t*pi*3);at[1]+=height*p.spread*.15*std::cos(t*pi*2);stem.push_back(at);}
  b.tube(stem,p.radius,p.radius*.3,sides(),Bark,Part::Trunk);
  int nodes=std::max(6,p.leaves/(lod==2?2:1));
  for(int i=1;i<nodes;i++){
    double a=p.phyllotaxy==1?(i%2)*pi+i*.17:i*2.399963;V dir{std::cos(a),std::sin(a),-.08};
    auto base=stem[size_t(double(i)*steps/nodes)],tip=add(base,mul(dir,p.leafSize*.25));
    b.tube({base,tip},p.radius*.25,.025,3,Bark,Part::Twigs);blade(tip,dir,p.leafSize,mix(doc.seed,i));
    if(i%4)continue;
    if(p.organ==2){
      for(int k=0;k<(lod?5:12);k++){double t=(k+.5)/(lod?5:12),turn=k*2.4,rad=p.leafSize*.07;
        auto q=add(base,{std::cos(turn)*rad*(1-t*.6),std::sin(turn)*rad*(1-t*.6),-p.leafSize*.4*t});b.lathe(q,{{.01,-rad},{rad,0},{.01,rad}},6,Center,Part::Flowers);}
    }else if(p.organ==3){
      for(int k=0;k<(lod?5:10);k++){auto at=add(base,{std::cos(k*2.4)*2,std::sin(k*2.4)*2,-k*p.flowerSize*.35});botanicalCorolla(*this,at,p.flowerSize*.4,mix(i,k),10);}
    }else if(p.organ==4)b.lathe(add(base,{0,0,-p.flowerSize}),{{.01,0},{p.flowerSize*.3,p.flowerSize*.5},{.01,p.flowerSize}},8,Leaf,Part::Flowers,.15);
    else if(p.organ==5)botanicalCorolla(*this,base,p.flowerSize*.65,mix(doc.seed,i),5);
  }
}
}
