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
    // Each geometric finger samples the connected centre of the leaf tile.
    // Spreading UVs across the entire fan clips the outer fingers at the apex.
    auto center=b.vertex(base,up,.5,.03,mat),l=b.vertex(add(base,mul(d1,size*.65)),up,.38,.55,mat,.6);
    auto r=b.vertex(add(base,mul(d2,size*.65)),up,.62,.55,mat,.6),t=b.vertex(tip,up,.5,.98,mat,1);
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
  if(foliagePhoto(doc)==Photo::Grass){
    int tufts=std::min(lod==0?12:lod==1?7:4,std::max(2,blades/5));
    for(int i=0;i<tufts;i++){
      Rng r(mix(doc.seed,i+1000));double a=i*2.399963,spread=height*.17*std::sqrt(r.real());
      V base{std::cos(a)*spread,std::sin(a)*spread,0},aim{std::cos(a)*.13,std::sin(a)*.13,1};
      double size=height*r.range(.68,1.05);
      b.foliageCard(base,aim,{std::cos(a),std::sin(a),0},size,size*.72,p.leafCurl*.30,lod?1:2,p.leafTwist*r.range(-.45,.45));
    }
    return;
  }
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
  bool photo=foliagePhoto(doc)==Photo::Fern;
  int fronds=photo?std::max(1,count(p.branches,3)):p.branches;
  for(int i=0;i<fronds;i++){
    b.check();Rng r(mix(doc.seed,i+42));double a=leafAngle(i),length=height*(tree?.60:.94)*r.range(.78,1.1);
    V base=add(crown,{std::cos(a)*height*.025,std::sin(a)*height*.025,0});
    auto tip=add(base,{std::cos(a)*length*(p.organ==1?.4:.75),std::sin(a)*length*(p.organ==1?.4:.75),length*(p.organ==1?.95:p.organ==5?1.1:.65)});
    if(photo){
      V aim=sub(tip,base);
      auto stipe=add(base,mul(aim,.18));
      b.tube({base,stipe},tree?.6:p.radius*.18,.025,std::max(3,sides()-2),Bark,Part::Twigs);
      b.foliageCard(base,aim,{0,0,1},ftebroom::plant::length(aim),length*.53,-.24*p.gravity,lod?1:2,p.leafTwist*r.range(-.40,.40));
      continue;
    }
    auto path=arc(base,tip,p.gravity,std::max(5,count(p.segments+4,4)),mix(doc.seed,i));
    b.tube(path,tree?.6:p.radius*.3,.025,std::max(3,sides()-2),Bark,Part::Twigs);
    if(p.organ==5){lamina(base,unit(sub(tip,base)),{0,0,1},length,.26,mix(doc.seed,i),2,foliageMaterial(i));continue;}
    int pinnae=std::max(3,int(p.leaves*(lod==2?.55:1)));
    if((p.organ==0||p.organ==2||p.organ==3||tree)&&lod<2){
      size_t cost=4*(p.organ==3?9:6)*leafCost()+24;
      int quota=int(b.budget*.72/std::max(1,p.branches)/std::max<size_t>(1,cost));pinnae=std::min(pinnae,std::max(lod?3:5,quota));
    }
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
      double a=leafAngle(i),span=height*p.spread*(p.organ==1?.5:.75);
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
inline void botanicalPetal(Botanical& g,V center,double angle,double radius,double width,double lift,double curl,uint64_t id,bool ray=false,bool cup=false) {
  auto& b=g.b;Rng r(mix(id,g.doc.seed));int steps=g.lod==2?2:g.lod==1?4:8,columns=g.lod?2:4;
  if(!b.room(steps*columns*2))return;b.organ++;unsigned first=b.mesh.vertices.size();
  V axis{std::cos(angle),std::sin(angle),0},right{-std::sin(angle),std::cos(angle),0};
  double uneven=r.range(-.045,.045),roll=r.range(.04,.10);
  for(int i=0;i<=steps;i++){
    double t=std::sin(double(i)/steps*pi*.5),outline=std::max(.006,std::pow(std::max(0.,std::sin(pi*t)),ray?.40:.56));
    for(int j=0;j<=columns;j++){
      double u=2.*j/columns-1,lateral=radius*width*.5*outline*u;
      double z=radius*(lift*t+curl*t*t+roll*u*u*std::sin(pi*t)+uneven*u*t);
      double radial=radius*(cup?t*(1-.42*t*t):t),turn=cup?u*width*.5*outline:0;
      V q=add(center,add(mul(axis,radial*std::cos(turn)),add(mul(right,cup?radial*std::sin(turn):lateral),V{0,0,z})));
      V n=unit(add(V{0,0,1},add(mul(axis,-lift-2*curl*t),mul(right,-roll*u*2-uneven*t))));
      b.vertex(q,n,(u+1)*.5,t,Petal,t);
    }
  }
  for(int i=0;i<steps;i++)for(int j=0;j<columns;j++){unsigned a=first+i*(columns+1)+j,c=a+columns+1;b.quad(a,a+1,c+1,c,Petal,Part::Flowers);}
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
  int petals=p.petals;
  // Tulips and poppies both have a cup, but their petal posture and overlap
  // differ. Use organ traits (four broad petals versus six upright tepals).
  bool openCup=form==2&&p.petals<=4;
  for(int layer=0;layer<p.flowerLayers;layer++)for(int i=0;i<petals;i++){
    double a=(i+.47*layer)*2*pi/petals+r.range(-.025,.025);
    double lift=form==1?.10+layer*.09:form==2?(openCup?.08:.46):form==4?(i%2?-.40:.70):form==5?.18:.025;
    double curl=form==1?.12+layer*.07:form==2?(openCup?.15:.73):form==4?-.20:form==5?-.13:.035;
    double width=form==0?.20:form==12?.15:form==1?1.15:form==2?(openCup?1.70:1.05):form==5?.49:.40;
    botanicalPetal(g,add(center,{0,0,layer*radius*.025}),a,radius*(1-layer*.12)*r.range(.95,1.05),width,lift,curl,mix(id,layer*43+i),form==0||form==12,form==2&&!openCup);
  }
  if(form==3)b.lathe(center,{{radius*.18,0},{radius*.22,radius*.5},{radius*.38,radius*.7},{radius*.34,radius*.75},{radius*.18,radius*.3}},g.lod?8:15,Center,Part::Flowers,.1);
  else {
    double disc=form==12?.40:form==0?.24:openCup?.19:.12;
    b.lathe(center,{{.01,-radius*.035},{radius*disc,0},{radius*disc*.86,radius*.065},{.01,radius*.095}},g.lod?10:20,Center,Part::Flowers,.015);
    if((form==0||form==12||openCup)&&g.lod<2){
      int florets=g.lod==1?12:form==12?70:36;
      for(int k=0;k<florets;k++){
        double t=std::sqrt((k+.5)/florets),a=k*2.399963,s=radius*(openCup?.012:.018);
        V q=add(center,{std::cos(a)*radius*disc*t*.88,std::sin(a)*radius*disc*t*.88,radius*(.095-.025*t*t)});
        b.lathe(q,{{s*.45,0},{s,s*.6},{s*.25,s*1.8}},g.lod?3:5,Center,Part::Flowers);
      }
    }
  }
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
      auto q=stem[std::min(stem.size()-1,size_t(j*(stem.size()-1)/6))];double turn=leafAngle(j);
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
      int florets=lod==2?5:lod==1?12:30;
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
    if(p.organ==1){
      double span=length(sub(stalk.back(),stalk.front()));
      b.lathe(base,{{p.radius*.75,0},{p.radius*1.06,span*.16},{p.radius*1.19,span*.40},{p.radius*.95,span*.72},{p.radius*.56,span}},std::max(8,sides()),Bark,Part::Trunk,.018);
    }else b.tube(stalk,p.radius*.75,p.radius*.58,std::max(6,sides()),Bark,Part::Trunk);
    V top=stalk.back();double thickness=radius*p.capThickness;
    std::vector<std::array<double,2>> cap;
    switch(p.organ){
      case 3:cap={{radius*.12,-radius*.32},{radius*.45,-radius*.15},{radius*.80,radius*.05},{radius,radius*.13},{radius*.98,radius*.20},{radius*.80,radius*.14},{radius*.55,-radius*.02},{radius*.24,-radius*.20},{.01,-radius*.25}};break;
      case 4:cap={{radius,-radius*.3},{radius*.92,radius*.35},{radius*.52,radius*.85},{.01,radius*.95}};break;
      case 6:cap={{radius,-thickness},{radius*.98,0},{radius*.8,radius*.2},{radius*.25,radius*.48},{.01,radius*.5}};break;
      case 7:cap={{radius*.45,0},{radius*.65,radius*.18},{radius*.78,radius*.42},{radius*.80,radius*.68},{radius*.74,radius*.98},{radius*.63,radius*1.30},{radius*.48,radius*1.60},{radius*.29,radius*1.85},{.01,radius*1.99}};break;
      default:cap={{radius,-thickness*.12},{radius*.995,thickness*.16},{radius*.94,radius*(p.organ==2?.06:.12)},{radius*.82,radius*(p.organ==2?.12:.28)},{radius*.64,radius*(p.organ==2?.19:.43)},{radius*.39,radius*(p.organ==2?.24:.52)},{radius*.15,radius*(p.organ==2?.26:.55)},{.01,radius*(p.organ==2?.27:.55)}};
    }
    size_t first=b.mesh.vertices.size();b.lathe(top,cap,std::max(10,count(48,8)),Cap,Part::Surface,0,p.organ==2?.20:0);
    for(size_t i=first;i<b.mesh.vertices.size();i++){
      auto& vertex=b.mesh.vertices[i];V q=sub(vertex.p,top);double a=std::atan2(q[1],q[0]),t=q[2]/std::max(.01,radius);
      double ripple=(p.organ==3?.065:p.organ==7?.038:.014)*p.irregularity/.28;
      double scale=1+ripple*(.55*std::sin(a*3+stem*1.7)+.30*std::sin(a*7+t*2.1)+.15*std::sin(a*11-t*3));
      q[0]*=scale;q[1]*=scale;
      if(p.organ==3)q[2]+=radius*.035*std::pow(std::hypot(q[0],q[1])/radius,2)*std::sin(a*5+stem);
      vertex.p=add(top,q);
    }
    if(p.organ==6)for(size_t i=first;i<b.mesh.vertices.size();i++){b.mesh.vertices[i].p[1]*=.55;b.mesh.vertices[i].n=unit({b.mesh.vertices[i].n[0],b.mesh.vertices[i].n[1]/.55,b.mesh.vertices[i].n[2]});}
    if(p.organ==1)b.lathe(top,{{.01,-thickness*.6},{radius*.5,-thickness*.5},{radius,-thickness*.12}},std::max(10,count(36,8)),Gills,Part::Surface);
    else if(p.organ!=7)for(int i=0;i<std::max(10,count(44,10));i++){
      double a=2*pi*i/std::max(10,count(44,10));
      if(p.organ==3){
        V inner=add(top,{std::cos(a)*p.radius*.64,std::sin(a)*p.radius*.64,-radius*.42});
        V mid=add(top,{std::cos(a)*radius*.52,std::sin(a)*radius*.52,-radius*.115});
        V outer=add(top,{std::cos(a)*radius*.96,std::sin(a)*radius*.96,radius*.105});
        b.gill(inner,mid,radius*.023);b.gill(mid,outer,radius*.018);
      }else b.gill(add(top,{std::cos(a)*p.radius*.5,std::sin(a)*p.radius*.5,-thickness*.4}),add(top,{std::cos(a)*radius*.94,std::sin(a)*radius*.94,-thickness*.1}),radius*.055);
    }
    if(p.organ==0||p.organ==5){
      auto ring=stalk[stalk.size()*2/3];b.lathe(ring,{{p.radius*.8,-.4},{p.radius*1.7,-1},{p.radius*.7,.3}},std::max(7,sides()),Gills,Part::Trunk);
      if(p.organ==0){b.lathe(base,{{p.radius*1.4,0},{p.radius*1.15,p.radius},{p.radius*.6,p.radius*1.3}},std::max(7,sides()),Gills,Part::Trunk);
        if(lod<2){int spots=lod?12:30;
          for(int i=0;i<spots;i++){
            double t=std::sqrt((i+.5)/spots)*.91,a=i*2.399963+stem,s=radius*r.range(.025,.045);
            auto q=add(top,{std::cos(a)*radius*t,std::sin(a)*radius*t,radius*.55*std::pow(1-t*t,.65)});
            V normal=unit({std::cos(a)*t*.8,std::sin(a)*t*.8,.65}),right=unit(cross(normal,V{0,1,0})),up=unit(cross(right,normal));
            size_t start=b.mesh.vertices.size();b.lathe({},{{s,0},{s*.86,s*.26},{s*.18,s*.56}},5,Gills,Part::Surface,.12);
            for(size_t k=start;k<b.mesh.vertices.size();k++){auto& vertex=b.mesh.vertices[k];V pos=vertex.p,n=vertex.n;vertex.p=add(q,add(add(mul(right,pos[0]),mul(up,pos[1])),mul(normal,pos[2])));vertex.n=unit(add(add(mul(right,n[0]),mul(up,n[1])),mul(normal,n[2])));}
          }
        }
      }
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
    int pads=std::max(4,p.leaves),depth=0;for(int n=pads;n>1;n/=2)depth++;
    struct Pad {V base,axis,right,normal;double radius,span;};std::vector<Pad> plan;
    double span=height/(1+.74*depth*.92);
    for(int i=0;i<pads;i++){
      b.check();Rng r(mix(doc.seed,i+39));Pad pad;pad.radius=p.radius*r.range(.90,1.10);pad.span=span*r.range(.94,1.06);pad.axis={0,0,1};pad.base={0,0,0};
      if(i){auto& parent=plan[(i-1)/2];double side=i%2?-1:1;
        // A child starts inside its parent's shoulder, so growth forms one
        // connected plant instead of a cloud of independent floating pads.
        pad.base=add(parent.base,add(mul(parent.axis,parent.span*.74),mul(parent.right,side*parent.radius*.40)));
        pad.axis=unit(add(mul(parent.axis,.82),add(mul(parent.right,side*(.42+r.range(-.08,.08)*p.irregularity)),mul(parent.normal,r.range(-.10,.10)*p.irregularity))));
      }
      double yaw=r.range(-.35,.35)*p.irregularity;V normal{-std::sin(yaw),std::cos(yaw),0};pad.right=unit(cross(normal,pad.axis));pad.normal=unit(cross(pad.axis,pad.right));plan.push_back(pad);
      auto first=b.mesh.vertices.size();double radius=pad.radius;
      b.lathe({},{{.01,0},{radius*.7,pad.span*.18},{radius,pad.span*.50},{radius*.8,pad.span*.86},{.01,pad.span}},lod==0?12:lod==1?8:5,CactusSkin,Part::Trunk,.01);
      if(first==b.mesh.vertices.size())break;
      for(size_t v=first;v<b.mesh.vertices.size();v++){auto& x=b.mesh.vertices[v];auto q=x.p,n=x.n;
        x.p=add(pad.base,add(add(mul(pad.right,q[0]),mul(pad.normal,q[1]*.20)),mul(pad.axis,q[2])));
        x.n=unit(add(add(mul(pad.right,n[0]),mul(pad.normal,n[1]/.20)),mul(pad.axis,n[2])));
      }
      int areoles=lod==0?12:lod==1?8:4;
      for(int k=0;k<areoles;k++){double t=.22+.57*(k+.5)/areoles,side=k%3?1:-1;
        V q=add(pad.base,add(mul(pad.axis,pad.span*t),add(mul(pad.right,(k%2?-.42:.42)*radius),mul(pad.normal,side*radius*.20))));
        b.leaf(q,mul(pad.normal,side),pad.axis,radius*p.spines*.22,.035,5,2,Wood,Part::Surface,0,0,p.leafProfile);
      }
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
  int radial=bamboo?sides():p.organ==2?3:std::max(3,sides()-3);
  int segments=std::max(1,count(p.segments,1));
  if(compact()){
    int perCulm=int(b.budget/2/p.clumps);
    radial=std::min(radial,std::max(3,perCulm/(2*(p.branches+1))));
    segments=std::min(segments,std::max(1,(perCulm/(2*radial)-1)/p.branches));
  }
  struct Node {V tip;int stem,index;double az,height;uint64_t id;};std::vector<Node> nodes,heads;
  // Emit every culm first. Internode caps and early leaf sprays must never
  // consume the last culm's budget, especially in the distant LOD.
  for(int stem=0;stem<p.clumps;stem++){
    b.check();Rng r(mix(doc.seed,stem+69));double az=stem*2.399963,h=height*r.range(.72,1);
    V base{std::cos(az)*height*.16,std::sin(az)*height*.16,0},last=base;std::vector<V> stalk{base};
    for(int j=0;j<p.branches;j++){
      double t=double(j+1)/p.branches;auto tip=add(base,{std::cos(az)*h*p.curvature*.13*t*t,std::sin(az)*h*p.curvature*.13*t*t,h*t});
      auto path=arc(last,tip,0,segments,mix(stem,j));stalk.insert(stalk.end(),path.begin()+1,path.end());
      nodes.push_back({tip,stem,j,leafAngle(j)+az,h,mix(doc.seed,stem*97+j+19)});last=tip;
    }
    b.tube(stalk,p.radius,p.radius*.66,radial,Bark,Part::Trunk);
    heads.push_back({last,stem,p.branches-1,az,h,mix(doc.seed,stem+903)});
  }
  if(lod<2&&(bamboo||p.organ==3))for(auto& n:nodes)b.lathe(n.tip,{{p.radius*.9,-p.radius*.2},{p.radius*1.12,0},{p.radius*.86,p.radius*.25}},std::max(4,sides()),Wood,Part::Primary);
  // Reed heads are structural organs, emitted before optional foliage detail.
  if(!bamboo)for(auto& n:heads){auto tip=n.tip;
    if(p.organ==1){double size=p.flowerSize;b.lathe(tip,{{.01,0},{size*.13,size*.1},{size*.13,size*.95},{.01,size}},lod==2?5:8,Wood,Part::Flowers);b.tube({add(tip,{0,0,size}),add(tip,{0,0,size*1.45})},.10,.01,3,Wood,Part::Flowers);}
    else if(p.organ==0)for(int k=0;k<(lod?5:10);k++){double turn=k*2.399963;lamina(add(tip,{0,0,p.flowerSize*k/12}),{std::cos(turn)*.25,std::sin(turn)*.25,1},{1,0,0},p.flowerSize*.7,.20,mix(n.stem,k),2,Wood,Part::Flowers);}
    else if(p.organ==2)for(int k=0;k<5;k++){double turn=k*2.399963;auto at=add(tip,{std::cos(turn)*3,std::sin(turn)*3,0});b.lathe(at,{{.01,0},{1.,1.},{.02,3.}},5,Wood,Part::Flowers);}
  }
  std::stable_sort(nodes.begin(),nodes.end(),[](auto& a,auto& c){return mix(a.id,51)<mix(c.id,51);});
  for(auto& n:nodes){auto tip=n.tip;int j=n.index,stem=n.stem;double az=n.az;
    if(!bamboo&&p.organ==3){for(int k=0;k<7;k++){double turn=k*2*pi/7;auto end=add(tip,{std::cos(turn)*height*.17,std::sin(turn)*height*.17,height*.08});b.tube({tip,end},p.radius*.18,.008,3,Leaf,Part::Leaves);}}
    else if(j>p.branches/4&&(bamboo||p.organ==0))for(int side:{-1,1})for(int shoot=0;shoot<(bamboo?3:1);shoot++){
      double turn=az+side*(.35+shoot*.36);V aim{std::cos(turn),std::sin(turn),.12+shoot*.10};auto end=add(tip,mul(aim,p.leafSize*(bamboo?1.7:.5)));
      b.tube({tip,end},p.radius*.18,.012,3,Bark,Part::Twigs);int blades=bamboo?(lod==2?3:lod==1?4:6):1;
      for(int k=0;k<blades;k++){double t=(k+.5)/blades,spread=(k%2?1:-1)*.62;lamina(add(tip,mul(sub(end,tip),t)),{std::cos(turn+spread),std::sin(turn+spread),-.18-.22*t},{0,0,1},p.leafSize*(1-.25*t),p.leafWidth,mix(stem*71+j,shoot*31+k+side+5),2);}
    }
  }
  if(!bamboo&&p.organ==1)for(auto& n:heads)for(int k=0;k<5;k++){double turn=k*2.399963;V base{std::cos(n.az)*height*.16,std::sin(n.az)*height*.16,0};lamina(base,{std::cos(turn)*.22,std::sin(turn)*.22,1},{1,0,0},n.height*.85,p.leafWidth,mix(n.stem,k),2,Grass);}
}
inline void Botanical::climbers() {
  std::vector<V> stem;int steps=std::max(12,count(p.segments*4,12));
  for(int i=0;i<=steps;i++){double t=double(i)/steps;auto at=trunk(t);at[0]+=height*p.spread*.22*std::sin(t*pi*3);at[1]+=height*p.spread*.15*std::cos(t*pi*2);stem.push_back(at);}
  b.tube(stem,p.radius,p.radius*.3,sides(),Bark,Part::Trunk);
  bool ivy=foliagePhoto(doc)==Photo::Ivy;
  int nodes=ivy?std::max(3,p.leaves/(lod==2?5:3)):std::max(6,p.leaves/(lod==2?2:1));
  for(int i=1;i<nodes;i++){
    double a=leafAngle(i);V dir{std::cos(a),std::sin(a),-.08};
    auto base=stem[size_t(double(i)*steps/nodes)],tip=add(base,mul(dir,p.leafSize*p.petiole));
    if(ivy){
      auto aim=unit(add(mul(dir,.42),{0,0,-.92}));
      b.foliageCard(base,aim,{0,0,1},p.leafSize*2.15,p.leafSize*1.42,-.12,lod?1:2,p.leafTwist*.5);
      continue;
    }
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
