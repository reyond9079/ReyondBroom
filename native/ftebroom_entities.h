// SPDX-License-Identifier: GPL-2.0-or-later
#include "ftebroom_browser_navigation.h"
#include "ftebroom_entity_defs.h"
static std::string entityLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return char(std::tolower(c));
  });
  return value;
}
static ftebroom::EntityDefinitions entityDefinitions;
static int entityDefinitionEpoch = 0;
static bool entityPropertyFocused = false;
static std::map<int, std::map<std::string, std::string>> entityProperties;
static std::map<std::string, int> entityUsage;
static std::set<std::string> selectedEntityClasses;
static std::set<std::string> entityMapColors;
static std::string definitionSource, definitionOrigin, definitionError, definitionNotice, definitionMapPath;
static std::map<std::string, ImVec2> entityTestItems;
static void entityTestItem(const std::string& name)
{
  if (cvars->GetFloat("ftebroom_skip_setup"))
  {
    auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    entityTestItems[name] = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
  }
}
static bool selectEntityTab = false, definitionRequestPending = false;
static bool definitionsReadSource(const std::string& path, std::string& source)
{
  std::filesystem::path archive;std::string member;
  if(ftebroom::pak::split(std::filesystem::u8path(path),archive,member)){
    source=ftebroom::pak::read(std::filesystem::u8path(path));return true;
  }
  std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
  if (in)
  {
    source.assign(std::istreambuf_iterator<char>(in), {});
    return true;
  }
  size_t size = 0;
  auto data = (char*)files->LoadFile(path.c_str(), &size);
  if (!data)
    return false;
  source.assign(data, size);
  core->Free(data);
  return true;
}
static std::string definitionsExpand(
  const std::string& path, std::set<std::string>& stack)
{
  if (stack.size() > 32 || !stack.insert(path).second)
    throw std::runtime_error("Cyclic or excessively deep FGD include: " + path);
  std::string source;
  if (!definitionsReadSource(path, source))
    throw std::runtime_error("Cannot read entity definitions: " + path);
  if (source.size() > 16 * 1024 * 1024)
    throw std::runtime_error("Definition file exceeds 16 MiB");
  // Includes occupy their own directive line; leave comments and descriptions intact.
  std::istringstream lines(source);
  std::string line, result;
  while (std::getline(lines, line))
  {
    auto trimmed = ftebroom::entityTrim(line);
    if (trimmed.rfind("@include", 0) == 0)
    {
      auto words = ftebroom::entityTokens(trimmed);
      if (words.size() != 2)
        throw std::runtime_error("Malformed FGD include");
      auto included = (std::filesystem::u8path(path).parent_path()
                       / std::filesystem::u8path(words[1].text))
                        .lexically_normal()
                        .generic_u8string();
      result += definitionsExpand(included, stack) + "\n";
    }
    else
      result += line + "\n";
    if (result.size() > 16 * 1024 * 1024)
      throw std::runtime_error("Expanded definitions exceed 16 MiB");
  }
  stack.erase(path);
  return result;
}
#include "ftebroom_entity_preview.h"
#include "ftebroom_bb_io.h"
static bool definitionsLoad(std::string selection, bool mapOverride)
{
  const bool bundled=selection=="builtin:reyond";
  std::string path=std::move(selection), notice;
  std::string origin="Map override";
  if (path.rfind("external:", 0) == 0)
    path = path.substr(9);
  if (path.rfind("builtin:", 0) == 0)
    path.clear();
  try
  {
    // A shared MAP may refer to a definitions file on its author's computer.
    if (mapOverride && gameClassic() && !path.empty())
    {
      std::string probe;
      if (!definitionsReadSource(path, probe)){
        notice=std::string(uiTr("Map definition file unavailable:"))+" "+path;
        path.clear();
      }
    }
    if (path.empty())
    {
      if(!bundled)path=gameDefinitionPath(origin);
      std::string probe;
      if(path.empty()&&!bundled&&gameProfile==1&&definitionsReadSource("scripts/entities.def",probe)){
        origin="Detected in mod";path="scripts/entities.def";
      }
      if(path.empty()){
        gameProfileLoad();origin="Bundled with ReyondBroom";path=bundledDefinitionPath();
        const auto local=documentPath(path);
        if(std::filesystem::is_regular_file(local))path=local.generic_u8string();
      }
    }
    std::set<std::string> includes;
    auto source = definitionsExpand(path, includes);
    const auto extension=entityLower(std::filesystem::u8path(path).extension().u8string());
    const auto firstMarkup=source.find_first_of("{<");
    if(extension==".ent"&&firstMarkup!=source.npos&&source[firstMarkup]=='{')
      throw std::runtime_error(uiTr("This .ent file contains map entities. Choose an XML .ent definition file instead."));
    auto loaded=ftebroom::entityRead(source,extension);
    if(gameProfile==2) {
      std::string base;
      if(definitionsReadSource("quake1.ent",base)) {
        auto defaults=ftebroom::entityRead(base,".ent");
        for(auto& item:loaded)
          if(auto fallback=defaults.find(item.first);item.second.model.empty()&&fallback!=defaults.end())
            item.second.model=fallback->second.model;
        // Some mods describe only classes implemented by their game code.
        for(const char* name:{"func_group","func_detail"})
          if(auto it=defaults.find(name);it!=defaults.end())loaded.emplace(name,it->second);
      }
      for(const char* name:{"info_player_start","info_player_start2","info_player_coop","info_player_deathmatch"})
        if(auto it=loaded.find(name);it!=loaded.end()&&it->second.model.empty())it->second.model="progs/player.mdl";
      auto player=loaded.find("info_player_start");
      if(player!=loaded.end()) {
        size_t length=0;auto data=files->LoadFile(player->second.model.c_str(),&length);
        core->Print(("ReyondBroom: info_player_start model '"+player->second.model+"' in mod '"+q1Authoring()+"': "+(data&&length?"found":"MISSING")+"\n").c_str());
        if(data)core->Free(data);
      }
    }
    std::ostringstream out;
    for (const auto& item : loaded)
    {
      const auto& d = item.second;
      auto vec = [](const auto& v) {
        std::ostringstream s;
        s << v[0] << ' ' << v[1] << ' ' << v[2];
        return s.str();
      };
      // Expressions remain in the inspector; only literal model paths reach the renderer.
      std::string model = d.model;
      if (model.find_first_of("{}()\"'") != std::string::npos)
        model.clear();
      out << std::quoted(d.name) << ' ' << std::quoted(vec(d.mins)) << ' '
          << std::quoted(vec(d.maxs)) << ' ' << std::quoted(vec(d.color)) << ' '
          << std::quoted(model) << ' ' << std::quoted(d.modelKey) << ' '
          << std::quoted(d.modelSkin) << ' ' << std::quoted(d.modelFrame) << ' '
          << std::quoted(d.modelScale) << '\n';
      out << "kind " << std::quoted(d.name) << ' ' << (d.brush ? 1 : 0) << '\n';
      for (const auto& prop : d.properties)
        out << "property " << std::quoted(d.name) << ' ' << std::quoted(prop.first) << ' '
            << std::quoted(prop.second.value) << '\n';
      for(const auto& rule:d.modelRules)out<<"modelrule "<<std::quoted(d.name)<<' '<<std::quoted(rule.condition)<<' '<<std::quoted(rule.path)<<' '<<std::quoted(rule.skin)<<' '<<std::quoted(rule.frame)<<' '<<std::quoted(rule.scale)<<'\n';
    }
    // Publish through FTE's filesystem so its lookup cache sees the new file immediately.
    auto cache = out.str();
    static std::string published, publishedScope;
    const auto scope=std::to_string(gameProfile)+":"+std::string(baseq3)+":"+gameRuntime();
    const bool changed=cache!=published || scope!=publishedScope;
    qhandle_t cacheFile;
    if(files->Open("data/ftebroom-entity-definitions.txt",&cacheFile,2)<0)
      throw std::runtime_error("Cannot write entity definition cache");
    int written=files->Write(cacheFile,cache.data(),static_cast<int>(cache.size()));files->Close(cacheFile);
    if(written!=static_cast<int>(cache.size()))throw std::runtime_error("Incomplete entity definition cache");
    clearEntityPreviews();
    entityDefinitions = std::move(loaded);
    ++entityDefinitionEpoch;
    definitionSource = path;
    definitionOrigin=origin;
    definitionNotice=notice;
    definitionError.clear();
    // A map can start its CSQC after the notification command has been sent.
    cvars->SetFloat("ftebroom_entity_assets_epoch",cvars->GetFloat("ftebroom_entity_assets_epoch")+1);
    if(changed) {
      published=cache;publishedScope=scope;
    }
    core->Print(("ReyondBroom: loaded " + std::to_string(entityDefinitions.size())
                 + " entity definitions from " + path + "\n")
                  .c_str());
    return true;
  }
  catch (const std::exception& e)
  {
    definitionError = e.what();
    core->Print(("ReyondBroom: " + definitionError + "\n").c_str());
  }
  return false;
}
static void definitionsLoadCommand()
{
  if (commands->IsInsecure())return;
  char argument[4096]{};
  commands->Argv(1,argument,sizeof argument);
  definitionsLoad(*argument?argument:definitionMapPath,!*argument&&!definitionMapPath.empty());
  definitionRequestPending = false;
}
static void definitionsSetMapSource(const std::string& value)
{
  definitionMapPath=value;
  command(("ftebroom_entity_definition_path "+organizationQuote(value)+"\n").c_str());
}
static bool definitionsChoose(const std::filesystem::path& path,std::string& error)
{
  const auto source="external:"+path.generic_u8string();
  if(!definitionsLoad(source,false)){error=definitionError;return false;}
  definitionsSetMapSource(source);
  return true;
}
static std::filesystem::path definitionsBrowseFolder()
{
  std::error_code error;
  const auto current=std::filesystem::u8path(definitionSource).parent_path();
  if(definitionOrigin!="Bundled with ReyondBroom"&&std::filesystem::is_directory(current,error))return current;
  const auto mod=std::filesystem::u8path(gameProfile==2?q1Authoring():gameRuntime());
  if(std::filesystem::is_directory(mod,error))return mod;
  return documentDefaultFolder();
}
static int entitySelectionEpoch = -1;
static bool entitySelectionLinked=false;
static void entitySelectionLoad()
{
  auto& epoch = entitySelectionEpoch;
  std::string header, serialized;
  int next=-1;
  if(qcQuery(0,0,header) && !header.empty()) {
    std::string version;int count=0,nodes=0,geometry=0,revision=0,linked=0;
    std::istringstream meta(header);
    if(!(meta>>version>>next>>count>>nodes>>geometry>>revision>>linked) || version!="RBUI1" || count<0 || count>1000000 || nodes<2 || nodes>4000000)return;
    entitySelectionLinked=linked!=0;
    if(next==epoch)return;
    static int resourceGeometry=-1,resourceRevision=-1,resourceMap=-1;
    static std::string resources;
    std::string part;
    if(resourceGeometry!=geometry || resourceRevision!=revision || resourceMap!=organizationMapEpoch) {
      std::string fresh;
      for(int cursor=0;cursor<nodes;cursor+=128) {
        if(!qcQuery(2,cursor,part))return;
        fresh+=part;
      }
      if(!qcQuery(3,0,part))return;
      fresh+=part;resources=std::move(fresh);
      resourceGeometry=geometry;resourceRevision=revision;resourceMap=organizationMapEpoch;
    }
    for(int cursor=0;cursor<count;cursor+=64) {
      if(!qcQuery(1,cursor,part))return;
      serialized+=part;
    }
    serialized+=resources;
  }else {
    // Compatibility with older CSQC packages. Updated packages use no file or
    // cvar transport for inspector state.
    next=(int)cvars->GetFloat("ftebroom_property_epoch");
    entitySelectionLinked=cvars->GetFloat("ftebroom_linked_open")!=0;
    if(next==epoch)return;
    std::ifstream legacy(documentPath("data/ftebroom-entity-selection.txt"));
    if(!legacy)return;
    serialized.assign(std::istreambuf_iterator<char>(legacy),{});
  }
  std::istringstream in(serialized);
  std::map<int, std::map<std::string, std::string>> properties;
  std::map<std::string, int> usage;
  std::set<std::string> colors;
  std::string line, path;
  while (std::getline(in, line))
  {
    std::istringstream row(line);
    char kind;
    int id;
    std::string key, value;
    if (!(row >> kind >> id))
      continue;
    if (kind == 'e')
      properties[id];
    if (kind == 'p' && row >> std::quoted(key) >> std::quoted(value))
      properties[id][key] = value;
    if (kind == 'c' && row >> std::quoted(key))
      colors.insert(key);
    if (kind == 'u' && row >> std::quoted(key))
      usage[key]++;
    if (kind == 'd')
      row >> std::quoted(path);
  }
  entityProperties = std::move(properties);
  selectedEntityClasses.clear();
  for (const auto& entity : entityProperties)
    if (entity.first > 0)
      if (auto cls = entity.second.find("classname"); cls != entity.second.end())
        selectedEntityClasses.insert(cls->second);
  entityUsage = std::move(usage);
  entityMapColors = std::move(colors);
  epoch = next;
  if (definitionMapPath != path || (entityDefinitions.empty() && definitionError.empty()))
  {
    definitionMapPath = path;
    if (!definitionRequestPending)
    {
      definitionRequestPending = true;
      command("ftebroom_definitions\n");
    }
  }
}
static void testQCBridge()
{
  if(!cvars->GetFloat("ftebroom_skip_setup") || commands->IsInsecure())return;
  char expected[32]={},classname[128]={};commands->Argv(1,expected,sizeof expected);commands->Argv(2,classname,sizeof classname);
  std::string header;bool passed=qcQuery(0,0,header) && header.rfind("RBUI1 ",0)==0;
  entitySelectionLoad();
  passed=passed && entityProperties.size()==std::strtoul(expected,nullptr,10);
  if(*classname)for(const auto& entity:entityProperties) {
    auto field=entity.second.find("classname");
    if(field==entity.second.end() || field->second!=classname)passed=false;
  }
  const int epoch=entitySelectionEpoch;entitySelectionLoad();passed=passed && epoch==entitySelectionEpoch;
  core->Print((std::string("RB_QC_BRIDGE88 ")+(passed?"PASS":"FAIL")+" entities="+std::to_string(entityProperties.size())+" header="+header+"\n").c_str());
}
static std::string entitySoundPath(std::string value)
{
  auto first=value.find_first_not_of(" \t\r\n");
  if(first==value.npos)return {};
  value=value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
  std::replace(value.begin(),value.end(),'\\','/');
  while(value.rfind("./",0)==0)value.erase(0,2);
  if(value.empty()||value.size()>240||value.front()=='/'||
      value.find_first_of(":\r\n\"")!=value.npos)return {};
  for(auto& part:std::filesystem::u8path(value))
    if(part==".."||part==".")return {};
  return value;
}
static std::string entitySoundResource(const std::string& value)
{
  const auto path=entitySoundPath(value);
  if(path.empty())return {};
  auto exists=[](const std::string& name){
    qhandle_t handle;
    const auto length=files->Open(name.c_str(),&handle,1);
    if(length<0)return false;
    files->Close(handle);
    return length>0;
  };
  const auto extension=std::filesystem::u8path(path).extension().u8string();
  for(const auto& prefix:{"sound/",""}){
    if(*prefix&&entityLower(path).rfind("sound/",0)==0)continue;
    const std::string name=prefix+path;
    if(!extension.empty()){
      if(exists(name))return name;
    }else for(const char* suffix:{".wav",".ogg",".opus",".flac",".mp3"}){
      if(exists(name+suffix))return name+suffix;
    }
  }
  return {};
}
static std::string entitySoundCandidate(const std::string& value)
{
  auto path=entitySoundPath(value);
  if(path.find_first_not_of("+-0123456789.eE \t")==path.npos)return {};
  return path;
}
static std::string entitySoundScope()
{
  std::ostringstream scope;
  auto add=[&](const std::filesystem::path& folder){
    auto path=folder.lexically_normal().generic_u8string();
#ifdef _WIN32
    path=entityLower(path);
#endif
    scope<<' '<<std::quoted(path);
  };
  scope<<gameProfile;
  add(std::filesystem::u8path(gameProfile==0?std::string(baseq3):gameRuntime()));
  if(gameProfile==2)for(const auto& folder:gameMapResourcePaths)add(folder);
  return scope.str();
}
using EntitySoundBindings=std::map<std::pair<std::string,std::string>,std::string>;
static std::filesystem::path entitySoundSettingsPath(){return settingsPath().parent_path()/"ftebroom-preferences-sound-previews.txt";}
static EntitySoundBindings entitySoundLoadBindings()
{
  EntitySoundBindings result;
  std::ifstream in(entitySoundSettingsPath());
  std::string scope,classname,path;
  while(result.size()<4096&&in>>std::quoted(scope)>>std::quoted(classname)>>std::quoted(path))
    result[{scope,classname}]=entitySoundPath(path);
  return result;
}
static void entitySoundSaveBindings(const EntitySoundBindings& bindings)
{
  std::ostringstream out;
  for(const auto& [key,path]:bindings)out<<std::quoted(key.first)<<' '<<std::quoted(key.second)<<' '<<std::quoted(path)<<'\n';
  ftebroom::bb::atomicWrite(entitySoundSettingsPath(),out.str());
}
static void entitySoundPreview()
{
  static char manual[1024]={};
  static std::string error;
  static std::vector<std::pair<int,std::string>> previousSelection;
  static std::string previousScope;
  static int previousMap=-1;
  std::vector<std::pair<int,std::string>> selection;
  for(const auto& [id,props]:entityProperties){
    auto it=props.find("classname");
    selection.emplace_back(id,it==props.end()?(id==0?"worldspawn":""):entityLower(it->second));
  }
  const auto scope=entitySoundScope();
  if(scope!=previousScope||selection!=previousSelection||previousMap!=organizationMapEpoch){
    manual[0]=0;error.clear();previousSelection=selection;previousScope=scope;previousMap=organizationMapEpoch;
  }
  if(!uiInspectorFold("Sound preview",0))return;
  static auto bindings=entitySoundLoadBindings();
  const std::string classname=selection.size()==1?selection.front().second:std::string();
  const auto bindingKey=std::make_pair(scope,classname);
  const char* source="Entity property";
  std::string candidate;
  bool music=false;
  const std::initializer_list<const char*> soundKeys={"noise","sound","soundfile","noise1"};
  const std::initializer_list<const char*> worldKeys={"ambient_sound","_ambient_sound","ambient","_ambient","ambience","music","sounds"};
  for(const auto& entity:entityProperties) {
    auto findSound=[&](std::initializer_list<const char*> keys){
      for(const char* key:keys){
        auto it=entity.second.find(key);
        if(it==entity.second.end())continue;
        auto path=entitySoundCandidate(it->second);
        if(path.empty())continue;
        candidate=std::move(path);
        music=std::string_view(key)=="music";
        return;
      }
    };
    if(entity.first==0)findSound(worldKeys);
    if(candidate.empty())findSound(soundKeys);
    if(!candidate.empty())break;
  }
  const auto saved=bindings.find(bindingKey);
  if(candidate.empty()&&saved!=bindings.end()){source="Saved preview for class";candidate=saved->second;}
  for(const auto& [id,name]:selection){
    if(!candidate.empty())break;
    const auto definition=entityDefinitions.find(name);
    if(definition!=entityDefinitions.end()){
      const auto& properties=definition->second.properties;
      auto findDefault=[&](std::initializer_list<const char*> keys){
        for(const char* key:keys){
          auto it=properties.find(key);
          if(it!=properties.end())candidate=entitySoundCandidate(it->second.value);
          if(!candidate.empty()){music=std::string_view(key)=="music";break;}
        }
      };
      if(id==0)findDefault(worldKeys);
      if(candidate.empty())findDefault(soundKeys);
      if(candidate.empty())for(const auto& [key,property]:properties){
        if(property.type=="sound")candidate=entitySoundCandidate(property.value);
        if(!candidate.empty())break;
      }
      if(!candidate.empty()){source="Definition default";break;}
    }
  }
  if(candidate.empty())for(const auto& [id,name]:selection){
      static const std::map<std::string,std::string> ambient={{"ambient_suck_wind","ambience/suck1.wav"},{"ambient_drone","ambience/drone6.wav"},{"ambient_flouro_buzz","ambience/buzz1.wav"},{"ambient_drip","ambience/drip1.wav"},{"ambient_comp_hum","ambience/comp1.wav"},{"ambient_thunder","ambience/thunder1.wav"},{"ambient_light_buzz","ambience/fl_hum1.wav"},{"ambient_swamp1","ambience/swamp1.wav"},{"ambient_swamp2","ambience/swamp2.wav"}};
      auto it=ambient.find(name);if(it!=ambient.end()){source="Built-in ambient sound";candidate=it->second;break;}
  }
  if(!candidate.empty())ImGui::TextWrapped("%s: %s",uiTr(source),candidate.c_str());
  if(ImGui::InputTextWithHint("Sound override",uiTr("Relative path, e.g. ambience/wind.wav"),manual,sizeof manual))error.clear();
  if(!classname.empty()){
    auto save=[&](bool clear){
      try{
        auto next=bindings;
        if(clear)next.erase(bindingKey);
        else {
          const auto path=entitySoundResource(manual);
          if(path.empty()){error="Sound file not found in mounted game resources.";return;}
          next[bindingKey]=path;
        }
        entitySoundSaveBindings(next);bindings=std::move(next);manual[0]=0;error.clear();
      }catch(const std::exception& e){error=e.what();}
    };
    const bool hasSaved=saved!=bindings.end();
    ImGui::BeginDisabled(!*manual);const bool store=ImGui::Button("Save preview for class");ImGui::EndDisabled();
    ImGui::SameLine();ImGui::BeginDisabled(!hasSaved);const bool clear=ImGui::Button("Clear class preview");ImGui::EndDisabled();
    if(store||clear)save(clear);
    ImGui::TextDisabled("%s",uiTr("Class preview paths are stored in the editor."));
  }
  static float volume=.7f;ImGui::SliderFloat("Preview volume",&volume,0,1);
  if(ImGui::Button("Play sound")) {
    const auto requested=*manual?std::string(manual):candidate;
    auto path=entitySoundResource(requested);
    if(path.empty()&&!*manual&&music){
      // Quake III music may specify an intro and a loop. Prefer an exact
      // match first, so a single path containing spaces also works.
      std::istringstream paths(requested);
      std::string intro,loop;
      if(paths>>std::quoted(intro)>>std::quoted(loop))path=entitySoundResource(intro);
    }
    if(requested.empty())error="No sound path is set.";
    else if(path.empty())error="Sound file not found in mounted game resources.";
    else if(cvars->GetFloat("nosound")!=0)error="Sound is disabled.";
    else {
      error.clear();
      // The FTE sound API uses this prefix to address the exact virtual
      // filesystem path without prepending another sound/ directory.
      draw->LocalSound(("../"+path).c_str(),257,volume);
    }
  }
  ImGui::SameLine();if(ImGui::Button("Stop sound"))command("stopsound\n");
  if(!error.empty())ImGui::TextWrapped("%s",uiTr(error.c_str()));
  ImGui::TextWrapped("Uses mounted mod resources. Custom progs.dat may choose sounds dynamically; enter that relative sound path above.");
}
static void entityProperty(
  const char* op, const std::string& key, const std::string& value = "")
{
  command(("ftebroom_entity_property " + std::string(op) + " " + organizationQuote(key)
           + " " + organizationQuote(value) + "\n")
            .c_str());
}
static void entityCreate(const ftebroom::EntityDefinition& d, const float* position)
{
  if(gameProfile==2 && d.brush && d.name=="func_group"){
    command("ftebroom_group \"Group\"\n");
    return;
  }
  cvars->SetFloat("ftebroom_instantiate_defaults",1);
  std::ostringstream point;
  point << position[0] << ' ' << position[1] << ' ' << position[2];
  command(("ftebroom_entity_create " + organizationQuote(d.name) + " "
           + (d.brush ? "1 " : "0 ") + organizationQuote(point.str()) + "\n")
            .c_str());
}
#include "ftebroom_property_table.h"
static bool entityAdvancedSearch=false;
static int entityThumbnailSize=80;
static ImVec2 entityFooterMin,entityFooterMax;
static int entityVisiblePreviewCount=0;
static bool entityPreviewMarginsValid=true;
static bool entityBrowserIcon(const char* id,bool filter){
  float h=ImGui::GetFrameHeight();auto p=ImGui::GetCursorScreenPos();
  bool clicked=false;if(filter)clicked=ImGui::Button(id,{h,h});else ImGui::Dummy({h,h});
  auto d=ImGui::GetWindowDrawList();
  auto c=ImGui::GetColorU32(ImGuiCol_Text);float u=h/24.f;
  if(filter){for(int row=0;row<3;row++){float y=p.y+(7+row*5)*u,x=p.x+(row==1?15:9)*u;
    d->AddLine({p.x+5*u,y},{p.x+19*u,y},c,u);d->AddCircleFilled({x,y},2*u,c,12);}}
  else {d->AddCircle({p.x+10*u,p.y+10*u},5*u,c,16,u);d->AddLine({p.x+14*u,p.y+14*u},{p.x+19*u,p.y+19*u},c,1.5f*u);}
  return clicked;
}
static void entityDefinitionControls(){
  const bool bundled=definitionMapPath=="builtin:reyond";
  const bool automatic=definitionMapPath.empty()||(!bundled&&definitionMapPath.rfind("builtin:",0)==0);
  const char* modes[]={"Automatic (profile or mod)","Bundled definitions","Map override"};
  if(ImGui::BeginCombo("Definition source",uiTr(modes[automatic?0:bundled?1:2]))){
    if(ImGui::Selectable(modes[0],automatic)&&!automatic&&definitionsLoad({},false))definitionsSetMapSource({});
    if(ImGui::Selectable(modes[1],bundled)&&!bundled&&definitionsLoad("builtin:reyond",false))definitionsSetMapSource("builtin:reyond");
    if(ImGui::Selectable(modes[2],!automatic&&!bundled))definitionsRequest=true;
    ImGui::EndCombo();
  }
  if(!definitionSource.empty()){
    const auto extension=entityLower(std::filesystem::u8path(definitionSource).extension().u8string());
    ImGui::TextWrapped("%s (%s)",uiTr(definitionOrigin.c_str()),extension.c_str());
    ImGui::TextWrapped("%s",definitionSource.c_str());
  }
  if(ImGui::Button("Browse definitions..."))definitionsRequest=true;
  ImGui::SameLine();if(ImGui::Button("Reload"))command("ftebroom_definitions\n");
  if(!definitionNotice.empty())ImGui::TextWrapped("%s",definitionNotice.c_str());
  if(!definitionError.empty())ImGui::TextWrapped("%s",definitionError.c_str());
}
static void entityPanel()
{
  entityPropertyFocused = false;
  entitySelectionLoad();
  entityPropertyFocused=false;
  cvars->SetString("ftebroom_entity_ghost", "");
  if (selectEntityTab)
    ImGui::SetNextWindowFocus();
  if (uiSoftPanelBegin("Entity",nullptr,uiPanelFlags()))
  {
    static bool hadSelection=false;
    static int lastSelectionEpoch=-1,lastMapEpoch=-1;
    static std::string chosen, classPreview, pendingPreview;
    static double previewAfter=0;
    static bool openProperties=false;
    const bool hasSelection=cvars->GetFloat("ftebroom_selection_count")>0 ||
      cvars->GetFloat("ftebroom_entity_selection_count")>0;
    if(hasSelection!=hadSelection || organizationMapEpoch!=lastMapEpoch ||
       (hasSelection&&entitySelectionEpoch!=lastSelectionEpoch))
    {
      classPreview.clear();
      pendingPreview.clear();
      if(organizationMapEpoch!=lastMapEpoch)chosen.clear();
      ImGui::SetNextItemOpen(hasSelection,ImGuiCond_Always);
    }
    hadSelection=hasSelection;
    lastSelectionEpoch=entitySelectionEpoch;
    lastMapEpoch=organizationMapEpoch;
    const bool previewChanged=!pendingPreview.empty()&&ImGui::GetTime()>=previewAfter;
    if(previewChanged){
      classPreview=std::exchange(pendingPreview,{});
      openProperties=true;
    }
    if(std::exchange(openProperties,false))
      ImGui::SetNextItemOpen(true,ImGuiCond_Always);
    const auto preview=entityDefinitions.find(classPreview);
    if(!classPreview.empty()&&preview==entityDefinitions.end()){
      classPreview.clear();
      ImGui::SetNextItemOpen(hasSelection,ImGuiCond_Always);
    }
    if(cvars->GetFloat("ftebroom_skip_setup")&&cvars->GetFloat("ftebroom_test_model_previews"))ImGui::SetNextItemOpen(false);
    if (uiInspectorFold("Properties",0))
    {
      const float maxHeight=std::max(32.f,ImGui::GetContentRegionAvail().y-148*uiScale());
      ImGui::SetNextWindowSizeConstraints({0,0},{FLT_MAX,maxHeight});
      if(previewChanged)ImGui::SetNextWindowScroll({-1,0});
      ImGui::BeginChild("Entity properties",{0,0},ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize);
      if(preview!=entityDefinitions.end())entityClassDefaults(preview->second);
      else{
        ImGui::Text(
          "Editing %u entit%s",
          (unsigned)entityProperties.size(),
          entityProperties.size() == 1 ? "y" : "ies");
        entityPropertyTable();
        entitySoundPreview();
      }
      ImGui::EndChild();
    }
    ImGui::Separator();
    if (uiInspectorFold("Entity browser",ImGuiTreeNodeFlags_DefaultOpen))
    {
      static char filter[256]{};
      if(cvars->GetFloat("ftebroom_skip_setup")){char testFilter[256]={};cvars->GetString("ftebroom_test_entity_filter",testFilter,sizeof testFilter);if(*testFilter)std::snprintf(filter,sizeof filter,"%s",testFilter);}
      static bool used = false, grouped = true;
      static int sort = 0;
      static float position[3] = {0, 0, 64};
      float searchWidth=ImGui::GetContentRegionAvail().x-ImGui::GetFrameHeight()-ImGui::GetStyle().ItemSpacing.x;
      ImGui::SetNextItemWidth(std::max(30.f,searchWidth));
      uiBrowserSearch("##classfilter","Filter classes",filter,sizeof filter);
      ImGui::SameLine();
      if(entityBrowserIcon("##entityAdvanced",true))entityAdvancedSearch=!entityAdvancedSearch;
      entityTestItem("advanced_search");
      if(ImGui::IsItemHovered())ImGui::SetTooltip("Advanced search");
      if(entityAdvancedSearch){
      const float filterWidth=ImGui::GetContentRegionAvail().x;
      ImGui::Checkbox("Used only", &used);
      const char* groupedLabel="Group by prefix";const char* groupedEnd=nullptr;
      FteBroomTranslate(groupedLabel,groupedEnd);
      const auto& style=ImGui::GetStyle();
      if(!uiModern()||ImGui::GetItemRectSize().x+style.ItemSpacing.x+ImGui::GetFrameHeight()
         +style.ItemInnerSpacing.x+ImGui::CalcTextSize(groupedLabel,groupedEnd).x<=filterWidth)
        ImGui::SameLine();
      ImGui::Checkbox("Group by prefix", &grouped);
      if(ImGui::BeginTable("Entity filters",2,ImGuiTableFlags_SizingStretchSame)){
        ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();ImGui::TextDisabled("Point entities");
        ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Sort");ImGui::SameLine();ImGui::SetNextItemWidth(-1);ImGui::Combo("##entitySort",&sort,"Name\0Usage\0");ImGui::EndTable();
      }
      }
      ImGui::SetNextItemWidth(std::max(60.f,ImGui::GetContentRegionAvail().x-ImGui::CalcTextSize("Create at").x-10*uiScale()));
      ImGui::InputFloat3("Create at", position);
      std::vector<const ftebroom::EntityDefinition*> classes;
      std::string query = entityLower(filter);
      for (const auto& item : entityDefinitions)
      {
        const auto& d = item.second;
        if (
          d.name == "worldspawn" || d.brush || (used && !entityUsage[d.name])
          || entityLower(d.name).find(query) == std::string::npos)
          continue;
        classes.push_back(&d);
      }
      if(browserQaEnabled){browserQaEntityClasses.clear();for(const auto* d:classes)browserQaEntityClasses.push_back(d->name);}
      std::stable_sort(classes.begin(), classes.end(), [&](auto a, auto b) {
        if (sort && entityUsage[a->name] != entityUsage[b->name])
          return entityUsage[a->name] > entityUsage[b->name];
        return a->name < b->name;
      });
      ImGui::BeginChild(
        "Classes",
        {0, std::max(24.f,ImGui::GetContentRegionAvail().y-ImGui::GetFrameHeight()-ImGui::GetStyle().ItemSpacing.y*2)},
        ImGuiChildFlags_Borders,ImGuiWindowFlags_None);
      browserQaWindow("entities/grid");browserHoverZoom(entityThumbnailSize,48,160);
      entityVisiblePreviewCount=0;entityPreviewMarginsValid=true;
      std::string group;
      int column=0;
      float size=entityThumbnailSize*uiScale(),rowTop=ImGui::GetCursorPosY();
      float cellWidth=size+12*uiScale(),cellHeight=size+ImGui::GetTextLineHeightWithSpacing()+8*uiScale();
      int columns=std::max(1,int(ImGui::GetContentRegionAvail().x/cellWidth));
      for (const auto* def : classes)
      {
        auto prefix=def->name.substr(0,def->name.find('_'));
        if(grouped && prefix!=group) {
          if(column){ImGui::SetCursorPosY(rowTop+cellHeight);column=0;}
          group=prefix;uiSectionHeading(group.c_str());rowTop=ImGui::GetCursorPosY();
        }
        ImGui::SetCursorPos({8+column*cellWidth,rowTop});ImGui::PushID(def->name.c_str());
        ImVec2 tile=ImGui::GetCursorScreenPos();
        bool selected=chosen==def->name || selectedEntityClasses.count(def->name)!=0;
        if(ImGui::InvisibleButton("tile",{size,cellHeight-4},
             ImGuiButtonFlags_PressedOnClickRelease|ImGuiButtonFlags_PressedOnDoubleClick)) {
          chosen=def->name;
          bool hasEntity=false,compatible=true;
          for(const auto& e:entityProperties)if(e.first>0) {
            hasEntity=true;auto model=e.second.find("model");
            bool brush=model!=e.second.end() && !model->second.empty() && model->second[0]=='*';
            auto groupType=e.second.find("_tb_type");
            if(brush!=def->brush || groupType!=e.second.end())compatible=false;
          }
          if(hasEntity && compatible){
            pendingPreview.clear();
            classPreview.clear();
            openProperties=true;
            if(selectedEntityClasses.size()!=1 || !selectedEntityClasses.count(def->name))
              command(("ftebroom_entity_property set classname "+organizationQuote(def->name)+"\n").c_str());
          }else if(!hasEntity&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
            pendingPreview.clear();
            entityCreate(*def,position);
          }else{
            // Keep the tile in place until a possible double-click has finished.
            pendingPreview=def->name;
            previewAfter=ImGui::GetTime()+(hasEntity?0:ImGui::GetIO().MouseDoubleClickTime);
            if(hasEntity)cvars->SetString("ftebroom_selection_status","Choose a compatible point/brush entity class; groups cannot be reclassified.");
          }
        }
        entityTestItem("class:"+def->name);
        if(!def->brush && ImGui::BeginDragDropSource()) {
          ImGui::SetDragDropPayload("FTE_ENTITY",def->name.c_str(),def->name.size()+1);
          ImGui::TextUnformatted(def->name.c_str());cvars->SetString("ftebroom_entity_ghost",def->name.c_str());ImGui::EndDragDropSource();
        }
        auto drawList=ImGui::GetWindowDrawList();
        // Clip every preview path, including the fallback bounds and thick lines.
        drawList->PushClipRect(tile,{tile.x+size,tile.y+cellHeight-4},true);
        const ImVec2 tileEnd{tile.x+size,tile.y+cellHeight-4};
        if(!uiModern()||selected||ImGui::IsItemHovered())
          drawList->AddRectFilled(tile,tileEnd,uiModern()?ImGui::GetColorU32(
            selected?uiModernPalette().active:uiModernPalette().hover):IM_COL32(55,58,62,255),(uiModern()?8:6)*uiScale());
        ImVec2 preview{tile.x+size*.15f,tile.y+size*.15f};
        if(ImGui::IsRectVisible(tile,{tile.x+size,tile.y+size}))++entityVisiblePreviewCount;
        if(!entityModelTile(*def,preview,size*.70f)) {
        ImVec2 points[8];float extent=1;
        for(int axis=0;axis<3;axis++)extent=std::max(extent,def->maxs[axis]-def->mins[axis]);
        for(int corner=0;corner<8;corner++) {
          float v[3];for(int axis=0;axis<3;axis++)v[axis]=((corner&(1<<axis))?0.5f:-0.5f)*(def->maxs[axis]-def->mins[axis])/extent;
          points[corner]={tile.x+size*(.5f+.30f*(v[0]-v[1])),tile.y+size*(.5f+.13f*(v[0]+v[1])-.34f*v[2])};
          entityPreviewMarginsValid &= points[corner].x>=tile.x+size*.19f&&points[corner].x<=tile.x+size*.81f&&points[corner].y>=tile.y+size*.19f&&points[corner].y<=tile.y+size*.81f;
        }
        auto color=ImGui::ColorConvertFloat4ToU32({def->color[0],def->color[1],def->color[2],1});
        for(int corner=0;corner<8;corner++)for(int axis=0;axis<3;axis++)if(!(corner&(1<<axis)))
          drawList->AddLine(points[corner],points[corner|(1<<axis)],color,uiScale());
        }
        if(selected)drawList->AddRect(tile,tileEnd,uiModern()?ImGui::GetColorU32(uiModernPalette().tileOutline):
          IM_COL32(225,95,55,255),uiModern()?8*uiScale():6*uiScale(),0,1.5f*uiScale());
        drawList->PopClipRect();
        drawList->PushClipRect({tile.x,tile.y+size+2},{tile.x+size,tile.y+cellHeight-2},true);
        ImVec2 label{tile.x+std::max(0.f,(size-ImGui::CalcTextSize(def->name.c_str()).x)*.5f),tile.y+size+2};
        ImGui::RenderTextEllipsis(drawList,label,{tile.x+size,tile.y+cellHeight-2},tile.x+size,tile.x+size,def->name.c_str(),nullptr,nullptr);drawList->PopClipRect();
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s\n%s",def->name.c_str(),def->brush?"Brush entity: select brushes first":"Point entity",def->description.c_str());
        ImGui::PopID();
        if(++column==columns){column=0;rowTop+=cellHeight;}
      }
      ImGui::SetCursorPosY(rowTop+(column?cellHeight:0));ImGui::Dummy({0,0});
      ImGui::EndChild();
      entityFooterMin=ImGui::GetCursorScreenPos();
      const float footerWidth=ImGui::GetContentRegionAvail().x;
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(5*uiScale(),3*uiScale()));
      float height=ImGui::GetFrameHeight(),sliderWidth=std::min(82*uiScale(),footerWidth*.23f);
      const char* definitions="Entity definitions";
      const char* translated=definitions;const char* end=nullptr;FteBroomTranslate(translated,end);
      float definitionWidth=ImGui::CalcTextSize(translated,end).x+ImGui::GetStyle().FramePadding.x*2;
      bool compact=definitionWidth+height+sliderWidth+ImGui::GetStyle().ItemSpacing.x*2>footerWidth;
      if(ImGui::Button(compact?"...##definitions":definitions))ImGui::OpenPopup("Entity definitions menu");
      entityTestItem("definitions_footer");
      if(compact&&ImGui::IsItemHovered())ImGui::SetTooltip("Entity definitions");
      if(ImGui::BeginPopup("Entity definitions menu")){entityDefinitionControls();ImGui::EndPopup();}
      ImGui::SameLine(std::max(ImGui::GetCursorPosX(),ImGui::GetWindowContentRegionMax().x-sliderWidth-height-ImGui::GetStyle().ItemSpacing.x));
      entityBrowserIcon("##entityThumbnailMagnify",false);
      if(ImGui::IsItemHovered())ImGui::SetTooltip("Thumbnail size");
      ImGui::SameLine();ImGui::SetNextItemWidth(sliderWidth);ImGui::SliderInt("##entityThumbnailSize",&entityThumbnailSize,48,160,"%d");
      entityTestItem("thumbnail_footer");if(ImGui::IsItemHovered())ImGui::SetTooltip("Thumbnail size");
      entityFooterMax=ImGui::GetItemRectMax();
      ImGui::PopStyleVar();
    }
  }
  ImGui::End();
  selectEntityTab = false;
}
