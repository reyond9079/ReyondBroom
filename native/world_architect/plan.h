// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../third_party/json.hpp"
#include "../../plugins/autolevel/al_brush.h"
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <iomanip>
#include <numeric>
#include <optional>
#include <memory>
#include <queue>
#include <unordered_map>

namespace ftebroom::worldArchitect {
using Json=nlohmann::json;
using V=al::Vec3;
inline constexpr int schemaVersion=1;
inline constexpr const char* stages[]={"Mission","Site","Architecture","Damage","Dressing","Navigation","Export"};
inline uint64_t mix(uint64_t n){n+=0x9e3779b97f4a7c15ULL;n=(n^(n>>30))*0xbf58476d1ce4e5b9ULL;n=(n^(n>>27))*0x94d049bb133111ebULL;return n^(n>>31);}
inline uint64_t hash(const std::string& s){uint64_t n=1469598103934665603ULL;for(unsigned char c:s){n^=c;n*=1099511628211ULL;}return n;}
inline uint64_t subSeed(uint64_t seed,const std::string& stage,uint64_t region=0){return mix(seed^hash(stage)^mix(region));}
struct Random {
  uint64_t state;explicit Random(uint64_t seed):state(seed){}
  uint64_t next(){state=mix(state);return state;}
  double unit(){return (next()>>11)*0x1.0p-53;}
  double between(double lo,double hi){return lo+(hi-lo)*unit();}
  int integer(int lo,int hi){return lo+int(next()%uint64_t(hi-lo+1));}
};
inline void finite(double value,double lo,double hi,const char* message){if(!std::isfinite(value)||value<lo||value>hi)throw std::runtime_error(message);}
inline void safeToken(const std::string& text){if(text.empty()||text.size()>120||text.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.")!=text.npos||text.find("..")!=text.npos)throw std::runtime_error("Unsafe World Architect identifier or material");}
inline Json vector(V p){return Json::array({p.x,p.y,p.z});}
inline V vector(const Json& j){if(!j.is_array()||j.size()!=3)throw std::runtime_error("Expected three coordinates");V v;for(int k=0;k<3;k++){v[k]=j.at(k).get<double>();finite(v[k],-30000,30000,"Coordinate outside World Architect bounds");}return v;}
enum class Scope {Interior,City,Mixed,Arena};
enum class Role {Start,Introduction,Challenge,Reveal,Landmark,Reward,Exit,Transition};
inline const char* roleName(Role r){static const char* names[]={"Start","Introduction","Challenge","Reveal","Landmark","Reward","Exit","Transition"};return names[int(r)];}
struct Player {double radius=16,height=56,step=18;};
struct Experience {int beats=7,branches=2,loops=1;double verticality=.5,openness=.5;};
struct MissionNode {uint64_t id=0;Role role=Role::Challenge;std::string name;V position;bool required=true,pinned=false;int floor=0;uint64_t room=0;};
struct MissionEdge {uint64_t a=0,b=0;bool optional=false,oneWay=false;std::string purpose;uint64_t prerequisite=0;};
struct MissionGraph {std::vector<MissionNode> nodes;std::vector<MissionEdge> edges;};
inline void validateMission(const MissionGraph& graph);
struct Road {uint64_t id=0;V a,b;double width=160;std::string hierarchy="local";bool pinned=false;std::optional<V> control;};
// Quadratic Bezier controls are optional, so old straight-road recipes retain
// their exact geometry. Sweep, collision validation and plan rendering share
// this bounded tessellation rather than drawing an unrelated preview curve.
inline std::vector<V> roadPoints(const Road& road){
  if(!road.control)return {road.a,road.b};
  int steps=std::clamp(int(std::ceil((al::length(*road.control-road.a)+al::length(road.b-*road.control))/64)),8,128);
  std::vector<V> result;for(int i=0;i<=steps;i++){double t=double(i)/steps;result.push_back(road.a*((1-t)*(1-t))+*road.control*(2*t*(1-t))+road.b*(t*t));}return result;
}
struct Lot {uint64_t id=0;V low,high;std::string zone;};
struct Room {uint64_t id=0;V low,high,entry;std::string function;int floor=0;bool required=true;uint64_t mission=0;};
struct Building {uint64_t id=0;V low,high;int floors=1;std::string archetype;bool pinned=false;std::vector<Room> rooms;double hall=72;bool atrium=false,parallel=false;std::vector<double> bays;};
struct SpatialGraph {std::vector<Road> roads;std::vector<Lot> lots;std::vector<Building> buildings;};
struct ArchitectureElement {uint64_t region=0;std::string kind;al::Brush brush;};
struct ArchitectureEntity {uint64_t region=0;al::PointEntity entity;};
struct ArchitecturePlan {std::vector<ArchitectureElement> elements;std::vector<ArchitectureEntity> entities;};
struct DamageZone {uint64_t region=0;V origin;double radius=128,intensity=.4;std::string cause="blast";};
struct DamageField {std::vector<DamageZone> zones;};
struct AssetPlacement {uint64_t region=0;std::string rule,provenance="original procedural brush kit";V position;};
struct NavigationNode {V feet;uint64_t region=0;bool required=false;};
struct NavigationGraph {std::vector<NavigationNode> nodes;std::vector<std::array<size_t,2>> edges;std::vector<size_t> mainRoute;size_t reachable=0;};
struct Diagnostics {bool valid=false;std::vector<std::string> rejects,warnings,cachedStages;std::map<std::string,double> timing,score;size_t brushes=0,mapBytes=0;double routeLength=0;};
struct Preset {
  std::string id,name,theme,archetype,story,layout;
  Scope scope=Scope::Interior;int floors=2,rooms=6,blocks=2;
  double width=1408,depth=832,street=192,damage=.4;
  double hallHalfWidth=72;std::vector<std::string> roomFunctions{"public","service","storage"};
  std::string ruinEvent,scenario;
  Experience experience;
};
struct ArtTheme {std::string id,grammar="plain";std::array<std::array<unsigned char,3>,6> colors;double weathering=0,baySpacing=192;std::array<double,3> lightColor{1,.85,.7};};
inline std::string shortKey(uint64_t n){std::ostringstream out;out.imbue(std::locale::classic());out<<std::hex<<std::setfill('0')<<std::setw(6)<<(n&0xffffff);return out.str();}
inline std::string themeAssetKey(const ArtTheme& theme){return shortKey(hash(Json{{"encoding",2},{"id",theme.id},{"colors",theme.colors},{"grammar",theme.grammar},{"weathering",theme.weathering},{"baySpacing",theme.baySpacing},{"lightColor",theme.lightColor}}.dump()));}
struct Influence {uint64_t id=0;V low,high;std::string kind="preserve";double strength=1;};
struct AuthoredModule {
  std::string id,function,source,license,map;int profile=2;
  std::map<std::string,int> materials;
};
inline Json moduleJson(const AuthoredModule& module){return {{"id",module.id},{"function",module.function},{"profile",module.profile},{"source",module.source},{"license",module.license},{"map",module.map},{"materialSlots",module.materials}};}
inline AuthoredModule readModule(const Json& j){
  AuthoredModule m;m.id=j.at("id");m.function=j.at("function");m.profile=j.at("profile");m.source=j.at("source");m.license=j.at("license");m.map=j.at("map");m.materials=j.at("materialSlots").get<std::map<std::string,int>>();return m;
}
inline void validateModule(const AuthoredModule& m){
  safeToken(m.id);safeToken(m.function);finite(m.profile,0,4,"Unknown authored module profile");
  if(m.source.empty()||m.source.size()>2000||m.license.empty()||m.license.size()>2000||m.map.empty()||m.map.size()>65536||m.materials.size()>64)throw std::runtime_error("Authored module exceeds its source, license, MAP or material budget");
  for(const auto& [material,slot]:m.materials){if(material.empty()||material.size()>120)throw std::runtime_error("Invalid authored material name");finite(slot,0,5,"Authored material aliases use theme slots 0..5");}
}
inline std::vector<AuthoredModule> readModulePack(const Json& j){
  if(j.at("schemaVersion")!=1||j.at("modules").empty()||j.at("modules").size()>16)throw std::runtime_error("Expected an authored module pack v1 with 1..16 modules");
  std::vector<AuthoredModule> result;size_t bytes=0;std::set<std::string> ids;
  for(const auto& item:j.at("modules")){auto m=readModule(item);validateModule(m);if(!ids.insert(m.id).second)throw std::runtime_error("Duplicate authored module ID");bytes+=m.map.size();if(bytes>131072)throw std::runtime_error("Authored module pack exceeds 128 KiB of source MAPs");auto geometry=al::parseMap(m.map);if(geometry.brushes.empty()||geometry.brushes.size()>128||geometry.entities.size()>32)throw std::runtime_error("Each authored module needs 1..128 convex brushes and at most 32 point entities");for(const auto& brush:geometry.brushes){al::validateBrush(brush);for(const auto& face:brush.faces)if(!m.materials.count(face.material))throw std::runtime_error("Declare a theme slot for every authored material; original MAP bytes are retained");}result.push_back(std::move(m));}
  return result;
}
struct Settings {
  uint64_t seed=1;std::string preset="hospital";int profile=2,candidates=4;
  double scale=1,damage=.4,dressing=.5;Player player;
  std::array<bool,7> locked{};
  std::array<Json,5> frozen;uint64_t frozenRegistry=0;
  std::vector<MissionNode> missionPins;
  std::optional<MissionGraph> authoredMission;
  // Empty / zero fields inherit the registry's archetype program.
  std::string layout,theme;double width=0,depth=0,hall=0;int floors=0,rooms=0,blocks=0;
  std::vector<Road> roadPins;
  std::vector<AuthoredModule> modules;std::map<uint64_t,std::string> roomModules;
  std::vector<uint64_t> pinnedRegions;
  // Clean per-building Architecture / Damage / Dressing recipes. Mission
  // and Site stay frozen while pins exist to retain their lots and routes.
  std::map<uint64_t,std::array<Json,3>> regionRecipes;
  uint64_t pinnedRegistry=0;
  std::map<uint64_t,uint64_t> regionSeeds,roomSeeds;
  std::vector<Influence> influences;
  std::map<std::string,double> weights{{"variety",1},{"route",1},{"circulation",1},{"visibility",1},{"cover",.5},{"verticality",.5}};
};
struct WorldPlan {
  int schema=schemaVersion;uint64_t seed=0,registryFingerprint=0;int profile=2;std::string preset,theme,assetKey,sourceRevision;
  V low,high;std::map<std::string,uint64_t> seeds;
  MissionGraph mission;SpatialGraph spatial;ArchitecturePlan architecture;
  DamageField damage;std::vector<AssetPlacement> assets;NavigationGraph navigation;Diagnostics diagnostics;
  std::string map,architectureKey;
  std::map<std::string,AuthoredModule> authoredSources;
  std::array<std::shared_ptr<const Settings>,7> stageInputs;
};
struct ProfileCapability {const char* name;bool sky,patches,models,shaders;const char* spawn;double coordinateLimit;};
inline ProfileCapability capability(int profile){
  static const ProfileCapability profiles[]={
    {"Quake III",true,true,true,true,"info_player_deathmatch",30000},
    {"FTE Custom",true,false,true,true,"info_player_start",30000},
    {"Quake 1",true,false,false,false,"info_player_start",4000},
    {"Quake II",true,false,false,false,"info_player_start",4000},
    {"GoldSrc",true,false,false,false,"info_player_start",4000}};
  if(profile<0||profile>=5)throw std::runtime_error("Unknown game profile");return profiles[profile];
}
struct Registry {
  uint64_t fingerprint=0;
  std::vector<Preset> presets;std::vector<ArtTheme> themes;
  const Preset& preset(const std::string& id)const{for(const auto& p:presets)if(p.id==id)return p;throw std::runtime_error("Unknown World Architect preset: "+id);}
  const ArtTheme& theme(const std::string& id)const{for(const auto& t:themes)if(t.id==id)return t;throw std::runtime_error("Unknown World Architect theme: "+id);}
};
inline Registry readRegistry(const Json& j){
  if(j.at("schemaVersion")!=schemaVersion)throw std::runtime_error("Unsupported World Architect registry version");
  Registry r;r.fingerprint=hash(j.dump());std::set<std::string> ids,materialPrefixes;
  if(j.at("presets").size()>64||j.at("themes").size()>32)throw std::runtime_error("World Architect registry budget exceeded");
  for(const auto& item:j.at("themes")){
    ArtTheme t;t.id=item.at("id").get<std::string>();safeToken(t.id);if(!materialPrefixes.insert(t.id.substr(0,7)).second)throw std::runtime_error("Theme material prefixes must be unique");if(!ids.insert("theme/"+t.id).second)throw std::runtime_error("Duplicate theme");
    t.grammar=item.value("grammar",std::string("plain"));if(t.grammar!="plain"&&t.grammar!="masonry"&&t.grammar!="brutalist"&&t.grammar!="industrial"&&t.grammar!="clinical"&&t.grammar!="subterranean"&&t.grammar!="civic")throw std::runtime_error("Unknown art-theme grammar");t.weathering=item.value("weathering",0.);finite(t.weathering,0,1,"Invalid theme weathering");t.baySpacing=item.value("baySpacing",192.);finite(t.baySpacing,96,384,"Invalid theme bay spacing");if(item.contains("lightColor"))t.lightColor=item.at("lightColor").get<std::array<double,3>>();for(auto channel:t.lightColor)finite(channel,0,1,"Invalid theme light color");
    const auto& colors=item.at("colors");if(colors.size()!=6)throw std::runtime_error("Theme needs six material colors");
    for(int a=0;a<6;a++)for(int c=0;c<3;c++){int n=colors.at(a).at(c);finite(n,0,255,"Invalid theme color");t.colors[a][c]=static_cast<unsigned char>(n);}r.themes.push_back(t);
  }
  for(const auto& item:j.at("presets")){
    Preset p;p.id=item.at("id");p.name=item.at("name");p.theme=item.at("theme");p.archetype=item.at("archetype");p.story=item.at("story");p.layout=item.at("layout");
    safeToken(p.id);safeToken(p.theme);safeToken(p.archetype);safeToken(p.layout);r.theme(p.theme);
    if(p.layout!="spine"&&p.layout!="ring"&&p.layout!="parallel"&&p.layout!="atrium"&&p.layout!="courtyard"&&p.layout!="offset"&&p.layout!="branched")throw std::runtime_error("Unknown architectural / site grammar");
    p.hallHalfWidth=item.value("hallHalfWidth",(p.layout=="atrium"||p.layout=="ring"||p.layout=="parallel")?128.:72.);finite(p.hallHalfWidth,64,224,"Gallery / corridor half-width must be 64..224");
    p.roomFunctions=item.value("roomFunctions",p.roomFunctions);if(p.roomFunctions.empty()||p.roomFunctions.size()>24)throw std::runtime_error("A room program needs 1..24 named functions");for(const auto& function:p.roomFunctions)safeToken(function);
    p.ruinEvent=item.value("ruinEvent",std::string{});if(!p.ruinEvent.empty()&&p.ruinEvent!="collapsed-stair"&&p.ruinEvent!="flooded-platform")throw std::runtime_error("Unsupported structural ruin event");
    p.scenario=item.value("scenario",std::string{});if(!p.scenario.empty()&&p.scenario!="checkpoint"&&p.scenario!="factory"&&p.scenario!="courtyard"&&p.scenario!="overpass"&&p.scenario!="mall"&&p.scenario!="evacuation")throw std::runtime_error("Unknown authored scenario kit");
    if(!ids.insert("preset/"+p.id).second)throw std::runtime_error("Duplicate preset");
    auto scope=item.at("scope").get<int>();finite(scope,0,3,"Invalid scope");p.scope=Scope(scope);
    if(!p.ruinEvent.empty()&&p.scope!=Scope::Interior)throw std::runtime_error("This structural event requires an interior envelope");
    p.floors=item.at("floors");p.rooms=item.at("rooms");p.blocks=item.at("blocks");p.width=item.at("width");p.depth=item.at("depth");p.street=item.at("street");p.damage=item.at("damage");
    finite(p.floors,1,4,"Floors must be 1..4");finite(p.rooms,2,16,"Rooms must be 2..16");finite(p.blocks,1,4,"Blocks must be 1..4");finite(p.width,768,2560,"Invalid building width");finite(p.depth,448,1536,"Invalid building depth");finite(p.street,128,384,"Invalid street width");finite(p.damage,0,1,"Invalid ruin amount");
    p.experience.beats=item.value("beats",7);p.experience.branches=item.value("branches",2);p.experience.loops=item.value("loops",1);finite(p.experience.beats,4,16,"Invalid mission size");finite(p.experience.branches,1,4,"Invalid branch count");finite(p.experience.loops,1,4,"Invalid loop count");r.presets.push_back(p);
  }
  if(r.presets.empty()||r.themes.empty())throw std::runtime_error("Empty World Architect registry");return r;
}
inline Settings deserialize(const Json& j,const Registry& r,bool recipe=false);
inline Preset stagePreset(const Settings& s,const Registry& registry){
  auto p=registry.preset(s.preset);
  if(!s.layout.empty()){
    if(s.layout!="spine"&&s.layout!="ring"&&s.layout!="atrium"&&s.layout!="parallel"&&s.layout!="courtyard"&&s.layout!="offset"&&s.layout!="branched")throw std::runtime_error("Unknown designer layout grammar");p.layout=s.layout;
  }
  if(!s.theme.empty()){registry.theme(s.theme);p.theme=s.theme;}
  if(s.width){finite(s.width,768,2560,"Building width must be 768..2560");p.width=s.width;}
  if(s.depth){finite(s.depth,448,1536,"Building depth must be 448..1536");p.depth=s.depth;}
  if(s.hall){finite(s.hall,64,224,"Gallery half-width must be 64..224");p.hallHalfWidth=s.hall;}
  if(s.floors){finite(s.floors,1,4,"Floors must be 1..4");p.floors=s.floors;}
  if(s.rooms){finite(s.rooms,2,16,"Rooms must be 2..16");p.rooms=s.rooms;}
  if(s.blocks){finite(s.blocks,1,4,"Blocks must be 1..4");if((p.scope==Scope::Interior||p.scope==Scope::Arena)&&s.blocks!=1)throw std::runtime_error("Interior archetypes use one building envelope");p.blocks=s.blocks;}
  return p;
}
inline Settings cleanSettings(Settings s){s.locked.fill(false);s.frozen.fill(Json(nullptr));s.frozenRegistry=0;return s;}
inline Settings unpinnedSettings(Settings s){s=cleanSettings(std::move(s));s.pinnedRegions.clear();s.regionRecipes.clear();s.pinnedRegistry=0;return s;}
inline int lockedThrough(const Settings& s){int last=-1;for(int i=0;i<5;i++)if(s.locked[i])last=i;return last;}
inline void validate(const Settings& s,const Registry& r,bool recipe=false){
  stagePreset(s,r);if(s.authoredMission)validateMission(*s.authoredMission);capability(s.profile);finite(s.scale,.6,1.4,"Scale must be 0.6..1.4");finite(s.damage,0,1,"Damage must be 0..1");finite(s.dressing,0,1,"Dressing must be 0..1");finite(s.candidates,1,16,"Candidate count must be 1..16");
  finite(s.player.radius,8,32,"Player radius must be 8..32");finite(s.player.height,32,80,"Player height must be 32..80");finite(s.player.step,4,18,"Player step must be 4..18");
  if(s.pinnedRegions.size()>4||s.regionRecipes.size()!=s.pinnedRegions.size())throw std::runtime_error("Building pins require one bounded recipe set per building");
  if(!s.pinnedRegions.empty()){
    if(s.pinnedRegistry!=r.fingerprint)throw std::runtime_error("Building pins require their original registry snapshot");
    if(!recipe&&(!s.locked[0]||!s.locked[1]))throw std::runtime_error("Unpin buildings before unlocking their Mission and Site");
    std::set<uint64_t> pins;const auto p=stagePreset(s,r);const int buildings=(p.scope==Scope::Interior||p.scope==Scope::Arena)?1:p.blocks;
    for(auto id:s.pinnedRegions){
      if(id<100||id>=uint64_t(100+buildings)||!pins.insert(id).second||!s.regionRecipes.count(id))throw std::runtime_error("Unknown or duplicate pinned building");
      for(const auto& input:s.regionRecipes.at(id)){
        if(!input.is_object()||input.dump().size()>262144)throw std::runtime_error("Invalid pinned-building recipe budget");
        if(input.value("pinnedRegions",Json::array()).size()||input.value("regionRecipes",Json::array()).size())throw std::runtime_error("Recursive building pins are not allowed");
        if(input.contains("frozenStageInputs"))for(const auto& nested:input.at("frozenStageInputs"))if(!nested.is_null())throw std::runtime_error("A building recipe cannot contain frozen stages");
        if(input.contains("stageLocks"))for(const auto& flag:input.at("stageLocks"))if(flag.get<bool>())throw std::runtime_error("A building recipe must be clean");
        auto clean=deserialize(input,r,true);if(clean.profile!=s.profile||clean.preset!=s.preset||stagePreset(clean,r).theme!=p.theme)throw std::runtime_error("Unpin buildings before changing profile, preset or theme");
      }
    }
  }
  if(s.modules.size()>16||s.roomModules.size()>256)throw std::runtime_error("Authored module assignment budget exceeded");
  size_t moduleBytes=0;std::set<std::string> moduleIds;for(const auto& module:s.modules){validateModule(module);if(!moduleIds.insert(module.id).second)throw std::runtime_error("Duplicate authored module ID");moduleBytes+=module.map.size();}if(moduleBytes>131072)throw std::runtime_error("Authored module MAP budget exceeded");
  for(const auto& [room,id]:s.roomModules)if(!room||!moduleIds.count(id))throw std::runtime_error("Room references an absent authored module");
  bool gap=false;for(int i=0;i<7;i++){if(s.locked[i]){if(gap||i>=5)throw std::runtime_error("Freeze a stage with its upstream stages; Navigation and Export are always revalidated");}else gap=true;}
  const int last=lockedThrough(s);
  if(last>=0&&s.frozenRegistry!=r.fingerprint)throw std::runtime_error("Locked stages require the registry snapshot from their accepted candidate");
  for(int i=0;i<5;i++)if(!s.frozen[i].is_null()){
    const auto& baseline=s.frozen[i];if(!baseline.is_object()||baseline.dump().size()>1024*1024)throw std::runtime_error("Invalid locked-stage input budget");
    if(baseline.contains("frozenStageInputs"))for(const auto& nested:baseline.at("frozenStageInputs"))if(!nested.is_null())throw std::runtime_error("Recursive locked-stage input is not allowed");
    if(baseline.contains("stageLocks"))for(const auto& flag:baseline.at("stageLocks"))if(flag.get<bool>())throw std::runtime_error("A stage input must be a clean generation recipe");
    auto input=deserialize(baseline,r,true);if(i<=last&&(input.profile!=s.profile||input.preset!=s.preset))throw std::runtime_error("Unlock stages before changing the game profile or preset");
  }else if(i<=last)throw std::runtime_error("A stage lock has no accepted generation input");
  if(s.missionPins.size()>64||s.roadPins.size()>64||s.influences.size()>128||s.roomSeeds.size()>256||s.regionSeeds.size()>128||s.pinnedRegions.size()>128)throw std::runtime_error("Too many World Architect overrides");
  for(auto& [name,weight]:s.weights){safeToken(name);if(name!="variety"&&name!="route"&&name!="circulation"&&name!="visibility"&&name!="cover"&&name!="verticality")throw std::runtime_error("Unknown quality weight");finite(weight,0,10,"Score weight must be 0..10");}
  std::set<uint64_t> ids;for(const auto& p:s.missionPins){if(!p.id||!ids.insert(p.id).second)throw std::runtime_error("Duplicate mission pin");for(int a=0;a<3;a++)finite(p.position[a],-30000,30000,"Invalid mission pin");}
  ids.clear();for(const auto& p:s.roadPins){if(!p.id||!ids.insert(p.id).second)throw std::runtime_error("Duplicate road pin");for(int a=0;a<3;a++){finite(p.a[a],-30000,30000,"Invalid road pin");finite(p.b[a],-30000,30000,"Invalid road pin");}if(p.control){for(int a=0;a<3;a++)finite((*p.control)[a],-30000,30000,"Invalid road curve control");if(p.control->z!=0)throw std::runtime_error("Road curve controls must stay on the ground plane");}finite(p.width,48,512,"Invalid road width");if(al::length(p.a-p.b)<48||p.a.z!=0||p.b.z!=0)throw std::runtime_error("Road pins require a ground-plane segment of at least 48 units");}
  for(const auto& mask:s.influences){for(int a=0;a<3;a++){finite(mask.low[a],-30000,30000,"Invalid influence bounds");finite(mask.high[a],-30000,30000,"Invalid influence bounds");if(mask.high[a]<=mask.low[a])throw std::runtime_error("Inverted influence bounds");}finite(mask.strength,0,1,"Invalid influence strength");if(mask.kind!="preserve"&&mask.kind!="avoid"&&mask.kind!="damage"&&mask.kind!="dressing")throw std::runtime_error("Unknown influence type");}
}
inline Json serialize(const Settings& s){
  Json j={{"schemaVersion",schemaVersion},{"worldSeed",s.seed},{"presetId",s.preset},{"profileId",s.profile},{"candidates",s.candidates},{"scale",s.scale},{"damage",s.damage},{"dressing",s.dressing},{"player",{{"radius",s.player.radius},{"height",s.player.height},{"step",s.player.step}}},{"stageLocks",s.locked},{"frozenStageInputs",s.frozen},{"frozenRegistry",s.frozenRegistry},{"qualityWeights",s.weights},{"pinnedRegions",s.pinnedRegions}};
  j["missionPins"]=Json::array();for(const auto& p:s.missionPins)j["missionPins"].push_back({{"id",p.id},{"position",vector(p.position)}});
  j["roadPins"]=Json::array();for(const auto& p:s.roadPins)j["roadPins"].push_back({{"id",p.id},{"a",vector(p.a)},{"b",vector(p.b)},{"width",p.width},{"control",p.control?vector(*p.control):Json(nullptr)}});
  j["authoredModules"]=Json::array();for(const auto& module:s.modules)j["authoredModules"].push_back(moduleJson(module));j["roomModules"]=Json::array();for(const auto& [room,module]:s.roomModules)j["roomModules"].push_back({{"room",room},{"module",module}});
  j["roomSeeds"]=Json::array();for(const auto& [id,seed]:s.roomSeeds)j["roomSeeds"].push_back({{"room",id},{"seed",seed}});
  j["regionSeeds"]=Json::array();for(const auto& [id,seed]:s.regionSeeds)j["regionSeeds"].push_back({{"region",id},{"seed",seed}});
  j["pinnedRegistry"]=s.pinnedRegistry;j["regionRecipes"]=Json::array();for(const auto& [id,recipes]:s.regionRecipes)j["regionRecipes"].push_back({{"region",id},{"inputs",recipes}});
  j["architecture"]={{"layout",s.layout},{"theme",s.theme},{"width",s.width},{"depth",s.depth},{"hall",s.hall},{"floors",s.floors},{"rooms",s.rooms},{"blocks",s.blocks}};
  j["missionGraph"]=nullptr;if(s.authoredMission){auto& graph=j["missionGraph"];graph={{"nodes",Json::array()},{"edges",Json::array()}};for(const auto& n:s.authoredMission->nodes)graph["nodes"].push_back({{"id",n.id},{"role",int(n.role)},{"name",n.name},{"required",n.required}});for(const auto& e:s.authoredMission->edges)graph["edges"].push_back({{"a",e.a},{"b",e.b},{"optional",e.optional},{"oneWay",e.oneWay},{"purpose",e.purpose},{"prerequisite",e.prerequisite}});}
  j["influences"]=Json::array();for(const auto& p:s.influences)j["influences"].push_back({{"id",p.id},{"low",vector(p.low)},{"high",vector(p.high)},{"kind",p.kind},{"strength",p.strength}});return j;
}
inline Settings deserialize(const Json& j,const Registry& r,bool recipe){
  if(j.at("schemaVersion")!=schemaVersion)throw std::runtime_error("Unsupported World Architect project version");Settings s;
  s.seed=j.at("worldSeed");s.preset=j.at("presetId");s.profile=j.at("profileId");s.candidates=j.at("candidates");s.scale=j.at("scale");s.damage=j.at("damage");s.dressing=j.at("dressing");
  const auto& p=j.at("player");s.player={p.at("radius"),p.at("height"),p.at("step")};s.locked=j.at("stageLocks").get<std::array<bool,7>>();s.weights=j.at("qualityWeights").get<std::map<std::string,double>>();s.pinnedRegions=j.value("pinnedRegions",std::vector<uint64_t>{});
  if(j.contains("frozenStageInputs"))s.frozen=j.at("frozenStageInputs").get<std::array<Json,5>>();s.frozenRegistry=j.value("frozenRegistry",uint64_t(0));
  for(const auto& p:j.at("missionPins")){MissionNode n;n.id=p.at("id");n.position=vector(p.at("position"));n.pinned=true;s.missionPins.push_back(n);}
  for(const auto& p:j.at("roadPins")){Road road;road.id=p.at("id");road.a=vector(p.at("a"));road.b=vector(p.at("b"));road.width=p.at("width");if(p.contains("control")&&!p.at("control").is_null())road.control=vector(p.at("control"));road.pinned=true;s.roadPins.push_back(road);}
  if(j.contains("authoredModules"))for(const auto& module:j.at("authoredModules"))s.modules.push_back(readModule(module));
  if(j.contains("roomModules"))for(const auto& assignment:j.at("roomModules"))if(!s.roomModules.emplace(assignment.at("room").get<uint64_t>(),assignment.at("module").get<std::string>()).second)throw std::runtime_error("Duplicate authored room assignment");
  if(j.contains("roomSeeds"))for(const auto& p:j.at("roomSeeds"))if(!s.roomSeeds.emplace(p.at("room").get<uint64_t>(),p.at("seed").get<uint64_t>()).second)throw std::runtime_error("Duplicate room seed");
  for(const auto& p:j.at("regionSeeds"))s.regionSeeds[p.at("region").get<uint64_t>()]=p.at("seed").get<uint64_t>();
  s.pinnedRegistry=j.value("pinnedRegistry",uint64_t(0));if(j.contains("regionRecipes"))for(const auto& p:j.at("regionRecipes"))if(!s.regionRecipes.emplace(p.at("region").get<uint64_t>(),p.at("inputs").get<std::array<Json,3>>()).second)throw std::runtime_error("Duplicate building recipe");
  if(j.contains("architecture")){const auto& a=j.at("architecture");s.layout=a.at("layout");s.theme=a.at("theme");s.width=a.at("width");s.depth=a.at("depth");s.hall=a.at("hall");s.floors=a.at("floors");s.rooms=a.at("rooms");s.blocks=a.at("blocks");}
  if(j.contains("missionGraph")&&!j.at("missionGraph").is_null()){
    const auto& graph=j.at("missionGraph");if(graph.at("nodes").size()>63||graph.at("edges").size()>256)throw std::runtime_error("Mission override budget exceeded");MissionGraph g;
    for(const auto& n:graph.at("nodes")){MissionNode node;node.id=n.at("id");int role=n.at("role");finite(role,0,7,"Unknown mission role");node.role=Role(role);node.name=n.at("name");if(node.name.size()>120)throw std::runtime_error("Mission name exceeds its budget");node.required=n.at("required");g.nodes.push_back(node);}
    for(const auto& e:graph.at("edges")){MissionEdge edge;edge.a=e.at("a");edge.b=e.at("b");edge.optional=e.at("optional");edge.oneWay=e.at("oneWay");edge.purpose=e.at("purpose");if(edge.purpose.size()>120)throw std::runtime_error("Mission transition name exceeds its budget");edge.prerequisite=e.at("prerequisite");g.edges.push_back(edge);}s.authoredMission=std::move(g);
  }
  for(const auto& p:j.at("influences")){Influence mask;mask.id=p.at("id");mask.low=vector(p.at("low"));mask.high=vector(p.at("high"));mask.kind=p.at("kind");mask.strength=p.at("strength");s.influences.push_back(mask);}validate(s,r,recipe);return s;
}
inline MissionGraph mission(const Settings& settings,const Preset& preset){
  MissionGraph graph;Random rng(subSeed(settings.seed,"Mission"));
  int count=preset.experience.beats+rng.integer(0,2);
  for(int i=0;i<count;i++){
    Role role=i==0?Role::Start:i==1?Role::Introduction:i==count-1?Role::Exit:i==count-2?Role::Landmark:i==count/2?Role::Reveal:Role::Challenge;
    MissionNode n;n.id=uint64_t(i+1);n.role=role;n.name=roleName(role);graph.nodes.push_back(n);
    if(i)graph.edges.push_back({uint64_t(i),uint64_t(i+1),false,false,"main route",0});
  }
  for(int i=0;i<preset.experience.branches;i++){
    MissionNode reward;reward.id=graph.nodes.size()+1;reward.role=Role::Reward;reward.name="Optional salvage "+std::to_string(i+1);reward.required=false;graph.nodes.push_back(reward);
    int from=rng.integer(2,count-2);graph.edges.push_back({uint64_t(from),reward.id,true,false,"risk / reward",0});
    graph.edges.push_back({reward.id,uint64_t(std::min(count,from+2)),true,false,"flanking return",0});
  }
  for(int i=0;i<preset.experience.loops;i++){
    int from=rng.integer(1,count-3);graph.edges.push_back({uint64_t(from),uint64_t(from+3),true,false,"return shortcut",0});
  }
  if(settings.authoredMission){graph=*settings.authoredMission;for(auto& n:graph.nodes){n.position={};n.room=0;n.floor=0;n.pinned=false;}}
  for(const auto& pin:settings.missionPins){auto found=std::find_if(graph.nodes.begin(),graph.nodes.end(),[&](const auto& n){return n.id==pin.id;});if(found==graph.nodes.end())throw std::runtime_error("A pinned mission node no longer exists");found->position=pin.position;found->pinned=true;}
  return graph;
}
// State-space traversal honors prerequisite nodes and one-way edges. It is
// separate from the later physical hull/navigation check.
inline std::vector<uint64_t> missionTraversal(const MissionGraph& graph){
  if(graph.nodes.empty()||graph.nodes.size()>63)throw std::runtime_error("Mission needs 1..63 nodes");std::map<uint64_t,size_t> index;int starts=0,exits=0;size_t start=0;
  for(size_t i=0;i<graph.nodes.size();i++){const auto& n=graph.nodes[i];if(!n.id||!index.emplace(n.id,i).second)throw std::runtime_error("Duplicate mission node");finite(int(n.role),0,7,"Unknown mission role");if(n.name.size()>120)throw std::runtime_error("Mission name exceeds its budget");if(n.role==Role::Start){start=i;starts++;}exits+=n.role==Role::Exit;}
  if(starts!=1||exits!=1)throw std::runtime_error("Mission needs one start and one exit");
  if(graph.edges.size()>256)throw std::runtime_error("Too many mission transitions");for(const auto& e:graph.edges)if(!index.count(e.a)||!index.count(e.b)||e.a==e.b||(e.prerequisite&&!index.count(e.prerequisite))||e.purpose.size()>120)throw std::runtime_error("Invalid mission edge");
  uint64_t required=0,tracked=0;
  for(size_t i=0;i<graph.nodes.size();i++)if(graph.nodes[i].required)required|=uint64_t(1)<<i;
  tracked=required;for(const auto& e:graph.edges)if(e.prerequisite)tracked|=uint64_t(1)<<index.at(e.prerequisite);
  using State=std::pair<size_t,uint64_t>;State initial{start,(uint64_t(1)<<start)&tracked};std::deque<State> queue{initial};std::map<State,State> parents{{initial,initial}};
  while(!queue.empty()){
    auto [at,visited]=queue.front();queue.pop_front();
    if(graph.nodes[at].role==Role::Exit&&(visited&required)==required){std::vector<uint64_t> route;State state{at,visited};for(;;){route.push_back(graph.nodes[state.first].id);if(state==initial)break;state=parents.at(state);}std::reverse(route.begin(),route.end());return route;}
    for(const auto& e:graph.edges){
      if(e.prerequisite&&!(visited&(uint64_t(1)<<index.at(e.prerequisite))))continue;
      size_t next=SIZE_MAX;if(index.at(e.a)==at)next=index.at(e.b);else if(!e.oneWay&&index.at(e.b)==at)next=index.at(e.a);
      if(next!=SIZE_MAX){State state{next,visited|((uint64_t(1)<<next)&tracked)};if(parents.emplace(state,State{at,visited}).second){if(parents.size()>100000)throw std::runtime_error("Mission state search exceeds 100000 states");queue.push_back(state);}}
    }
  }
  throw std::runtime_error("Mission cannot visit its required beats and reach the exit under its gate / direction rules");
}
inline void validateMission(const MissionGraph& graph){(void)missionTraversal(graph);}
inline Json diagnostics(const WorldPlan& world){
  Json j={{"schemaVersion",schemaVersion},{"seed",world.seed},{"preset",world.preset},{"profile",world.profile},{"valid",world.diagnostics.valid},{"rejects",world.diagnostics.rejects},{"warnings",world.diagnostics.warnings},{"timingMs",world.diagnostics.timing},{"cachedStages",world.diagnostics.cachedStages},{"scores",world.diagnostics.score},{"brushes",world.diagnostics.brushes},{"mapBytes",world.diagnostics.mapBytes},{"routeLength",world.diagnostics.routeLength},{"stageSeeds",world.seeds},{"reachable",world.navigation.reachable},{"navigationNodes",world.navigation.nodes.size()}};
  j["missionNodes"]=Json::array();for(const auto& n:world.mission.nodes)j["missionNodes"].push_back({{"id",n.id},{"role",roleName(n.role)},{"required",n.required},{"room",n.room},{"floor",n.floor},{"position",vector(n.position)}});
  j["missionEdges"]=Json::array();for(const auto& e:world.mission.edges)j["missionEdges"].push_back({{"a",e.a},{"b",e.b},{"purpose",e.purpose},{"optional",e.optional},{"oneWay",e.oneWay},{"prerequisite",e.prerequisite}});
  j["roads"]=Json::array();for(const auto& road:world.spatial.roads){Json points=Json::array();for(auto p:roadPoints(road))points.push_back(vector(p));j["roads"].push_back({{"id",road.id},{"width",road.width},{"points",points}});}
  j["artTheme"]=world.theme;j["assetKey"]=world.assetKey;j["bounds"]={{"low",vector(world.low)},{"high",vector(world.high)}};j["rooms"]=Json::array();for(const auto& b:world.spatial.buildings)for(const auto& room:b.rooms)j["rooms"].push_back({{"id",room.id},{"building",b.id},{"floor",room.floor},{"low",vector(room.low)},{"high",vector(room.high)},{"function",room.function},{"mission",room.mission}});return j;
}
}
