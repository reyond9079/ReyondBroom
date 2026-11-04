// SPDX-License-Identifier: MIT
#include "reyondbroom_plugin.h"
#include "growth.h"
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
static std::string texture(bool leaf){const int size=64;std::string image(18+size*size*4,'\0');image[2]=2;image[12]=size;image[14]=size;image[16]=32;image[17]=0x28;for(int y=0;y<size;y++)for(int x=0;x<size;x++){size_t p=18+(y*size+x)*4;double shade=.7+.2*std::sin(x*.7+y*.4);if(leaf){bool vein=std::abs(x-size/2)<2||std::abs(std::fmod(y*.6+std::abs(x-32),13.)-6)<1;image[p]=char(vein?38:25*shade);image[p+1]=char(vein?116:94*shade);image[p+2]=char(vein?60:40*shade);}else{image[p]=char(28*shade);image[p+1]=char(49*shade);image[p+2]=char(73*shade);}image[p+3]=char(255);}return image;}
static void create(){
 if(state||growth.nodes.size()<2){message="Grow ivy first.";return;}
 try{
  if(host->number("rb.document_generation")!=sourceGeneration)throw std::runtime_error("The document changed. Grow ivy on the current target surfaces first.");
  for(float v:{thickness,leafSize,leafDensity,leafVariation})if(!std::isfinite(v))throw std::runtime_error("Geometry values must be finite.");
  if(thickness<.6||thickness>16||leafSize<2||leafSize>64||leafDensity<0||leafDensity>1||leafVariation<0||leafVariation>1)throw std::runtime_error("Use branch radius 0.6..16, leaf size 2..64 and density/variation 0..1.");
  if(!branches&&!leaves)throw std::runtime_error("Enable branches or leaves.");
  std::vector<std::vector<V>> pieces;std::vector<bool> leafPiece;std::mt19937 rng(settings.seed+9817);auto random=[&]{return double(rng())/4294967296.;};
  for(size_t i=1;i<growth.nodes.size();i++){auto n=growth.nodes[i];if(branches){double radius=std::max(.6,double(thickness)*(1-double(n.depth)/std::max(1,settings.iterations)*.65));pieces.push_back(tube(growth.nodes[n.parent].p,n.p,radius));leafPiece.push_back(false);}
   if(leaves&&random()<leafDensity){V direction=unit(n.p-growth.nodes[n.parent].p),normal=unit(n.normal),side=unit(cross(normal,direction));double angle=(random()-.5)*2;side=unit(side+direction*angle);V along=unit(cross(side,normal));double size=leafSize*(1+leafVariation*(random()*2-1));V center=n.p+side*(size*.4)*(i%2?1:-1)+normal*2;
    std::vector<V> p;for(double depth:{-.6,.6})for(V delta:{along*size,side*(size*.55),along*(-size*.7),side*(-size*.55)})p.push_back(center+delta+normal*depth);pieces.push_back(p);leafPiece.push_back(true);
   }
  }
  if(pieces.empty())throw std::runtime_error("The chosen density produced no geometry.");
  std::string map="{\n\"classname\" \"worldspawn\"\n";char tex[512]{};host->string("ca_newbrushtexture",tex,sizeof tex);
  if(output==0){if(pieces.size()>1800)throw std::runtime_error("Brush output is limited to 1800 pieces. Reduce iterations/branches/leaves or choose OBJ model.");for(size_t k=0;k<pieces.size();k++){auto mat=leafPiece[k]?leafMaterial:branchMaterial;map+=convex(pieces[k],*mat?mat:tex);}map+="}\n";}
  else{
   char modelClass[128]{};host->string("rb.model_class",modelClass,sizeof modelClass);if(!*modelClass)throw std::runtime_error("This profile has no native OBJ placement entity. Choose Brush geometry, or convert the model in Model editor first.");
   auto serial=std::chrono::system_clock::now().time_since_epoch().count();std::string name="ivy-"+std::to_string(serial),path="models/reyond-plugins/"+name;V origin=growth.nodes[0].p;std::ostringstream obj;obj<<"mtllib "<<name<<".mtl\n";int index=1;
   // Export convex pieces as indexed polygon caps/sides, retaining world scale.
   std::vector<size_t> order(pieces.size());std::iota(order.begin(),order.end(),0);std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return leafPiece[a]<leafPiece[b];});int previousMaterial=-1,orientation=int(host->number("mod_obj_orientation"));
   size_t pieceIndex=0;for(size_t k:order){auto& p=pieces[k];int half=int(p.size()/2);if(pieceIndex++%400==0)obj<<"g ivy_batch_"<<pieceIndex/400<<'\n';if(int(leafPiece[k])!=previousMaterial){previousMaterial=int(leafPiece[k]);obj<<"usemtl textures/reyond-plugins/"<<(leafPiece[k]?"ivy_leaf":"ivy_bark")<<"\n";}V center{};for(V v:p)center=center+v;center=center/double(p.size());for(V v:p){v=v-origin;if(orientation==1)std::swap(v.y,v.z);else if(orientation==2)v={v.y,v.z,v.x};obj<<"v "<<v.x<<' '<<v.y<<' '<<v.z<<'\n';}for(int i=0;i<int(p.size());i++){double angle=(i%half)*6.28318530718/half;obj<<"vt "<<(.5+.5*std::cos(angle))<<' '<<(.5+.5*std::sin(angle))<<'\n';}for(V v:p){v=unit(v-center);if(orientation==1)std::swap(v.y,v.z);else if(orientation==2)v={v.y,v.z,v.x};obj<<"vn "<<v.x<<' '<<v.y<<' '<<v.z<<'\n';}
    auto face=[&](std::vector<int> f){for(int pass=0;pass<2;pass++){obj<<"f";for(int n:f)obj<<' '<<index+n<<'/'<<index+n<<'/'<<index+n;obj<<'\n';std::reverse(f.begin(),f.end());}};
    std::vector<int> cap;for(int j=0;j<half;j++)cap.push_back(j);face(cap);for(int& v:cap)v+=half;face(cap);for(int j=0;j<half;j++)face({j,(j+1)%half,(j+1)%half+half,j+half});index+=int(p.size());
   }
   std::string mtl="newmtl textures/reyond-plugins/ivy_leaf\nKd 0.25 0.65 0.2\nmap_Kd ../../textures/reyond-plugins/ivy_leaf.tga\nnewmtl textures/reyond-plugins/ivy_bark\nKd 0.35 0.22 0.1\nmap_Kd ../../textures/reyond-plugins/ivy_bark.tga\n";
   for(bool leaf:{false,true}){auto bytes=texture(leaf);if(!host->write_asset(leaf?"textures/reyond-plugins/ivy_leaf.tga":"textures/reyond-plugins/ivy_bark.tga",bytes.data(),bytes.size()))throw std::runtime_error("Cannot write the ivy textures.");}
   auto bytes=obj.str();if(!host->write_asset((path+".obj").c_str(),bytes.data(),bytes.size())||!host->write_asset((path+".mtl").c_str(),mtl.data(),mtl.size()))throw std::runtime_error("Cannot save the generated OBJ/MTL assets.");
   std::ostringstream entity;entity<<"}\n{\n\"classname\" \""<<modelClass<<"\"\n\"model\" \""<<path<<".obj\"\n\"origin\" \""<<origin.x<<' '<<origin.y<<' '<<origin.z<<"\"\n}\n";map+=entity.str();
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
 host->separator("Geometry");host->checkbox("Branches",&branches);host->input_float("Branch radius",&thickness);host->checkbox("Leaves",&leaves);host->input_float("Leaf size",&leafSize);host->input_float("Leaf density",&leafDensity);host->input_float("Leaf size variation",&leafVariation);host->combo("Output",&output,"Brush geometry (game-native)\0Textured OBJ model (FTE / Q3)\0");
 if(!message.empty())host->text(message.c_str());
 if(!output){host->input_text("Branch material (empty: current)",branchMaterial,sizeof branchMaterial);host->input_text("Leaf material (empty: current)",leafMaterial,sizeof leafMaterial);}
 if(output)host->text("OBJ uses original generated bark/leaf textures. FTE supports this model directly. Vanilla Quake engines require converting it to MDL; brush output works without model conversion.");
}
static void shutdown(){state=0;host->preview_lines(nullptr,0);}
extern "C" RB_PLUGIN_EXPORT const RBPluginV1* ReyondBroomPluginInit(const RBHostV1* api){if(!api||api->api_version!=1||api->struct_size<sizeof(RBHostV1))return nullptr;host=api;static RBPluginV1 plugin{1,sizeof(RBPluginV1),"org.reyondbroom.ivy",draw,tick,busy,shutdown};return &plugin;}
