// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
static void plantFieldWidth(const char* name){ImGui::SetNextItemWidth(std::max(80.f,ImGui::GetContentRegionAvail().x-ImGui::CalcTextSize(name).x-ImGui::GetStyle().ItemInnerSpacing.x-2));}
static bool plantCombo(const char* name,int& value,const char* items){plantFieldWidth(name);bool changed=ImGui::Combo(name,&value,items);plantRect(name);return changed;}
static bool plantSlider(const char* name,double& value,double low,double high,const char* format="%.2f") {
  plantFieldWidth(name);float v=value;bool changed=ImGui::SliderFloat(name,&v,low,high,format);if(changed)value=v;plantRect(name);return changed;
}
static bool plantInt(const char* name,int& value,int low,int high){plantFieldWidth(name);bool changed=ImGui::SliderInt(name,&value,low,high);plantRect(name);return changed;}
static bool plantText(const char* name,std::string& text) {
  std::array<char,512> buffer{};std::snprintf(buffer.data(),buffer.size(),"%s",text.c_str());
  plantFieldWidth(name);bool changed=ImGui::InputText(name,buffer.data(),buffer.size());if(changed)text=buffer.data();plantRect(name);return changed;
}
static bool plantButton(const char* label,const char* key=nullptr){bool b=ImGui::Button(label);plantRect(key?key:label);return b;}
static void plantColor(const char* name,plants::V& color) {
  plantFieldWidth(name);float v[3]={float(color[0]),float(color[1]),float(color[2])};if(ImGui::ColorEdit3(name,v))for(int i=0;i<3;i++)color[i]=v[i];plantRect(name);
}
static void plantProperties() {
  auto& s=plantState;auto& p=s.doc.p;auto f=s.doc.family;
  bool tree=f==plants::Family::Broadleaf||f==plants::Family::Bush||f==plants::Family::Custom;
  bool leafy=tree||f==plants::Family::Fern||f==plants::Family::Palm||f==plants::Family::Bamboo||f==plants::Family::Vine||f==plants::Family::Flower;
  if(ImGui::CollapsingHeader("General",ImGuiTreeNodeFlags_DefaultOpen)) {
    plantText("Plant name",s.doc.name);ImGui::TextDisabled("%s",plants::familyName(f));
    plantSlider("Height",p.height,1,480,"%.1f units");plantSlider("Age / growth",p.age,.1,1);
    plantCombo("Quality",p.quality,"Realistic\0Optimized Game-Ready\0Low Poly\0Stylized\0");
    if(f!=plants::Family::Mushroom)plantCombo("Season",p.season,"Spring\0Summer\0Autumn\0Winter\0");
    if(tree||f==plants::Family::Conifer||f==plants::Family::Grass)plantSlider("Foliage density",p.density,.1,1.8);
    if(f!=plants::Family::Grass)plantSlider("Stem radius",p.radius,.1,std::min(100.,p.height*.40),"%.2f units");
  }
  if(s.advanced&&ImGui::CollapsingHeader("Shape / growth")) {
    if(tree||f==plants::Family::Conifer||f==plants::Family::Cactus||f==plants::Family::Mushroom)plantSlider("Crown / cap spread",p.spread,.1,1.5);
    if(f!=plants::Family::Mushroom&&s.doc.species!="barrel-cactus")plantSlider("Curvature",p.curvature,0,1.5);
    if(tree) {plantSlider("Apical taper",p.taper,.1,.98);plantSlider("Root flare",p.rootFlare,0,3);}
    if(tree||f==plants::Family::Conifer||f==plants::Family::Fern||f==plants::Family::Palm)plantSlider("Gravity / droop",p.gravity,-.2,1.7);
    if(tree)plantSlider("Light tropism",p.light,-.5,1);
    plantSlider("Organic irregularity",p.irregularity,0,1);
    if(tree) {
      ImGui::TextUnformatted("Branch distribution curve");float curve[5];for(int i=0;i<5;i++)curve[i]=p.growth[i];ImGui::PlotLines("##growth-plot",curve,5,0,nullptr,0,2,{0,50});
      for(int i=0;i<5;i++){ImGui::PushID(i);float v=p.growth[i];if(ImGui::SliderFloat("Growth sample",&v,0,2))p.growth[i]=v;ImGui::PopID();}
    }
    if(tree||f==plants::Family::Palm||f==plants::Family::Vine) {
      ImGui::Checkbox("Edit trunk control points",&s.handles);
      for(int i=1;i<4;i++){ImGui::PushID(i);float v[3]={float(p.controls[i][0]),float(p.controls[i][1]),float(p.controls[i][2])};if(ImGui::DragFloat3("Spline control",v,.005f,-1.5f,1.5f))for(int a=0;a<3;a++)p.controls[i][a]=v[a];ImGui::PopID();}
    }
  }
  if(s.advanced&&ImGui::CollapsingHeader("Branching / stems")) {
    if(tree||f==plants::Family::Conifer||f==plants::Family::Palm||f==plants::Family::Fern||(f==plants::Family::Cactus&&s.doc.species!="barrel-cactus")||f==plants::Family::Bamboo||f==plants::Family::Grass)plantInt("Branches / fronds",p.branches,f==plants::Family::Cactus?0:3,f==plants::Family::Cactus?6:40);
    if(tree){plantInt("Branching depth",p.depth,1,f==plants::Family::Bush?2:4);plantSlider("Branch angle",p.branchAngle,15,110,"%.0f degrees");plantSlider("Branch length ratio",p.branchLength,.2,1.3);plantSlider("Branch thickness ratio",p.branchTaper,.2,.9);plantSlider("Dead twig fraction",p.dead,0,.8);}
    if(f==plants::Family::Bush||f==plants::Family::Flower||f==plants::Family::Mushroom||f==plants::Family::Bamboo||f==plants::Family::Grass)plantInt("Clump / stem count",p.clumps,1,20);
  }
  if(leafy||f==plants::Family::Conifer||f==plants::Family::Grass)if(ImGui::CollapsingHeader("Leaves / needles",ImGuiTreeNodeFlags_DefaultOpen)) {
    if(f!=plants::Family::Grass)plantSlider("Leaf / leaflet size",p.leafSize,.5,60,"%.1f units");
    plantSlider("Leaf / blade width",p.leafWidth,.015,1.5);
    if(s.advanced) {
      plantSlider("Leaf curl",p.leafCurl,-.5,1);plantSlider("Leaf twist",p.leafTwist,-1,1);
      if(tree||f==plants::Family::Fern||f==plants::Family::Palm||f==plants::Family::Conifer||f==plants::Family::Vine)plantInt("Leaves / shoot",p.leaves,3,40);
      if(f!=plants::Family::Conifer&&f!=plants::Family::Grass)plantCombo("Leaf shape",p.leafShape,"Oval\0Elliptical\0Lanceolate\0Lobed\0Palmate\0Needle\0Custom profile\0Compound\0");
      if(p.leafShape==6)for(int i=0;i<5;i++){ImGui::PushID(i);float x=p.leafProfile[i];if(ImGui::SliderFloat("Leaf outline",&x,0,1.5))p.leafProfile[i]=x;ImGui::PopID();}
      if(f!=plants::Family::Conifer)plantCombo("Foliage geometry",p.foliageMode,"Detailed curved leaf\0Curved low-poly leaf\0Alpha-cutout card\0Crossed cards\0Distant foliage cards\0");
    }
  }
  if(f==plants::Family::Flower||f==plants::Family::Cactus||s.doc.species=="flowering-shrub")if(ImGui::CollapsingHeader("Flowers",ImGuiTreeNodeFlags_DefaultOpen)) {
    plantSlider("Flower diameter",p.flowerSize,1,70);plantSlider("Bloom stage",p.bloom,0,1);plantInt("Petals",p.petals,3,40);plantInt("Petal layers",p.flowerLayers,1,8);
  }
  if(f==plants::Family::Mushroom&&ImGui::CollapsingHeader("Mushroom surface",ImGuiTreeNodeFlags_DefaultOpen)){plantSlider("Cap thickness",p.capThickness,.03,.8);plantSlider("Gills / pores",p.gills,0,1);}
  if(f==plants::Family::Cactus&&ImGui::CollapsingHeader("Cactus ribs / spines",ImGuiTreeNodeFlags_DefaultOpen)){plantSlider("Rib count",p.ribs,3,24,"%.0f");plantSlider("Spine length",p.spines,0,1);}
  if(ImGui::CollapsingHeader("Materials / textures",ImGuiTreeNodeFlags_DefaultOpen)) {
    if(f!=plants::Family::Grass)plantColor(f==plants::Family::Mushroom?"Stem / gill color":f==plants::Family::Cactus?"Spine color":"Bark / stem color",p.bark);
    if(f!=plants::Family::Mushroom)plantColor("Foliage color",p.leaf);
    if(f==plants::Family::Mushroom||f==plants::Family::Flower||f==plants::Family::Cactus||s.doc.species=="flowering-shrub")plantColor("Flower / cap color",p.flower);
    if(s.advanced) {
      plantSlider("Surface roughness",p.roughness,.05,1);plantSlider("Ambient occlusion",p.ao,0,1);plantSlider("Color variation",p.variation,0,.6);
      if(f!=plants::Family::Mushroom)plantSlider("Dryness",p.dryness,0,1);
      if(leafy||f==plants::Family::Conifer||f==plants::Family::Grass)if(plantButton("Import leaf albedo...","import-texture"))runUiChoose(s.doc.texture,[](const std::string& path){auto before=plants::serialize(plantState.doc);plantState.doc.texture=path;plantChanged(before);});
      if(!s.doc.texture.empty()){ImGui::TextWrapped("%s",s.doc.texture.c_str());if(plantButton("Use procedural leaf texture")){s.doc.texture.clear();}}
      ImGui::TextWrapped("Preview: directional vertex shading + alpha cutout. GLB retains normal, roughness and AO maps.");
    }
  }
  if(s.advanced&&ImGui::CollapsingHeader("Wind preview")) {
    if(ImGui::Checkbox("Animate wind",&s.wind))s.dirtyRender=true;
    plantSlider("Wind strength",p.windStrength,0,2);plantSlider("Wind direction",p.windDirection,-180,180,"%.0f degrees");
    plantSlider("Wind frequency",p.windFrequency,.1,4);plantSlider("Branch stiffness",p.stiffness,0,1);plantSlider("Leaf flutter",p.flutter,0,1);
    ImGui::TextWrapped("Export uses the undeformed static pose. Runtime wind weights are not assumed by Quake profiles.");
  }
  if(s.advanced&&ImGui::CollapsingHeader("Geometry / LOD")) {
    plantInt("Triangle budget",p.budget,3000,100000);if(f!=plants::Family::Grass&&f!=plants::Family::Cactus)plantInt("Radial sides",p.sides,3,20);if(f!=plants::Family::Grass)plantInt("Spline segments",p.segments,3,16);
    plantSlider("LOD1 budget ratio",p.lod1Ratio,.25,.8);plantSlider("LOD2 budget ratio",p.lod2Ratio,.05,std::min(.4,p.lod1Ratio-.02));
    if(s.result)for(int i=0;i<3;i++)ImGui::Text("LOD%i: %zu triangles",i,s.result->lods[i].faces.size());
  }
  if(ImGui::CollapsingHeader("Export / target")) {
    ImGui::TextWrapped("OBJ + PNG / GLB: portable editable geometry, UVs, normals and material textures. Add to Map installs MD3 + cutout shader.");
    if(gameProfile==2)ImGui::Checkbox("Use FTE static models for Quake 1",&s.allowFteQ1);
    if(gameProfile==2)ImGui::TextWrapped("Vanilla Quake needs an MDL-compatible mod/entity. Convert OBJ with Model Editor for that target.");
    if(gameProfile==3||gameProfile==4)ImGui::TextWrapped("Quake II / GoldSrc require format conversion in Model Editor before placement.");
  }
}
static void plantStructureNode(size_t index) {
  auto& s=plantState;auto& n=s.doc.nodes[index];ImGui::PushID(n.id.c_str());
  bool children=std::any_of(s.doc.nodes.begin(),s.doc.nodes.end(),[&](auto& x){return x.parent==n.id;});
  ImGui::Checkbox("##visible",&n.visible);plantRect("node-visible/"+n.id);ImGui::SameLine();
  ImGuiTreeNodeFlags flags=ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_DefaultOpen;
  if(!children)flags|=ImGuiTreeNodeFlags_Leaf;if(int(index)==s.selected)flags|=ImGuiTreeNodeFlags_Selected;
  bool expanded=ImGui::TreeNodeEx("node",flags,"%s",n.name.c_str());plantRect("node/"+n.id);
  if(ImGui::IsItemClicked()&&!ImGui::IsItemToggledOpen())s.selected=index;
  if(expanded){for(size_t i=0;i<s.doc.nodes.size();i++)if(s.doc.nodes[i].parent==n.id)plantStructureNode(i);ImGui::TreePop();}
  ImGui::PopID();
}
static void plantStructure() {
  auto& s=plantState;
  for(size_t i=0;i<s.doc.nodes.size();i++)if(s.doc.nodes[i].parent.empty())plantStructureNode(i);
  if(s.selected>=0&&s.selected<int(s.doc.nodes.size())) {
    auto& n=s.doc.nodes[s.selected];ImGui::Separator();plantText("Component name",n.name);
    float offset[3],scale[3];for(int i=0;i<3;i++){offset[i]=n.offset[i];scale[i]=n.scale[i];}
    plantFieldWidth("Offset XYZ");if(ImGui::DragFloat3("Offset XYZ",offset,.25f,-4096,4096))for(int i=0;i<3;i++)n.offset[i]=offset[i];
    plantFieldWidth("Scale XYZ");if(ImGui::DragFloat3("Scale XYZ",scale,.01f,.05f,8))for(int i=0;i<3;i++)n.scale[i]=scale[i];
    plantSlider("Component rotation",n.rotation,-180,180,"%.0f degrees");
    if(plantButton("Duplicate component","duplicate-component")) {
      plantAction([&](){s.selected=plants::duplicateComponent(s.doc,size_t(s.selected));});
    }ImGui::SameLine();
    if(plantButton("Delete","delete-component")) {
      auto id=s.doc.nodes[s.selected].id,parent=s.doc.nodes[s.selected].parent;
      for(auto& child:s.doc.nodes)if(child.parent==id)child.parent=parent;
      s.doc.nodes.erase(s.doc.nodes.begin()+s.selected);s.selected=std::min<int>(s.selected,s.doc.nodes.size()-1);
    }
  }
  ImGui::BeginDisabled(s.doc.nodes.size()>=64);if(plantButton("Add component","add-component"))ImGui::OpenPopup("Plant component kind");ImGui::EndDisabled();
  if(ImGui::BeginPopup("Plant component kind")){for(int i=0;i<8;i++){
    bool available=s.result&&std::any_of(s.result->baseLods[0].faces.begin(),s.result->baseLods[0].faces.end(),[&](auto& face){return int(face.part)==i;});
    if(ImGui::MenuItem(plants::partName(plants::Part(i)),nullptr,false,available)){
    plants::Component n;n.id="component-"+std::to_string(s.revision)+"-"+std::to_string(s.doc.nodes.size());n.name=plants::partName(plants::Part(i));n.kind=plants::Part(i);
    s.doc.nodes.push_back(n);s.selected=s.doc.nodes.size()-1;}}ImGui::EndPopup();}
}
static void plantLibrary() {
  auto& s=plantState;
  ImGui::InputTextWithHint("##plant-search","Search species...",s.search.data(),s.search.size());plantRect("search");
  if(ImGui::BeginCombo("Category",s.category<0?"All plants":plants::familyName(plants::Family(s.category)))) {
    if(ImGui::Selectable("All plants",s.category<0))s.category=-1;
    for(int i=0;i<12;i++)if(ImGui::Selectable(plants::familyName(plants::Family(i)),s.category==i))s.category=i;ImGui::EndCombo();
  }
  ImGui::Combo("Show",&s.filter,"All presets\0Favorites\0Recent\0User presets\0");
  float listHeight=std::max(110.f,ImGui::GetContentRegionAvail().y*(s.advanced?.46f:.82f));
  ImGui::BeginChild("Plant preset list",{0,listHeight},ImGuiChildFlags_Borders);
  std::string search=entityLower(s.search.data());
  for(auto& d:s.library) {
    if(s.category>=0&&int(d.family)!=s.category&&!(s.category==int(plants::Family::Custom)&&s.userFiles.count(d.preset)))continue;
    if(!search.empty()&&entityLower(d.name).find(search)==std::string::npos)continue;
    if(s.filter==1&&std::find(s.favorites.begin(),s.favorites.end(),d.preset)==s.favorites.end())continue;
    if(s.filter==2&&std::find(s.recent.begin(),s.recent.end(),d.preset)==s.recent.end())continue;
    if(s.filter==3&&!s.userFiles.count(d.preset))continue;
    ImGui::PushID(d.preset.c_str());auto start=ImGui::GetCursorScreenPos();
    int thumb=ImGui::IsRectVisible({52,55*uiScale()})?plantThumbnail(d):0;
    bool selected=ImGui::Selectable("##preset",s.doc.preset==d.preset,ImGuiSelectableFlags_None,{0,55*uiScale()});plantRect("preset/"+d.preset);
    if(thumb)ImGui::GetWindowDrawList()->AddImage((ImTextureID)thumb,start,{start.x+52,start.y+52});
    auto color=ImGui::GetColorU32(ImGuiCol_Text);ImGui::GetWindowDrawList()->AddText({start.x+58,start.y+5},color,d.name.c_str());
    ImGui::GetWindowDrawList()->AddText({start.x+58,start.y+24},ImGui::GetColorU32(ImGuiCol_TextDisabled),s.userFiles.count(d.preset)?"User preset":plants::familyName(d.family));
    if(selected)plantSelect(d);ImGui::PopID();
  }ImGui::EndChild();
  bool favorite=std::find(s.favorites.begin(),s.favorites.end(),s.doc.preset)!=s.favorites.end();
  if(plantButton(favorite?"Unfavorite":"Favorite","favorite")){s.favorites.erase(std::remove(s.favorites.begin(),s.favorites.end(),s.doc.preset),s.favorites.end());if(!favorite)s.favorites.push_back(s.doc.preset);plantLibrarySave();}
  ImGui::SameLine();if(plantButton("Save preset","save-preset"))plantAction(plantPresetSave);
  if(plantButton("Manage preset...","manage-preset"))ImGui::OpenPopup("Plant preset actions");
  if(ImGui::BeginPopup("Plant preset actions")){
    if(ImGui::MenuItem("Duplicate"))plantAction([&](){s.doc.name+=" copy";s.doc.preset="custom";plantPresetSave();});
    if(ImGui::MenuItem("Reset parameters")){auto it=std::find_if(s.library.begin(),s.library.end(),[&](auto& d){return d.preset==s.doc.preset;});if(it!=s.library.end()){auto copy=*it;plantSelect(copy);}}
    ImGui::TextDisabled("Rename: edit Plant name, then Save preset.");ImGui::EndPopup();
  }
  if(s.userFiles.count(s.doc.preset)) {
    if(plantButton("Delete user preset","delete-preset"))plantAction([&](){auto path=s.userFiles[s.doc.preset];auto trash=plantFolder()/"trash";std::filesystem::create_directories(trash);std::filesystem::rename(path,trash/(std::to_string(s.revision)+"-"+path.filename().u8string()));plantLibraryLoad();});
  }
  if(s.advanced){ImGui::SeparatorText("Plant structure");ImGui::BeginChild("Plant structure tree",{0,0});plantStructure();ImGui::EndChild();}
}
struct PlantCamera {plants::V center,forward,right,up,eye;double distance,focal;};
static PlantCamera plantCamera() {
  auto& s=plantState;double y=s.yaw*plants::pi/180,p=s.pitch*plants::pi/180;
  PlantCamera c;c.center=plants::mul(plants::add(s.cameraLow,s.cameraHigh),.5);c.forward={std::cos(p)*std::cos(y),std::cos(p)*std::sin(y),-std::sin(p)};
  c.right={std::sin(y),-std::cos(y),0};c.up=plants::unit(plants::cross(c.right,c.forward));
  c.distance=std::max(24.,plants::length(plants::sub(s.cameraHigh,s.cameraLow))*1.4)*s.zoom;c.eye=plants::sub(c.center,plants::mul(c.forward,c.distance));c.focal=s.viewportSize.y/(2*std::tan(22.5*plants::pi/180));return c;
}
static bool plantProjectPoint(plants::V point,ImVec2& out) {
  auto& s=plantState;auto c=plantCamera();auto delta=plants::sub(point,c.eye);double depth=plants::dot(delta,c.forward);if(depth<=.1)return false;
  double scale=s.ortho?s.viewportSize.x/(std::max(16.,plants::length(plants::sub(s.cameraHigh,s.cameraLow)))*s.zoom):c.focal/depth;
  out={float(s.viewportMin.x+s.viewportSize.x*.5+plants::dot(delta,c.right)*scale),float(s.viewportMin.y+s.viewportSize.y*.5-plants::dot(delta,c.up)*scale)};return true;
}
static void plantPick() {
  auto& s=plantState;if(!s.result)return;auto c=plantCamera();auto mouse=ImGui::GetIO().MousePos;
  double x=mouse.x-s.viewportMin.x-s.viewportSize.x*.5,y=-(mouse.y-s.viewportMin.y-s.viewportSize.y*.5);
  auto origin=c.eye,ray=plants::unit(plants::add(c.forward,plants::add(plants::mul(c.right,x/c.focal),plants::mul(c.up,y/c.focal))));
  if(s.ortho){double scale=std::max(16.,plants::length(plants::sub(s.cameraHigh,s.cameraLow)))*s.zoom/s.viewportSize.x;origin=plants::add(origin,plants::add(plants::mul(c.right,x*scale),plants::mul(c.up,y*scale)));ray=c.forward;}
  double best=1e30;int part=-1;const auto& mesh=s.result->lods[s.lod];
  for(auto& face:mesh.faces) {
    auto a=mesh.vertices[face.i[0]].p,b=mesh.vertices[face.i[1]].p,d=mesh.vertices[face.i[2]].p;
    auto e=plants::sub(b,a),f=plants::sub(d,a),p=plants::cross(ray,f);double det=plants::dot(e,p);if(std::abs(det)<1e-9)continue;
    auto t=plants::sub(origin,a);double u=plants::dot(t,p)/det;if(u<0||u>1)continue;auto q=plants::cross(t,e);double v=plants::dot(ray,q)/det;if(v<0||u+v>1)continue;
    double distance=plants::dot(f,q)/det;if(distance>0&&distance<best){best=distance;part=int(face.part);}
  }
  if(part>=0)for(size_t i=0;i<s.doc.nodes.size();i++)if(int(s.doc.nodes[i].kind)==part){s.selected=i;break;}
}
static void plantViewport() {
  auto& s=plantState;auto& io=ImGui::GetIO();
  if(s.owner&&(bbActive||modelViewerOpen)) {s.owner=false;cvars->SetFloat("ftebroom_plant_active",0);}
  if(!s.owner){ImGui::TextWrapped("Model Editor is using the shared 3D preview.");if(plantButton("Resume plant preview"))plantResumePreview();return;}
  ImGui::SetNextItemWidth(120);if(ImGui::Combo("##plant-lod",&s.lod,"LOD0 high\0LOD1 medium\0LOD2 low\0"))s.dirtyRender=true;plantRect("lod");ImGui::SameLine();
  ImGui::SetNextItemWidth(140);if(ImGui::Combo("##plant-display",&s.display,"Textured\0Solid\0Wireframe\0Normals\0UV debug\0"))s.dirtyRender=true;plantRect("display");ImGui::SameLine();
  if(plantButton("Frame","frame")){s.framePending=true;if(s.result){s.cameraLow=s.result->lods[s.lod].low;s.cameraHigh=s.result->lods[s.lod].high;s.zoom=1;s.framePending=false;}}
  ImGui::Checkbox("Ortho",&s.ortho);ImGui::SameLine();ImGui::Checkbox("Grid",&s.floor);ImGui::SameLine();ImGui::Checkbox("Axes",&s.axes);
  auto size=ImGui::GetContentRegionAvail();size.x=std::max(64.f,size.x);size.y=std::max(64.f,size.y-45*uiScale());
  auto pos=ImGui::GetCursorScreenPos();s.viewportMin=pos;s.viewportSize=size;
  if(!modelViewerImage)modelViewerImage=draw->LoadImageShader("ftebroom/modelviewer-image","{ program default2d { map $rt:ftebroom_modelviewer rgbgen vertex alphagen vertex } }");
  auto dl=ImGui::GetWindowDrawList();dl->AddImage((ImTextureID)modelViewerImage,pos,{pos.x+size.x,pos.y+size.y});
  ImGui::InvisibleButton("plant-viewport",size,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_MouseButtonMiddle);plantRect("viewport");
  bool hovered=ImGui::IsItemHovered(),active=ImGui::IsItemActive();bool handleHovered=false;
  if(s.handles&&s.advanced)for(int i=1;i<4;i++) {
    auto point=plants::mul(s.doc.p.controls[i],s.doc.p.height);ImVec2 screen;if(!plantProjectPoint(point,screen))continue;
    dl->AddCircleFilled(screen,6*uiScale(),IM_COL32(245,181,58,255));
    if(std::hypot(io.MousePos.x-screen.x,io.MousePos.y-screen.y)<12*uiScale())handleHovered=true;
    static int drag=-1;if(handleHovered&&ImGui::IsMouseClicked(0))drag=i;
    if(drag==i&&ImGui::IsMouseDown(0)){auto c=plantCamera();double scale=c.distance/std::max(1.,c.focal)/s.doc.p.height;auto delta=plants::add(plants::mul(c.right,io.MouseDelta.x*scale),plants::mul(c.up,-io.MouseDelta.y*scale));s.doc.p.controls[i]=plants::add(s.doc.p.controls[i],delta);for(double& x:s.doc.p.controls[i])x=std::clamp(x,-1.5,1.5);handleHovered=true;}
    if(!ImGui::IsMouseDown(0))drag=-1;
  }
  if(active&&!handleHovered) {
    if(ImGui::IsMouseDragging(0,2)){s.yaw-=io.MouseDelta.x*.4;s.pitch=std::clamp(s.pitch-io.MouseDelta.y*.4,-85.,85.);}
    if(ImGui::IsMouseDragging(1,0)||ImGui::IsMouseDragging(2,0)){auto c=plantCamera();double scale=c.distance/std::max(1.,c.focal);auto delta=plants::add(plants::mul(c.right,-io.MouseDelta.x*scale),plants::mul(c.up,io.MouseDelta.y*scale));s.cameraLow=plants::add(s.cameraLow,delta);s.cameraHigh=plants::add(s.cameraHigh,delta);}
  }
  if(hovered&&ImGui::IsMouseReleased(0)&&ImGui::GetMouseDragDelta(0).x*ImGui::GetMouseDragDelta(0).x+ImGui::GetMouseDragDelta(0).y*ImGui::GetMouseDragDelta(0).y<4&&!handleHovered)plantPick();
  if(hovered&&io.MouseWheel){ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);s.zoom=std::clamp(s.zoom*std::pow(1.12,-io.MouseWheel),.05,20.);}
  if(s.result) {
    auto& mesh=s.result->lods[s.lod];ImGui::Text("%zu triangles  |  %zu vertices  |  seed %u",mesh.faces.size(),mesh.vertices.size(),s.doc.seed);
    if(mesh.budgetLimited)ImGui::TextColored({1,.72f,.3f,1},"Budget reached; reduce density or raise the budget.");
    double now=ImGui::GetTime();if(s.dirtyRender||(s.wind&&now-s.lastUpload>1./30)) {
      auto api=bbAPI();auto images=bbImageAPI();if(!api||!images){s.error="This engine lacks the existing dynamic-mesh preview interfaces";return;}
      if(s.dirtyRender) {
        auto& maps=*s.result->maps;
        images->upload("bbtex/plant-atlas.tga",maps.albedo.rgba.data(),maps.albedo.width,maps.albedo.height);
        images->upload("bbtex/plant-normal.tga",maps.normal.rgba.data(),maps.normal.width,maps.normal.height);
        images->upload("bbtex/plant-orm.tga",maps.orm.rgba.data(),maps.orm.width,maps.orm.height);
        std::vector<unsigned char> checker(64*64*4,255);for(int y=0;y<64;y++)for(int x=0;x<64;x++)for(int a=0;a<3;a++)checker[(y*64+x)*4+a]=((x/4+y/4)%2)?80:210;
        images->upload("bbtex/plant-uv.tga",checker.data(),64,64);
      }
      auto buffer=plants::preview(mesh,s.doc,s.display==3?2:s.display==4?3:s.display==1||s.display==2?1:0,now,s.wind,s.sun);
      if(!api->upload(buffer.data(),buffer.size()))s.error="Engine rejected plant preview mesh";s.dirtyRender=false;s.lastUpload=now;
    }
  }else ImGui::TextUnformatted("Generating the first mesh...");
  auto vectorString=[](plants::V v){std::ostringstream out;out.imbue(std::locale::classic());out<<v[0]<<' '<<v[1]<<' '<<v[2];return out.str();};
  cvars->SetFloat("ftebroom_plant_active",1);cvars->SetFloat("ftebroom_bb_active",1);cvars->SetFloat("ftebroom_modelviewer_open",1);
  cvars->SetFloat("ftebroom_modelviewer_width",size.x);cvars->SetFloat("ftebroom_modelviewer_height",size.y);
  cvars->SetFloat("ftebroom_modelviewer_yaw",s.yaw);cvars->SetFloat("ftebroom_modelviewer_pitch",s.pitch);cvars->SetFloat("ftebroom_modelviewer_zoom",s.zoom);
  cvars->SetString("ftebroom_bb_low",vectorString(s.cameraLow).c_str());cvars->SetString("ftebroom_bb_high",vectorString(s.cameraHigh).c_str());
  cvars->SetString("ftebroom_plant_background",vectorString(s.background).c_str());
  cvars->SetFloat("ftebroom_bb_ortho",s.ortho);cvars->SetFloat("ftebroom_bb_cull",0);cvars->SetFloat("ftebroom_bb_display",s.display==2?2:0);
  cvars->SetFloat("ftebroom_modeleditor_floor",s.floor);cvars->SetFloat("ftebroom_modeleditor_axes",s.axes);
  cvars->SetFloat("ftebroom_modeleditor_player",0);cvars->SetFloat("ftebroom_modeleditor_freecamera",0);
  cvars->SetFloat("ftebroom_modeleditor_active",1);
}
static void plantAssets() {
  auto& s=plantState;
  if(ImGui::BeginTabBar("Plant assets")) {
    if(ImGui::BeginTabItem("Variations")) {
      ImGui::SetNextItemWidth(120);ImGui::InputScalar("Seed",ImGuiDataType_U32,&s.doc.seed);plantRect("Seed");ImGui::SameLine();
      if(plantButton("Randomize","randomize"))s.doc.seed=uint32_t(plants::mix(s.doc.seed,s.revision+uint64_t(core->GetMilliseconds())));
      ImGui::SameLine();if(plantButton("Keep variation","keep-variation")&&s.doc.variations.size()<64)s.doc.variations.push_back(s.doc.seed);
      ImGui::BeginChild("Plant saved variations",{0,80*uiScale()},ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
      for(size_t i=0;i<s.doc.variations.size();i++){ImGui::PushID(int(i));if(i)ImGui::SameLine();auto seed=s.doc.variations[i];
        ImGui::BeginGroup();auto variant=s.doc;variant.seed=seed;variant.preset="variant-"+plants::assetId(variant,0);
        int image=ImGui::IsRectVisible({70,70})?plantThumbnail(variant):0;
        bool choose=image?ImGui::ImageButton("variant",(ImTextureID)image,{64,48}):ImGui::Button("...##variant",{72,56});
        if(choose)s.doc.seed=seed;
        if(ImGui::BeginPopupContextItem("variation")){if(ImGui::MenuItem("Remove"))s.doc.variations.erase(s.doc.variations.begin()+i);ImGui::EndPopup();}
        ImGui::Text("%u",seed);ImGui::EndGroup();ImGui::PopID();}
      ImGui::EndChild();ImGui::EndTabItem();
    }
    if(ImGui::BeginTabItem("Textures / UV")) {
      if(s.result) {
        for(int i=0;i<3;i++){if(i)ImGui::SameLine();auto name=i==0?"bbtex/plant-atlas.tga":i==1?"bbtex/plant-normal.tga":"bbtex/plant-orm.tga";
          auto handle=draw->LoadImage(name);if(handle)ImGui::Image((ImTextureID)handle,{85*uiScale(),85*uiScale()});}
        ImGui::SameLine();ImGui::BeginGroup();ImGui::TextUnformatted("Albedo / normal / AO-roughness-metallic");ImGui::Checkbox("UV inspector",&s.uv);ImGui::EndGroup();
      }ImGui::EndTabItem();
    }
    if(ImGui::BeginTabItem("Scene")) {
      if(plantSlider("Sun direction",s.sun,-180,180,"%.0f degrees"))s.dirtyRender=true;
      plantColor("Background",s.background);ImGui::TextDisabled("LMB orbit / click select  |  RMB or MMB pan  |  wheel zoom");ImGui::EndTabItem();
    }ImGui::EndTabBar();
  }
  if(s.uv&&s.result) {
    ImGui::SetNextWindowSize({480*uiScale(),520*uiScale()},ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Plant Editor — UV inspector",&s.uv)) {
      auto p=ImGui::GetCursorScreenPos();float size=std::min(ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y);
      auto image=draw->LoadImage("bbtex/plant-atlas.tga");if(image)ImGui::GetWindowDrawList()->AddImage((ImTextureID)image,p,{p.x+size,p.y+size});
      auto& mesh=s.result->lods[s.lod];size_t shown=0;
      for(auto& face:mesh.faces)if(s.selected<0||s.selected>=int(s.doc.nodes.size())||face.part==s.doc.nodes[s.selected].kind) {
        for(int i=0;i<3;i++){auto a=mesh.vertices[face.i[i]].uv,b=mesh.vertices[face.i[(i+1)%3]].uv;ImGui::GetWindowDrawList()->AddLine({p.x+float(a[0])*size,p.y+float(a[1])*size},{p.x+float(b[0])*size,p.y+float(b[1])*size},IM_COL32(245,194,65,160));}if(++shown>4000)break;
      }ImGui::Dummy({size,size});
    }ImGui::End();
  }
}
static void plantConfirmSwitch() {
  auto& s=plantState;if(s.closeRequest){ImGui::OpenPopup("Save plant changes?");s.closeRequest=false;}
  if(ImGui::BeginPopupModal("Save plant changes?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("Save the editable plant before continuing?");
    if(plantButton("Save","confirm-save")){auto next=std::move(s.nextAction);s.nextAction={};ImGui::CloseCurrentPopup();plantAction([&](){plantAskSave(std::move(next));});}
    ImGui::SameLine();if(plantButton("Discard","confirm-discard")){auto next=std::move(s.nextAction);s.nextAction={};
      s.doc=s.saved.empty()?plants::presets().front():plants::deserialize(plants::Json::parse(s.saved));s.undo.clear();s.redo.clear();
      ImGui::CloseCurrentPopup();if(next)next();}
    ImGui::SameLine();if(plantButton("Keep editing","confirm-cancel")){s.nextAction={};ImGui::CloseCurrentPopup();}
    ImGui::EndPopup();
  }
}
static void plantWriteQa() {
  auto& s=plantState;if(!s.qa)return;static double last=0;if(ImGui::GetTime()-last<.08)return;last=ImGui::GetTime();
  plants::Json counts=plants::Json::array();if(s.result)for(auto& m:s.result->lods)counts.push_back({{"triangles",m.faces.size()},{"vertices",m.vertices.size()},{"low",m.low},{"high",m.high}});
  if(runUiBrowseOpen)for(auto it=runUiBrowseRects.begin();it!=runUiBrowseRects.end();++it)s.widgets[it.key()]=it.value();
  auto j=plants::Json{{"open",s.open},{"busy",plantBusy()},{"ready",plantReady()},{"widgets",s.widgets},{"project",plants::serialize(s.doc)},
    {"lods",counts},{"selected",s.selected},{"error",s.error},{"message",s.message},{"path",s.file.u8string()},{"model",s.lastModel.u8string()},
    {"camera",{s.yaw,s.pitch,s.zoom}},{"cameraLow",s.cameraLow},{"cameraHigh",s.cameraHigh},{"undo",s.undo.size()},{"redo",s.redo.size()},
    {"display",{ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y}},{"theme",uiTheme},{"profile",gameProfile},{"settingsDir",settingsPath().parent_path().u8string()},
    {"assetSource",std::string(baseq3)},{"assetRuntime",gameRuntime()}};
  plantAction([&](){bb::atomicWrite(documentPath("plant-ui-live.json"),j.dump());});
}
static void plantPanel() {
  auto& s=plantState;plantAction(plantTick);if(!s.open){plantWriteQa();return;}
  s.widgets=plants::Json::object();auto before=plants::serialize(s.doc);auto revision=s.revision;
  auto display=ImGui::GetIO().DisplaySize;float u=uiScale();
  ImGui::SetNextWindowSize({std::min(1640*u,display.x-28),std::min(980*u,display.y-54)},ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos({14,36},ImGuiCond_FirstUseEver);ImGui::SetNextWindowSizeConstraints({680*u,400*u},{FLT_MAX,FLT_MAX});
  bool open=true;
  const bool dirty=plants::serialize(s.doc).dump()!=s.saved;
  if(ImGui::Begin(dirty?"Plant Editor *###Plant Editor":"Plant Editor###Plant Editor",&open,ImGuiWindowFlags_NoDocking)) {
    if(s.qa){auto p=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();s.widgets["window"]={{"x",p.x},{"y",p.y},{"width",size.x},{"height",size.y}};}
    ImGui::PushItemWidth(-1);
    if(plantButton("New","new"))plantRequest([](){auto d=plants::presets().front();d.name="Untitled oak";d.seed=uint32_t(plants::mix(d.seed,plantState.revision+1));plantSelect(d);});ImGui::SameLine();
    if(plantButton("Open...","open"))plantRequest([](){runUiChoose(plantFolder().u8string(),[](const std::string& path){plantOpenFile(std::filesystem::u8path(path));});});ImGui::SameLine();
    if(plantButton("Save","save"))plantAction([](){plantAskSave();});ImGui::SameLine();
    ImGui::BeginDisabled(!plantReady());
    if(plantButton("Export...","export"))runUiChoose((plantFolder()/(plants::assetId(s.doc,s.lod)+".glb")).u8string(),[](const std::string& path){plantExport(std::filesystem::u8path(path));},true);ImGui::SameLine();
    if(plantButton("Add to Map","add-map"))plantAction(plantAddToMap);
    ImGui::EndDisabled();if(ImGui::GetWindowWidth()>=850*u)ImGui::SameLine();if(plantButton("Generate","generate"))plantSchedule(true);ImGui::SameLine();
    ImGui::BeginDisabled(s.undo.empty());if(plantButton("Undo","undo"))plantUndo();ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(s.redo.empty());if(plantButton("Redo","redo"))plantUndo(true);ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Checkbox("Advanced",&s.advanced))plantLibrarySave();plantRect("advanced");
    ImGui::Separator();ImGui::PopItemWidth();
    float bottom=155*u;
    ImGui::BeginChild("Plant workspace",{0,std::max(160.f,ImGui::GetContentRegionAvail().y-bottom)});
    bool narrow=ImGui::GetContentRegionAvail().x<850*u;
    if(!narrow&&ImGui::BeginTable("Plant panes",3,ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV)) {
      ImGui::TableSetupColumn("Library / structure",ImGuiTableColumnFlags_WidthFixed,245*u);ImGui::TableSetupColumn("Viewport",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("Properties",ImGuiTableColumnFlags_WidthFixed,310*u);
      ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::BeginChild("Plant library");plantLibrary();ImGui::EndChild();
      ImGui::TableSetColumnIndex(1);ImGui::BeginChild("Plant preview");plantViewport();ImGui::EndChild();
      ImGui::TableSetColumnIndex(2);ImGui::BeginChild("Plant properties");ImGui::PushItemWidth(-1);plantProperties();ImGui::PopItemWidth();ImGui::EndChild();ImGui::EndTable();
    }else if(narrow&&ImGui::BeginTabBar("Plant narrow panes")) {
      if(ImGui::BeginTabItem("Preview")){plantViewport();ImGui::EndTabItem();}
      if(ImGui::BeginTabItem("Library / Structure")){ImGui::BeginChild("Plant library");plantLibrary();ImGui::EndChild();ImGui::EndTabItem();}
      if(ImGui::BeginTabItem("Properties")){ImGui::BeginChild("Plant properties");plantProperties();ImGui::EndChild();ImGui::EndTabItem();}ImGui::EndTabBar();
    }ImGui::EndChild();plantAssets();
    if(plantBusy()) {static const char* stages[]={"Growing branches","Building LOD1","Building LOD2","Baking botanical textures"};int stage=s.work?s.work->stage.load():0;ImGui::ProgressBar((stage+.5f)/4,{-1,0},stages[std::clamp(stage,0,3)]);}
    else if(!s.error.empty())ImGui::TextColored({1,.38f,.28f,1},"%s",s.error.c_str());else ImGui::TextDisabled("%s",s.message.c_str());
    plantConfirmSwitch();
    if(plantFocused()&&!ImGui::GetIO().WantTextInput) {
      if(ImGui::Shortcut(ImGuiMod_Ctrl|ImGuiKey_Z))plantUndo(ImGui::GetIO().KeyShift);
      if(ImGui::Shortcut(ImGuiMod_Ctrl|ImGuiKey_Y))plantUndo(true);
      if(ImGui::Shortcut(ImGuiMod_Ctrl|ImGuiKey_S))plantAction([](){plantAskSave();});
    }
  }ImGui::End();
  if(!open)plantRequest([](){plantClose();});
  if(before!=plants::serialize(s.doc)&&revision==s.revision)plantChanged(before);
  if(!ImGui::GetActiveID())s.gesture=0;
  plantWriteQa();
}
