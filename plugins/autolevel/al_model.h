// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include "al_fill.h"
#include <map>
#include <set>
#include <string>
#include <limits>
namespace al {
using Id=uint32_t;
enum class Resolve {Matching,AllToAll};
enum class Placement {Add,Replace,ReplaceFirst};
struct BlockRef {std::string blockUid;int variant=0;};
inline bool operator==(const BlockRef& a,const BlockRef& b){return a.blockUid==b.blockUid&&a.variant==b.variant;}
struct LayerSettings {int layer=0;std::vector<BlockRef> dependencies;Resolve resolve=Resolve::Matching;Placement placement=Placement::Add;};
struct Variant {std::string bigBlockUid;std::vector<BlockAction> actions;SideIds sideIds;uint8_t fill=0;float weight=1;LayerSettings layerSettings;Vec3 editorOffset;};
struct BlockAsset {std::string uid,name;bool active=true;std::string group="Base",weightGroup="Base";std::vector<std::string> actionsGroups;std::vector<Variant> variants{Variant{}};std::string geometry;Vec3 workbenchPos;bool walkable=true,hasSpawn=false;Vec3 spawnPoint;uint8_t walkableSides=63;};
struct BigBlockAsset {std::string uid,name;bool active=true;int blockLayer=0;bool overrideGroup=false;std::string group="Base";bool overrideWeightGroup=false;std::string weightGroup="Base";std::vector<std::string> actionsGroups;Int3 size{1,1,1};std::vector<std::vector<BlockRef>> data{1};};
struct BlockSide {BlockRef block;int d=0;};
inline bool operator==(const BlockSide& a,const BlockSide& b){return a.block==b.block&&a.d==b.d;}
struct Connection {BlockSide a,b;};
inline bool operator==(const Connection& a,const Connection& b){return (a.a==b.a&&a.b==b.b)||(a.a==b.b&&a.b==b.a);}
struct ActionsGroup {std::string name;std::vector<std::vector<BlockAction>> groupActions;};
inline std::vector<ActionsGroup> defaultActions(){using A=BlockAction;return {{"Full Rotate On Z",{{A::RotateZ},{A::RotateZ,A::RotateZ},{A::RotateZ,A::RotateZ,A::RotateZ}}},{"Rotate On Z",{{A::RotateZ}}},{"Full Mirror",{{A::MirrorX},{A::MirrorY},{A::MirrorZ}}}};}
struct Repo {std::string uid="repo",name="New Repo";Vec3 cellSize{256,256,192};bool useFilling=true;std::vector<std::string> groups,weightGroups;std::vector<ActionsGroup> actionsGroups=defaultActions();std::vector<Connection> bannedConnections,exclusiveConnections;std::vector<BlockAsset> blocks;std::vector<BigBlockAsset> bigBlocks;std::vector<Id> closedSideIds{0};std::map<std::string,std::string> materialSlots{{"al_floor",""},{"al_wall",""},{"al_ceiling",""},{"al_trim",""},{"al_stair",""}};};
inline std::vector<std::string> allGroups(const std::vector<std::string>& custom){std::vector<std::string> a{"Empty","Solid","Base"};a.insert(a.end(),custom.begin(),custom.end());return a;}
inline const BlockAsset* block(const Repo& r,const std::string& uid){for(auto& b:r.blocks)if(b.uid==uid)return &b;return nullptr;}
inline BlockAsset* block(Repo& r,const std::string& uid){return const_cast<BlockAsset*>(block(static_cast<const Repo&>(r),uid));}
inline const Variant* variant(const Repo& r,const BlockRef& ref){auto b=block(r,ref.blockUid);return b&&ref.variant>=0&&size_t(ref.variant)<b->variants.size()?&b->variants[ref.variant]:nullptr;}
inline Variant* variant(Repo& r,const BlockRef& ref){return const_cast<Variant*>(variant(static_cast<const Repo&>(r),ref));}
inline void distinctNames(std::vector<std::string>& a,bool builtin=true){std::set<std::string> used;if(builtin)used={"Empty","Solid","Base"};a.erase(std::remove_if(a.begin(),a.end(),[&](auto& n){return n.empty()||!used.insert(n).second;}),a.end());if(a.size()>(builtin?28:512))throw std::runtime_error("too many groups (maximum 31 including built-ins)");}
inline void validateActions(const std::vector<BlockAction>& a){if(a.size()>128)throw std::runtime_error("too many actions");for(auto v:a)if(int(v)<0||int(v)>6)throw std::runtime_error("invalid action");}
inline bool safeUid(const std::string& s){return !s.empty()&&s.size()<=96&&s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.")==s.npos&&s!="."&&s!="..";}
inline void integrity(Repo& r){
 if(!safeUid(r.uid))throw std::runtime_error("invalid repo UID");
 for(int a=0;a<3;a++)if(!std::isfinite(r.cellSize[a])||r.cellSize[a]<16||r.cellSize[a]>4096)throw std::runtime_error("cell size must be 16..4096");
 distinctNames(r.groups);distinctNames(r.weightGroups);if(r.blocks.size()>512||r.bigBlocks.size()>512)throw std::runtime_error("maximum 512 blocks");
 auto groups=allGroups(r.groups),weights=allGroups(r.weightGroups);std::set<std::string> uids,ags;
 for(auto& ag:r.actionsGroups){if(!ags.insert(ag.name).second)throw std::runtime_error("duplicate action group");for(auto& a:ag.groupActions)validateActions(a);}
 for(auto& b:r.blocks){if(!safeUid(b.uid)||!uids.insert(b.uid).second)throw std::runtime_error("invalid or duplicate block UID");if(b.variants.empty())b.variants.emplace_back();if(b.variants.size()>128)throw std::runtime_error("maximum 128 variants per block");b.variants[0].actions.clear();if(std::find(groups.begin(),groups.end(),b.group)==groups.end())b.group="Base";if(std::find(weights.begin(),weights.end(),b.weightGroup)==weights.end())b.weightGroup="Base";
 for(auto& v:b.variants){validateActions(v.actions);if(!std::isfinite(v.weight)||v.weight<0||v.weight>1e6)throw std::runtime_error("weight must be finite and non-negative");if(v.layerSettings.layer<0||v.layerSettings.layer>=16)throw std::runtime_error("layer must be 0..15");}}
 std::set<std::string> bigIds;for(auto& b:r.bigBlocks){if(b.blockLayer<0||b.blockLayer>=16)throw std::runtime_error("big block layer must be 0..15");if(!safeUid(b.uid)||!bigIds.insert(b.uid).second)throw std::runtime_error("invalid big block UID");for(int a=0;a<3;a++)if(b.size[a]<1||b.size[a]>8)throw std::runtime_error("big block size must be 1..8");b.data.resize(volume(b.size));for(auto& cell:b.data)cell.erase(std::remove_if(cell.begin(),cell.end(),[&](auto ref){return !variant(r,ref);}),cell.end());}
 std::set<std::pair<std::string,int>> attached;for(auto& b:r.bigBlocks)for(auto& c:b.data)for(auto ref:c){if(!attached.insert({ref.blockUid,ref.variant}).second)throw std::runtime_error("variant attached more than once");auto v=variant(r,ref);v->bigBlockUid=b.uid;v->layerSettings.layer=b.blockLayer;}
 for(auto& b:r.blocks)for(size_t i=0;i<b.variants.size();i++){auto& v=b.variants[i];if(!attached.count({b.uid,int(i)}))v.bigBlockUid.clear();auto& deps=v.layerSettings.dependencies;deps.erase(std::remove_if(deps.begin(),deps.end(),[&](auto ref){auto d=variant(r,ref);return !d||d->layerSettings.layer>=v.layerSettings.layer;}),deps.end());}
 auto clean=[&](auto& cs){cs.erase(std::remove_if(cs.begin(),cs.end(),[&](auto c){return !variant(r,c.a.block)||!variant(r,c.b.block)||c.a.d<0||c.a.d>5||c.b.d<0||c.b.d>5;}),cs.end());};clean(r.bannedConnections);clean(r.exclusiveConnections);
}
struct GroupSettings {std::string weightGroup;bool overrideWeight=false;float weight=1;};
struct BoundarySide {std::vector<std::string> groups;std::string levelBuilderUid;};
struct InputWaveCell {int32_t groups=INT32_MAX;bool containAll()const{return groups==INT32_MAX;}};
struct LevelLayer {std::vector<uint64_t> blocks;bool valid=false;};
struct LevelData {Int3 position{},size{5,5,5};std::vector<LevelLayer> layers;};
enum class CellPick {RandomOrder,MinWeightSum,ShannonEntropy};
struct Builder {std::string uid="builder",name="New Builder",repoUid;Vec3 worldOrigin;LevelData levelData;Bounds3i selection;std::vector<InputWaveCell> inputWave;std::vector<GroupSettings> groupsWeights;std::map<std::string,float> moduleWeightMultipliers;std::array<BoundarySide,6> boundary;bool useMultiThreadedSolver=false,rebuildBottomLayers=false;int currentLayer=0,seed=0,lastSeed=0;CellPick cellPick=CellPick::RandomOrder;bool keepLargestComponent=true;float minCoverage=.45f;bool requireAllFloors=true,autoAdvanceLayers=true,addPlayerStart=true;float lightScale=1;int previewMode=0;bool selected=true;};
struct EditorSettings {bool groupsToggle=false;int maxIterations=10;bool combineMeshBeforeExport=true;int exportSize=5,tool=0;bool drawSelfConnections=true;int maxConnectionsDrawCount=30,editMode=0;bool showedConnectionTutorial=false;std::string activeBuilder,activeRepo;std::vector<std::string> repoUids,builderUids;Vec3 workbenchOrigin{-8192,-8192,0};int spacing=2;};
inline void resize(Builder& b,Int3 pos,Int3 size,int layers){for(int a=0;a<3;a++)if(size[a]<1||size[a]>64)throw std::runtime_error("builder size must be 1..64");if(volume(size)>32768||layers<1||layers>16)throw std::runtime_error("builder cell/layer budget exceeded");auto old=b.levelData;auto wave=b.inputWave;b.levelData.position=pos;b.levelData.size=size;b.levelData.layers.resize(layers);b.inputWave.assign(volume(size),{});for(auto& l:b.levelData.layers){l.blocks.assign(volume(size),0);l.valid=false;}
 for(size_t i=0;i<volume(size);i++){auto p=position(i,size)+pos-old.position;if(!inside(p,old.size))continue;auto j=index(p,old.size);if(j<wave.size())b.inputWave[i]=wave[j];for(size_t l=0;l<std::min(old.layers.size(),b.levelData.layers.size());l++)if(j<old.layers[l].blocks.size())b.levelData.layers[l].blocks[i]=old.layers[l].blocks[j];}
 if(pos==old.position&&size==old.size)for(size_t l=0;l<std::min(old.layers.size(),b.levelData.layers.size());l++)b.levelData.layers[l].valid=old.layers[l].valid;
 for(int a=0;a<3;a++){b.selection.min[a]=std::clamp(b.selection.min[a],pos[a],pos[a]+size[a]-1);b.selection.max[a]=std::clamp(b.selection.max[a],b.selection.min[a]+1,pos[a]+size[a]);}b.currentLayer=std::clamp(b.currentLayer,0,layers-1);
}
inline void remapGroups(Builder& b,const std::vector<std::string>& old,const std::vector<std::string>& now){for(auto& c:b.inputWave)if(!c.containAll()){uint32_t bits=0;for(size_t n=0;n<now.size();n++){auto it=std::find(old.begin(),old.end(),now[n]);if(it==old.end()||(uint32_t(c.groups)&(1u<<std::distance(old.begin(),it))))bits|=1u<<n;}c.groups=bits==0||bits==((1u<<now.size())-1)?INT32_MAX:int32_t(bits);}}
inline void paint(Builder& b,Bounds3i world,int group,int operation,int groups){uint32_t full=(1u<<groups)-1;for(size_t i=0;i<b.inputWave.size();i++){auto p=position(i,b.levelData.size)+b.levelData.position;if(!world.contains(p))continue;uint32_t m=b.inputWave[i].containAll()?full:uint32_t(b.inputWave[i].groups);if(operation==2)m=group<0?full:1u<<group;else if(group>=0){if(operation==0)m|=1u<<group;else if((m&(m-1))!=0)m&=~(1u<<group);}b.inputWave[i].groups=m==full?INT32_MAX:int32_t(m);}}
}
