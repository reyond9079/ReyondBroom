// SPDX-License-Identifier: MIT
#include "reyondbroom_plugin.h"
#include "growth.h"
#include "model.h"
#include <chrono>
using namespace rbgen;
static const RBHostV1* host;static ivy::Growth growth;static ivy::Settings settings;
static int state=0,autoRoot=1,branches=1,leaves=1,output=1;static float root[3]{},step=8,primary=.65f,randomWeight=.3f,adhesion=.85f,gravity=.12f,up=.35f,branching=.035f,maxFloat=96,thickness=2,leafSize=10,leafDensity=.65f,leafVariation=.45f;
static std::string message;static std::vector<float> lines;
static char branchMaterial[256]{},leafMaterial[256]{};
static double sourceGeneration=0;static std::chrono::steady_clock::time_point captureStarted;
static void updatePreview(){lines.clear();for(auto n:growth.nodes)if(n.parent>=0){V a=growth.nodes[n.parent].p;for(V p:{a,n.p}){lines.push_back(float(p.x));lines.push_back(float(p.y));lines.push_back(float(p.z));}}host->preview_lines(lines.data(),lines.size()/6);}
static void tick(){
 if(state&&host->number("rb.document_generation")!=sourceGeneration){state=0;growth.nodes.clear();host->preview_lines(nullptr,0);message="The document changed. Select the target surfaces and Grow again.";return;}
 if(state==1){int status=int(host->number("ftebroom_plugin_surfaces_status"));if(!status){if(std::chrono::steady_clock::now()-captureStarted>std::chrono::seconds(5)){state=0;message="Surface capture timed out. Wait for the map to load, then Grow again.";}return;}state=0;if(status<0){message="Select 1..512 target brushes or patches first (up to 16384 surface triangles).";return;}
  int size=host->read_data("plugin-surfaces.txt",nullptr,0);if(size<=0){message="Cannot read the selected surfaces.";return;}std::string text(size+1,'\0');if(host->read_data("plugin-surfaces.txt",text.data(),text.size())<0){message="Surface snapshot is incomplete.";return;}
  growth.surface.triangles.clear();std::istringstream in(text);std::string kind;bool complete=false;
  while(in>>kind){if(kind=="END"){complete=true;break;}ivy::Triangle t;if(kind!="T"||!(in>>t.a.x>>t.a.y>>t.a.z>>t.b.x>>t.b.y>>t.b.z>>t.c.x>>t.c.y>>t.c.z>>t.normal.x>>t.normal.y>>t.normal.z))break;if(length(cross(t.b-t.a,t.c-t.a))>.001){t.normal=unit(t.normal);growth.surface.triangles.push_back(t);}if(growth.surface.triangles.size()>16384)break;}
  if(!complete||growth.surface.triangles.empty()){message="Invalid target surfaces.";return;}growth.surface.build();V start{root[0],root[1],root[2]};
  if(autoRoot){auto box=growth.surface.nodes[0];start={(box.lo.x+box.hi.x)*.5,box.lo.y-8,box.lo.z+std::min(24.,(box.hi.z-box.lo.z)*.75)};}
  settings.maxNodes=!output&&branches&&leaves?900:1800;settings.step=step;settings.primary=primary;settings.random=randomWeight;settings.adhesion=adhesion;settings.gravity=gravity;settings.up=up;settings.branchProbability=branching;settings.maxFloat=maxFloat;settings.offset=std::max(2.,double(thickness)+1);growth.start(start,settings);state=2;message="Growing ivy...";
 }
 if(state==2){for(int i=0;i<5;i++)if(!growth.advance()){state=0;message="Growth finished. Adjust settings and Grow again, or create the geometry.";break;}updatePreview();}
}
static std::vector<V> tube(V a,V b,double radius){V d=unit(b-a),u=unit(cross(d,std::abs(d.z)>.8?V{1,0,0}:V{0,0,1})),v=cross(d,u);std::vector<V> p;for(int end=0;end<2;end++)for(int i=0;i<5;i++){double angle=i*6.28318530718/5;p.push_back((end?b:a)+(u*std::cos(angle)+v*std::sin(angle))*radius*(end?.85:1));}return p;}
static void create(){
 if(state||growth.nodes.size()<2){message="Grow ivy first.";return;}
 try{
  if(host->number("rb.document_generation")!=sourceGeneration)throw std::runtime_error("The document changed. Grow ivy on the current target surfaces first.");
  for(float v:{thickness,leafSize,leafDensity,leafVariation})if(!std::isfinite(v))throw std::runtime_error("Geometry values must be finite.");
  if(thickness<.6||thickness>16||leafSize<2||leafSize>64||leafDensity<0||leafDensity>1||leafVariation<0||leafVariation>1)throw std::runtime_error("Use branch radius 0.6..16, leaf size 2..64 and density/variation 0..1.");
  if(!branches&&!leaves)throw std::runtime_error("Enable branches or leaves.");
  std::vector<std::vector<V>> pieces;std::vector<bool> leafPiece;std::mt19937 rng(settings.seed+9817);auto random=[&]{return double(rng())/4294967296.;};
  for(size_t i=1;i<growth.nodes.size();i++){auto n=growth.nodes[i];if(branches){double radius=std::max(.6,double(thickness)*(1-double(n.depth)/std::max(1,settings.iterations)*.65));pieces.push_back(tube(growth.nodes[n.parent].p,n.p,radius));leafPiece.push_back(false);}
   if(leaves&&random()<leafDensity){V direction=unit(n.p-growth.nodes[n.parent].p),normal=unit(n.normal),side=cross(normal,direction);if(length(side)<.01)side=cross(normal,std::abs(normal.z)>.8?V{1,0,0}:V{0,0,1});side=unit(side);double angle=(random()-.5)*2;side=unit(side+direction*angle);V along=unit(cross(side,normal));double size=leafSize*(1+leafVariation*(random()*2-1));V center=n.p+side*(size*.4)*(i%2?1:-1)+normal*2;
    std::vector<V> p;for(double depth:{-.6,.6}){
      if(!output)for(V delta:{along*size,side*(size*.55),along*(-size*.7),side*(-size*.55)})p.push_back(center+delta+normal*depth);
      else for(auto uv:ivy::outline)p.push_back(center+along*(uv[1]*size)+side*(uv[0]*size)+normal*(depth+std::abs(uv[0])*size*.12));
    }pieces.push_back(p);leafPiece.push_back(true);
   }
  }
  if(pieces.empty())throw std::runtime_error("The chosen density produced no geometry.");
  std::string map="{\n\"classname\" \"worldspawn\"\n";char tex[512]{};host->string("ca_newbrushtexture",tex,sizeof tex);
  if(output==0){if(pieces.size()>1800)throw std::runtime_error("Brush output is limited to 1800 pieces. Reduce iterations/branches/leaves or choose OBJ model.");for(size_t k=0;k<pieces.size();k++){auto mat=leafPiece[k]?leafMaterial:branchMaterial;map+=convex(pieces[k],*mat?mat:tex);}map+="}\n";}
  else{
   const int profile=int(host->number("rb.profile"));const char* extension=profile==2||profile==4?".mdl":profile==3?".md2":".md3";
   const auto serial=std::chrono::system_clock::now().time_since_epoch().count();const std::string path="models/reyond-plugins/ivy-"+std::to_string(serial);
   const std::string bark=*branchMaterial?branchMaterial:"textures/reyond-plugins/ivy_bark_v2.tga",leaf=*leafMaterial?leafMaterial:"textures/reyond-plugins/ivy_leaf_v2.tga";
   for(bool isLeaf:{false,true})if(!(isLeaf?*leafMaterial:*branchMaterial)){
    auto bytes=ivy::texture(isLeaf);if(!host->write_asset((isLeaf?leaf:bark).c_str(),bytes.data(),bytes.size()))throw std::runtime_error("Cannot write the ivy textures.");
   }
   char modelClass[128]{};host->string("rb.model_class",modelClass,sizeof modelClass);
   ivy::Model model;V low{1e30,1e30,1e30},high{-1e30,-1e30,-1e30};int part=0;map+="}\n";
   auto flush=[&](){if(!model.vertices())return;V origin=(low+high)*.5;auto bytes=model.snapshot(origin,bark,leaf);auto name=path+"-"+std::to_string(part++)+extension;
    if(!host->write_model(name.c_str(),bytes.data(),bytes.size())){char error[2048]{};host->string("rb.last_error",error,sizeof error);throw std::runtime_error(*error?error:"Cannot export the generated native model.");}
    std::ostringstream entity;entity<<std::setprecision(9)<<"{\n\"classname\" \""<<modelClass<<"\"\n\"model\" \""<<name<<"\"\n\"origin\" \""<<origin.x<<' '<<origin.y<<' '<<origin.z<<"\"\n}\n";map+=entity.str();model={};low={1e30,1e30,1e30};high={-1e30,-1e30,-1e30};
   };
   // Split only at complete leaves/stems. Native coordinate precision, origin,
   // texture atlas and vertex limits are respected without resizing the ivy.
   for(size_t k=0;k<pieces.size();k++){
    V lo=low,hi=high;for(V p:pieces[k])for(int a=0;a<3;a++){lo[a]=std::min(lo[a],p[a]);hi[a]=std::max(hi[a],p[a]);}
    if(model.vertices()+pieces[k].size()+2>1800||std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z})>480)flush();
    for(V p:pieces[k])for(int a=0;a<3;a++){low[a]=std::min(low[a],p[a]);high[a]=std::max(high[a],p[a]);}model.append(pieces[k],leafPiece[k]);
   }flush();
  }
  if(!host->import_map(map.data(),map.size()))throw std::runtime_error("Cannot apply ivy outside MAP editing.");host->preview_lines(nullptr,0);message="Ivy import queued: "+std::to_string(pieces.size())+" pieces. Generated assets keep their dimensions and origin. Undo removes the inserted objects.";
 }catch(const std::exception& e){message=e.what();}
}
static int busy(){return state!=0;}
static void startGrow(){
  bool valid=settings.iterations>=1&&settings.iterations<=2000&&settings.maxBranches>=1&&settings.maxBranches<=64;
  for(float v:{step,primary,randomWeight,adhesion,up,gravity,branching,maxFloat,thickness,leafSize,leafDensity,leafVariation,root[0],root[1],root[2]})valid&=std::isfinite(v);
  valid&=step>=1&&step<=64&&primary>=0&&primary<=4&&randomWeight>=0&&randomWeight<=4&&adhesion>=0&&adhesion<=4&&up>=-4&&up<=4&&gravity>=0&&gravity<=4&&branching>=0&&branching<=1&&maxFloat>=step&&maxFloat<=1024&&thickness>=.6&&thickness<=16&&leafSize>=2&&leafSize<=64&&leafDensity>=0&&leafDensity<=1&&leafVariation>=0&&leafVariation<=1;
  if(valid){sourceGeneration=host->number("rb.document_generation");captureStarted=std::chrono::steady_clock::now();growth.nodes.clear();host->preview_lines(nullptr,0);host->set_number("ftebroom_plugin_surfaces_status",0);host->command("ftebroom_plugin_surfaces\n");state=1;message="Capturing selected target surfaces...";}else message="Invalid growth settings. Use 1..2000 iterations, 1..64 branches, step 1..64, probabilities 0..1, positive sizes and finite values.";
 }
