// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Included by generator.h inside worldArchitect after its convex primitives.
inline al::Brush orientedPrism(const std::vector<V>& polygon,V a,V b,const std::string& texture){
  auto axis=b-a;double length=al::length(axis);if(length<1)throw std::runtime_error("Extrusion axis is too short");axis=axis/length;auto x=al::unit(al::cross(std::abs(axis.z)<.9?V{0,0,1}:V{0,1,0},axis)),y=al::cross(axis,x);
  auto brush=prism(polygon,0,length,texture);auto rotate=[&](V p){return x*p.x+y*p.y+axis*p.z;};
  for(auto& face:brush.faces){for(auto& point:face.points)point=a+rotate(point);face.u=rotate(face.u);face.v=rotate(face.v);face.offsetU-=al::dot(face.u,a);face.offsetV-=al::dot(face.v,a);}al::validateBrush(brush);return brush;
}
inline void tube(WorldPlan& w,uint64_t region,const std::string& kind,V a,V b,double radius,int slot){
  std::vector<V> polygon;for(int i=0;i<8;i++){double angle=i*6.283185307179586/8;polygon.push_back({radius*std::cos(angle),radius*std::sin(angle),0});}addBrush(w,region,kind,orientedPrism(polygon,a,b,material(w,slot)));
}
inline void archBand(WorldPlan& w,uint64_t region,const std::string& kind,V a,V b,double radius,double rise,double thickness,int slot){
  for(int segment=0;segment<8;segment++){double start=segment*3.141592653589793/8,end=(segment+1)*3.141592653589793/8;
    std::vector<V> polygon{{radius*std::cos(start),rise*std::sin(start),0},{radius*std::cos(end),rise*std::sin(end),0},{(radius-thickness)*std::cos(end),(rise-thickness)*std::sin(end),0},{(radius-thickness)*std::cos(start),(rise-thickness)*std::sin(start),0}};addBrush(w,region,kind,orientedPrism(polygon,a,b,material(w,slot)));
  }
}
inline void themeArchitecture(WorldPlan& w,const Settings& settings,const Building& b,const ArtTheme& theme,const std::atomic<bool>* cancel){
  if(theme.grammar=="plain")return;Random rng(regionSeed(settings,b.id,"ThemeArchitecture"));double width=b.high.x-b.low.x,cy=(b.low.y+b.high.y)/2;
  const int bays=std::max(2,int(width/theme.baySpacing));
  for(int floor=0;floor<b.floors;floor++){
    cancelled(cancel);double z=floor*256.;
    if(theme.grammar=="subterranean"){
      for(double x:{b.low.x+112,b.high.x-448})archBand(w,b.id,"reinforced concourse vault",{x,cy,z+160},{x+16,cy,z+160},b.hall-8,60,10,2);
    }
    for(int side:{-1,1}){
      const double y=side<0?b.low.y:b.high.y;
      if(theme.grammar=="clinical"){
        box(w,b.id,"clinical ceramic belt",{b.low.x,y-12,z+82},{b.high.x,y+12,z+100},3);
        box(w,b.id,"clinical service rail",{b.low.x,y-13,z+204},{b.high.x,y+13,z+212},2);
      }
      for(int i=0;i<bays;i++){
        double x=b.low.x+width*(i+.5)/bays;
        if(theme.grammar=="masonry")archBand(w,b.id,"masonry arch surround",{x,y+12,z+168},{x,y-12,z+168},42,42,10,2);
        if(theme.grammar=="brutalist")addBrush(w,b.id,"exposed concrete buttress",bevel({x-14,y-28,z},{x+14,y+28,z+236},material(w,2),8));
        if(theme.grammar=="industrial"){
          tube(w,b.id,"exposed utility riser",{x,y+side*25,z+16},{x,y+side*25,z+228},7,3);
          for(double height:{48.,192.})tube(w,b.id,"utility flange",{x,y+side*25,z+height},{x,y+side*25,z+height+8},12,2);
        }
        if(theme.grammar=="clinical")for(int bar=0;bar<3;bar++)box(w,b.id,"clinical window louvre",{x-26,y-14,z+140+bar*10},{x+26,y+14,z+144+bar*10},2);
        if(theme.grammar=="civic"){
          box(w,b.id,"civic facade pier",{x-9,y-24,z},{x+9,y+24,z+240},2);
          box(w,b.id,"civic pier capital",{x-18,y-30,z+212},{x+18,y+30,z+228},5);
        }
        if(theme.weathering>.1&&rng.unit()<theme.weathering&&influence(settings,{x,y,z+64},"preserve",0)==0){double base=z+rng.between(18,124);addBrush(w,b.id,"peeling facade fragment",bevel({x+30,y-19,base},{x+54,y+19,base+48},material(w,3),7));}
      }
    }
  }
  // Roof accents stay above the walkable galleries and below the sealed sky.
  if(theme.grammar=="masonry")box(w,b.id,"masonry chimney",{b.high.x-112,b.high.y-96,b.high.z},{b.high.x-64,b.high.y-48,b.high.z+96},1);
  if(theme.grammar=="industrial")tube(w,b.id,"factory ventilation stack",{b.high.x-96,b.high.y-80,b.high.z},{b.high.x-96,b.high.y-80,b.high.z+112},20,3);
  if(theme.grammar=="civic")tube(w,b.id,"civic radio mast",{b.high.x-80,b.high.y-80,b.high.z},{b.high.x-80,b.high.y-80,b.high.z+144},6,3);
  V lamp{b.low.x-48,cy,96};std::string color=vectorText({theme.lightColor[0],theme.lightColor[1],theme.lightColor[2]});al::PointEntity light{{{"classname","light"},{"origin",vectorText(lamp)},{"light","100"},{"_color",color}}};if(w.profile==4)light.fields["_light"]=std::to_string(int(theme.lightColor[0]*255))+" "+std::to_string(int(theme.lightColor[1]*255))+" "+std::to_string(int(theme.lightColor[2]*255))+" 100";w.architecture.entities.push_back({b.id,std::move(light)});
}
inline void semanticProp(WorldPlan& w,const Building& b,const Room& room,const ArtTheme& theme,V p){
  const auto& function=room.function;const bool bed=function=="ward"||function=="treatment"||function=="surgery"||function=="bedroom"||function=="shelter";
  auto part=[&](const char* kind,V low,V high,int slot){box(w,b.id,kind,p+low,p+high,slot);};std::string rule;
  if(bed){
    part("bed frame",{-20,-28,12},{20,28,20},4);part("bed mattress",{-19,-27,20},{19,27,28},2);for(int x:{-1,1})for(int y:{-1,1})part("bed leg",{x*16.-2,y*23.-2,0},{x*16.+2,y*23.+2,14},4);part("bed headboard",{-22,24,12},{22,30,44},4);rule="sleeping / treatment";
  }else if(function=="production"||function=="machine-shop"||(theme.grammar=="industrial"&&function=="maintenance")){
    part("machine plinth",{-28,-22,0},{28,22,12},2);for(int x:{-1,1})part("press upright",{x*21.-5,-16,12},{x*21.+5,16,80},3);part("press crosshead",{-30,-20,72},{30,20,84},3);part("press work table",{-20,-18,32},{20,18,40},2);tube(w,b.id,"press piston",p+V{0,0,40},p+V{0,0,72},8,3);rule="production machine";
  }else if(function=="office"||function=="control"||function=="dispatch"||function=="security"||theme.grammar=="civic"){
    part("office desk",{-27,-18,30},{27,18,36},4);for(int x:{-1,1})for(int y:{-1,1})part("desk leg",{x*23.-2,y*14.-2,0},{x*23.+2,y*14.+2,32},3);part("abandoned terminal",{-12,0,36},{12,12,56},3);part("terminal screen",{-10,-1,40},{10,1,54},5);rule="work / control desk";
  }else if(function=="pharmacy"||function=="records"||function=="stockroom"||function=="stores"||function=="storage"){
    for(int side:{-1,1})part("shelving upright",{side*23.-2,-18,0},{side*23.+2,18,88},3);for(int shelf=0;shelf<4;shelf++){part("shelf",{-26,-20,shelf*24.},{26,20,shelf*24.+5},4);part("stored supplies",{-17,-14,shelf*24.+5},{17,12,shelf*24.+18},2);}rule="organized supplies / archive";
  }else if(function=="retail"||function=="food-court"||function=="reception"||function=="ticket-office"){
    part("service counter",{-28,-20,0},{28,20,36},4);part("counter top",{-30,-22,36},{30,22,42},2);part("counter register",{4,0,42},{22,14,55},3);rule="public service counter";
  }else if(function=="platform-access"||function=="assembly"||function=="stair-lobby"||function=="living"){
    part("public bench",{-29,-16,16},{29,16,24},4);for(int x:{-1,1})part("bench support",{x*22.-4,-12,0},{x*22.+4,12,18},2);part("bench back",{-29,12,22},{29,18,48},4);rule="rest / assembly seating";
  }else if(theme.grammar=="brutalist"){
    addBrush(w,b.id,"precast service barrier",bevel(p+V{-28,-18,0},p+V{28,18,42},material(w,2),12));rule="precast infrastructure";
  }else if(theme.grammar=="subterranean"||theme.grammar=="clinical"||function=="electrical"||function=="utility"){
    part("service cabinet",{-22,-18,0},{22,18,80},3);part("cabinet panel",{-18,-20,8},{18,-18,72},2);for(int i=0;i<3;i++)part("cabinet louvre",{-14,-22,48+i*7.},{14,-20,50+i*7.},3);rule="utility equipment";
  }else{
    addBrush(w,b.id,"salvage crate",bevel(p+V{-24,-20,0},p+V{24,20,40},material(w,4),8));part("crate band",{-25,-22,8},{25,-18,12},2);rule="service storage";
  }
  w.assets.push_back({b.id,room.function+" / "+rule,"original semantic brush kit / "+theme.grammar,p});
}
