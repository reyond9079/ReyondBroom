// SPDX-License-Identifier: MIT
// Independent constraint/entropy solver; AutoLevel's Unity code is not embedded.
#pragma once
#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cmath>
#include <deque>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
namespace rbwfc {
using Domain=std::bitset<256>;using I=std::array<int,3>;
static constexpr int delta[6][3]={{1,0,0},{0,1,0},{-1,0,0},{0,-1,0},{0,0,1},{0,0,-1}};
inline int opposite(int d){return d<4?(d+2)%4:9-d;}
struct Block{std::string name,source;I size{1,1,1};std::array<int,6> sockets{};double weight=1;int group=0,rotations=1,kind=0;};
struct State{int block=0,rotation=0;I part{},size{};std::array<int,6> sockets{};double weight=1;int group=0;};
struct Region{I low{},high{};int group=-1,block=-1;};
struct Config{I size{6,6,1};unsigned seed=1337;std::array<int,6> boundary{};std::array<double,3> groupWeights{1,.1,0};bool connected=true;std::vector<Region> constraints;bool rebuild=false;Region region;};
inline std::vector<State> compile(const std::vector<Block>& blocks){
 std::vector<State> result;int variant=0;
 for(size_t i=0;i<blocks.size();i++){auto b=blocks[i];if(!std::isfinite(b.weight)||b.weight<0||b.group<0||b.group>2)throw std::runtime_error("Invalid block weight/group.");if(b.source.size()>1024*1024||b.name.size()>256)throw std::runtime_error("Module source/name exceeds its budget.");for(auto socket:b.sockets)if(socket<0||socket>65535)throw std::runtime_error("Module connector IDs must be 0..65535.");for(int a:b.size)if(a<1||a>4)throw std::runtime_error("Block sizes must be 1..4 cells.");
  for(int turn=0;turn<(b.rotations?4:1);turn++,variant++){
   I size=b.size;if(turn%2)std::swap(size[0],size[1]);std::array<int,6> ports=b.sockets;for(int d=0;d<4;d++)ports[(d+turn)%4]=b.sockets[d];
   auto index=[&](I p){return (p[2]*size[1]+p[1])*size[0]+p[0];};
   for(int z=0;z<size[2];z++)for(int y=0;y<size[1];y++)for(int x=0;x<size[0];x++){
    State s;s.block=i;s.rotation=turn;s.part={x,y,z};s.size=size;s.weight=b.weight;s.group=b.group;s.sockets=ports;
    for(int d=0;d<6;d++){I neighbor{x+delta[d][0],y+delta[d][1],z+delta[d][2]};if(neighbor[0]>=0&&neighbor[0]<size[0]&&neighbor[1]>=0&&neighbor[1]<size[1]&&neighbor[2]>=0&&neighbor[2]<size[2])s.sockets[d]=100000+variant*512+std::min(index(s.part),index(neighbor))*3+(d==0||d==2?0:d==1||d==3?1:2);}
    result.push_back(s);if(result.size()>256)throw std::runtime_error("Block library exceeds 256 rotated cell variants. Reduce rotations or large-block sizes.");
   }
  }
 }if(result.empty())throw std::runtime_error("The block library is empty.");return result;
}
inline bool inside(I p,const Region& r){for(int a=0;a<3;a++)if(p[a]<r.low[a]||p[a]>=r.high[a])return false;return true;}
struct Result{std::vector<int> cells;int decisions=0,backtracks=0;double seconds=0;};
class Solver {
 Config config;const std::vector<State>& states;std::vector<Domain> wave;std::vector<std::array<Domain,6>> compatible;std::mt19937 rng;
 std::atomic<bool>& cancel;std::atomic<float>& progress;std::deque<int> queue;
 struct Change{int cell;Domain before;};std::vector<Change> trail;
 struct Decision{int cell;size_t marker;Domain alternatives;};std::vector<Decision> decisions;
 int count=0;std::chrono::steady_clock::time_point started;
 int index(I p)const{return (p[2]*config.size[1]+p[1])*config.size[0]+p[0];}
 I position(int i)const{return {i%config.size[0],(i/config.size[0])%config.size[1],i/(config.size[0]*config.size[1])};}
 int neighbor(int cell,int dir)const{auto p=position(cell);for(int a=0;a<3;a++){p[a]+=delta[dir][a];if(p[a]<0||p[a]>=config.size[a])return -1;}return index(p);}
 void check(){if(cancel)throw std::runtime_error("Generation cancelled.");if(std::chrono::steady_clock::now()-started>std::chrono::seconds(15))throw std::runtime_error("Solver time budget reached; reduce the volume or add constraints.");if(trail.size()>1500000)throw std::runtime_error("Solver memory budget reached.");}
 void assign(int cell,Domain value){if(value==wave[cell])return;trail.push_back({cell,wave[cell]});wave[cell]=value;queue.push_back(cell);}
 bool propagate(){int polls=0;while(!queue.empty()){if((polls++&127)==0)check();int cell=queue.front();queue.pop_front();if(wave[cell].none())return false;
  for(int d=0;d<6;d++){int n=neighbor(cell,d);if(n<0)continue;Domain allowed;for(size_t s=0;s<states.size();s++)if(wave[cell][s])allowed|=compatible[s][d];Domain next=wave[n]&allowed;if(next.none())return false;if(next!=wave[n])assign(n,next);}
 }return true;}
 int choose(Domain domain){double total=0;for(size_t s=0;s<states.size();s++)if(domain[s])total+=states[s].weight*config.groupWeights[states[s].group];double target=std::generate_canonical<double,32>(rng)*total;for(size_t s=0;s<states.size();s++)if(domain[s]){target-=states[s].weight*config.groupWeights[states[s].group];if(target<=0)return int(s);}for(size_t s=0;s<states.size();s++)if(domain[s])return s;throw std::runtime_error("No enabled block fits this cell.");}
 public:
 Solver(Config c,const std::vector<State>& s,std::atomic<bool>& stop,std::atomic<float>& p):config(c),states(s),rng(c.seed),cancel(stop),progress(p){
  count=1;for(int a:c.size){if(a<1||a>32)throw std::runtime_error("Grid dimensions must be 1..32.");count*=a;}if(count>4096)throw std::runtime_error("The grid is limited to 4096 cells.");
  for(auto w:c.groupWeights)if(!std::isfinite(w)||w<0)throw std::runtime_error("Group weights must be finite and nonnegative.");
  compatible.resize(s.size());for(size_t a=0;a<s.size();a++)for(int d=0;d<6;d++)for(size_t b=0;b<s.size();b++)if(s[a].sockets[d]==s[b].sockets[opposite(d)])compatible[a][d].set(b);
 }
 Result solve(const std::vector<int>& previous={}){
  started=std::chrono::steady_clock::now();wave.assign(count,{});std::vector<unsigned> required(count,0);
  if(config.rebuild&&previous.size()!=size_t(count))throw std::runtime_error("Generate a full layout before rebuilding a region.");
  if(config.connected){
   // Force a random spanning tree before WFC. Socket matching alone does not
   // guarantee a traversable network. No geometry is patched after solving.
   std::vector<bool> visited(count);std::vector<int> stack{0};visited[0]=true;
   while(!stack.empty()){int cell=stack.back();std::vector<int> next;for(int d=0;d<6;d++){int n=neighbor(cell,d);if(n<0||visited[n])continue;if(config.rebuild){if(!inside(position(cell),config.region)&&states[previous[cell]].sockets[d]==0)continue;if(!inside(position(n),config.region)&&states[previous[n]].sockets[opposite(d)]==0)continue;}next.push_back(d);}if(next.empty()){stack.pop_back();continue;}int d=next[rng()%next.size()],n=neighbor(cell,d);required[cell]|=1u<<d;required[n]|=1u<<opposite(d);visited[n]=true;stack.push_back(n);}
   for(bool v:visited)if(!v)throw std::runtime_error("Locked cells cannot form one connected network through this rebuild region.");
  }
  for(int i=0;i<count;i++){auto p=position(i);for(size_t s=0;s<states.size();s++){
    auto& state=states[s];bool allowed=state.weight>0&&config.groupWeights[state.group]>0;
    for(int a=0;a<3;a++)if(p[a]-state.part[a]<0||p[a]-state.part[a]+state.size[a]>config.size[a])allowed=false;
    for(int d=0;d<6;d++){if(neighbor(i,d)<0&&config.boundary[d]>=0&&state.sockets[d]!=config.boundary[d])allowed=false;if((required[i]&(1u<<d))&&state.sockets[d]==0)allowed=false;}
    for(auto r:config.constraints)if(inside(p,r)&&((r.group>=0&&r.group!=state.group)||(r.block>=0&&r.block!=state.block)))allowed=false;
    if(config.rebuild&&!inside(p,config.region)){if(previous.size()!=size_t(count)||previous[i]!=int(s))allowed=false;}
    if(allowed)wave[i].set(s);
   }if(wave[i].none())throw std::runtime_error("No block fits cell "+std::to_string(p[0])+","+std::to_string(p[1])+","+std::to_string(p[2])+". Check boundaries, weights, volume and connectivity constraints.");queue.push_back(i);
  }
  if(!propagate())throw std::runtime_error("Initial constraints contradict neighboring sockets. No map changes were applied.");trail.clear();Result result;
  for(;;){check();int best=-1;double entropy=1e30;int resolved=0;
   for(int i=0;i<count;i++){if(wave[i].count()==1){resolved++;continue;}double total=0,weightedLog=0;for(size_t s=0;s<states.size();s++)if(wave[i][s]){double w=states[s].weight*config.groupWeights[states[s].group];total+=w;weightedLog+=w*std::log(w);}double h=std::log(total)-weightedLog/total+std::generate_canonical<double,32>(rng)*1e-6;if(h<entropy){entropy=h;best=i;}}
   progress=float(resolved)/count;if(best<0)break;
   int pick=choose(wave[best]);Decision decision{best,trail.size(),wave[best]};decision.alternatives.reset(pick);decisions.push_back(decision);Domain one;one.set(pick);assign(best,one);result.decisions++;
   while(!propagate()){
    bool retry=false;while(!decisions.empty()){auto& d=decisions.back();while(trail.size()>d.marker){auto change=trail.back();trail.pop_back();wave[change.cell]=change.before;}queue.clear();if(d.alternatives.any()){pick=choose(d.alternatives);d.alternatives.reset(pick);one.reset();one.set(pick);assign(d.cell,one);retry=true;break;}decisions.pop_back();}
    if(!retry||++result.backtracks>8192)throw std::runtime_error("The constraints have no solution within the backtracking budget. Previous preview/map is retained.");
   }
  }
  for(auto domain:wave){int picked=-1;for(size_t s=0;s<states.size();s++)if(domain[s]){picked=s;break;}result.cells.push_back(picked);}result.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();progress=1;return result;
 }
};
}