static void draw(){
 host->text("Select target brushes. Grow follows their surfaces; the green lines are a non-destructive preview. Stop keeps the current growth. Create ivy commits it with Undo.");
 bool growing=state!=0;if(growing)host->disabled(1);if(host->button("Grow"))startGrow();if(growing)host->disabled(0);host->same_line();
 if(state){if(host->button("Stop")){state=0;message="Stopped; the current preview can be created.";}host->same_line();}
 bool cannotCreate=state||growth.nodes.size()<2;if(cannotCreate)host->disabled(1);if(host->button("Create ivy"))create();if(cannotCreate)host->disabled(0);host->same_line();
 if(host->button("Clear preview")){state=0;growth.nodes.clear();host->preview_lines(nullptr,0);}
 if(state)host->progress(float(growth.iteration)/std::max(1,settings.iterations),"Growth");
 growing=state!=0;if(growing)host->disabled(1);host->input_int("Seed",&settings.seed);host->checkbox("Root at selection base",&autoRoot);if(!autoRoot)for(int a=0;a<3;a++)host->input_float(a==0?"Root X":a==1?"Root Y":"Root Z",&root[a]);
 host->separator("Growth");host->input_int("Iterations",&settings.iterations);host->input_int("Maximum branches",&settings.maxBranches);host->input_float("Growth step",&step);if(host->section("Advanced growth",0)){host->input_float("Primary direction",&primary);host->input_float("Random influence",&randomWeight);host->input_float("Adhesion",&adhesion);host->input_float("Phototropism (up)",&up);host->input_float("Gravity",&gravity);host->input_float("Branch probability",&branching);host->input_float("Maximum floating length",&maxFloat);}

 if(growing)host->disabled(0);
 host->separator("Geometry");host->checkbox("Branches",&branches);host->input_float("Branch radius",&thickness);host->checkbox("Leaves",&leaves);host->input_float("Leaf size",&leafSize);host->input_float("Leaf density",&leafDensity);host->input_float("Leaf size variation",&leafVariation);host->combo("Output",&output,"Brush geometry (game-native)\0Native model (automatic format)\0");
 if(!message.empty())host->text(message.c_str());
 host->material_picker("Branch material",branchMaterial,sizeof branchMaterial);host->material_picker("Leaf material",leafMaterial,sizeof leafMaterial);
 if(output){int profile=int(host->number("rb.profile"));host->text(profile==2?"Quake MDL with an embedded paletted skin.":profile==3?"Quake II MD2 with a PCX skin. In-game placement requires a mod with misc_model support.":profile==4?"GoldSrc MDL with embedded textures.":"MD3 model with material textures. Large growth is split into native-sized model parts without changing world size.");}
}
static void shutdown(){state=0;host->preview_lines(nullptr,0);}
extern "C" RB_PLUGIN_EXPORT const RBPluginV1* ReyondBroomPluginInit(const RBHostV1* api){if(!api||api->api_version!=1||api->struct_size<sizeof(RBHostV1))return nullptr;host=api;static RBPluginV1 plugin{1,sizeof(RBPluginV1),"org.reyondbroom.ivy",draw,tick,busy,shutdown};return &plugin;}
