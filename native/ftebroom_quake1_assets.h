// SPDX-License-Identifier: GPL-2.0-or-later
#include "ftebroom_quake1.h"
#include "ftebroom_quake_palette.h"
#include "ftebroom_map_wads.h"
#include "ftebroom_wad_cache.h"
#include "ftebroom_compiler_wad.h"
#include "ftebroom_placeholder_texture.h"
static std::string q1Palette(){
  size_t size=0;auto data=(char*)files->LoadFile("gfx/palette.lmp",&size);
  if(!data)return ftebroom::defaultQuakePalette();
  std::string palette(data,size);core->Free(data);
  if(palette.size()!=768)throw std::runtime_error("Invalid Quake palette.lmp: expected 768 bytes.");
  return palette;
}
static std::map<std::string,std::string> q1MaterialWad,q1WadLabels;
static std::map<std::string,std::pair<int,int>> q1MaterialDimensions;
static std::vector<qhandle_t> q1ShaderHandles;
static std::string q1Read(const std::filesystem::path& path) {return ftebroom::pak::read(path);}
static std::vector<std::filesystem::path> q1ResolvedWads,q1MapWads;
static std::vector<std::filesystem::path> q1MissingMapWads, q1MissingWads;
static std::filesystem::path q1DocumentPak,q1DocumentBspWad;
static const std::vector<std::filesystem::path>& q1WadPaths(){return q1ResolvedWads;}
static void q1FindWads(bool strict=false){
  const auto jam = jamPreviewRuntime();
  q1MissingWads=jam.empty() ? q1MissingMapWads : std::vector<std::filesystem::path>{};
  q1ResolvedWads.clear();q1WadLabels.clear();std::vector<std::filesystem::path> sources;
  char builtin[4096]={};if(gameWad()&&files->NativePath(gameProfile==4?"halflife-editor.wad":"quake1-editor.wad",(fs_relative)5,builtin,sizeof builtin))sources.push_back(std::filesystem::u8path(builtin));
  std::vector<std::string> roots={q1Authoring(),gameRuntime()};if(roots[0]==roots[1])roots.pop_back();
  auto base=q1BaseDirectory();if(!base.empty())roots.insert(roots.begin(),base);
  if(gameMapModsKnown)for(const auto& path:gameMapResourcePaths)roots.push_back(path.u8string());
  if(!jam.empty())roots={jam.u8string()};
  std::set<std::filesystem::path> scannedRoots;
  for(const auto& root:roots){if(root.empty())continue;std::vector<std::filesystem::path> archives,loose;
    const auto directory=std::filesystem::u8path(root).lexically_normal();
    std::error_code error;
    if(!std::filesystem::is_directory(directory,error)||!scannedRoots.insert(directory).second)continue;
    for(std::filesystem::recursive_directory_iterator it(directory,std::filesystem::directory_options::skip_permission_denied),end;it!=end;++it){
      const auto& e=*it;auto leaf=e.path().filename().u8string();
      if(e.is_directory()&&(leaf=="_ftebroom-build"||leaf=="_ftebroom-wads"||leaf=="cache")){it.disable_recursion_pending();continue;}
      if(!e.is_regular_file())continue;auto ext=e.path().extension().u8string();std::transform(ext.begin(),ext.end(),ext.begin(),::tolower);
      if(ext==".pak")archives.push_back(e.path());else if(ext==".wad")loose.push_back(e.path());
    }
    std::sort(archives.begin(),archives.end());std::sort(loose.begin(),loose.end());
    for(const auto& archive:archives)for(const auto& entry:ftebroom::pak::directory(archive)){auto ext=std::filesystem::u8path(entry.name).extension().u8string();std::transform(ext.begin(),ext.end(),ext.begin(),::tolower);if(ext==".wad")sources.push_back(archive/std::filesystem::u8path(entry.name));}
    sources.insert(sources.end(),loose.begin(),loose.end());
  }
  if(jam.empty()&&!q1DocumentPak.empty())for(const auto& e:ftebroom::pak::directory(q1DocumentPak)){auto ext=std::filesystem::u8path(e.name).extension().u8string();std::transform(ext.begin(),ext.end(),ext.begin(),::tolower);if(ext==".wad")sources.push_back(q1DocumentPak/std::filesystem::u8path(e.name));}
  std::istringstream input(jam.empty()?(gameProfile==4?hlWads:q1Wads):"");std::string line;
  while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;auto path=std::filesystem::u8path(line);if(path.is_relative())path=std::filesystem::u8path(q1Authoring())/path;sources.push_back(path);}
  if(jam.empty())ftebroom::quake1::prioritizeMapWads(sources,q1MapWads);
  if(jam.empty()&&gameProfile==2&&!q1DocumentBspWad.empty())sources.insert(sources.begin(),q1DocumentBspWad);
  char writable[4096]={};if(!files->NativePath("ftebroom.cfg",(fs_relative)5,writable,sizeof writable))throw std::runtime_error("No writable editor cache.");auto cache=std::filesystem::u8path(writable).parent_path()/"cache/wads";std::filesystem::create_directories(cache);
  std::set<std::filesystem::path> seen, availableNames;
  for(const auto& source:sources){if(!seen.insert(source.lexically_normal()).second)continue;
    if(!ftebroom::pak::exists(source)){if(std::find(q1MissingWads.begin(),q1MissingWads.end(),source)==q1MissingWads.end())q1MissingWads.push_back(source);continue;}
    std::filesystem::path archive;
    std::string member, bytes;
    const bool packed=ftebroom::pak::split(source,archive,member);
    if(packed)bytes=q1Read(source);
    if(gameProfile==2){
      // A Quake mod folder can also contain GoldSrc decals or unrelated WADs.
      // Filter discovery before indexing materials or staging compiler input.
      char magic[4];
      const bool wad2=packed?bytes.compare(0,4,"WAD2")==0:
        std::ifstream(source,std::ios::binary).read(magic,sizeof magic) && std::string(magic,sizeof magic)=="WAD2";
      if(!wad2){
        core->Print(("ReyondBroom: skipped non-WAD2 archive for Quake 1: "+source.u8string()+"\n").c_str());
        continue;
      }
    }
    if(packed){
      ftebroom::quake1::wad(bytes);auto id=std::to_string(std::hash<std::string>{}(source.generic_u8string()));auto target=cache/(id+"-"+source.filename().u8string());
      bool unchanged=false;try{unchanged=std::filesystem::exists(target)&&q1Read(target)==bytes;}catch(...){}
      if(!unchanged){std::ofstream out(target,std::ios::binary);out.write(bytes.data(),bytes.size());out.close();if(!out)throw std::runtime_error("Cannot cache packed WAD.");}
      q1ResolvedWads.push_back(target);q1WadLabels[target.u8string()]=archive.filename().u8string()+"/"+member;
    }else {
      q1ResolvedWads.push_back(source);
      if(source==q1DocumentBspWad)q1WadLabels[source.u8string()]="BSP: "+source.filename().u8string();
    }
    availableNames.insert(source.filename());
  }
  // A user can reconnect a relocated archive without editing the original MAP.
  q1MissingWads.erase(std::remove_if(q1MissingWads.begin(),q1MissingWads.end(),[&](const auto& missing){
    return availableNames.count(missing.filename())!=0;
  }),q1MissingWads.end());
  for(const auto& path:q1MissingWads)core->Print(("ReyondBroom: missing WAD archive: "+path.u8string()+"\n").c_str());
  if(strict && !q1MissingWads.empty())throw std::runtime_error("Missing WAD archive: "+q1MissingWads.front().u8string()+". Reconnect the archive in Material / Add WAD(s) before compiling.");
}
static std::string q1WadList() {
  std::string list;for(auto& path:q1WadPaths()){auto name=path.generic_u8string();
    if(path==q1DocumentBspWad)continue;
    if(name.find_first_of(";\"\r\n")!=std::string::npos)throw std::runtime_error("WAD path contains a separator or quote.");
    if(!list.empty())list+=';';list+=name;}
  return list;
}
// Compiler input uses ASCII relative paths, independent of external Unicode names.
static std::string q1CompilerWads(const std::filesystem::path& folder,const std::string& source={},std::vector<std::string>* missing=nullptr,bool placeholders=false) {
  q1FindWads();std::filesystem::create_directories(folder);std::set<std::string> wanted;
  if(source.empty()){for(const auto& material:q1MaterialWad)wanted.insert(material.first);}
  else for(auto name:ftebroom::quake1::mapTextures(source))wanted.insert(name);
  std::set<std::string> owners;bool unresolved=false;
  for(const auto& name:wanted){auto found=q1MaterialWad.find(name);if(found==q1MaterialWad.end())unresolved=true;else owners.insert(found->second);}
  std::vector<std::string> archives;
  for(const auto& path:q1WadPaths()){auto label=q1WadLabels.count(path.u8string())?q1WadLabels[path.u8string()]:path.filename().u8string();if(unresolved||owners.count(label))archives.push_back(q1Read(path));}
  auto bytes=ftebroom::quake1::compilerWad(archives,wanted,gameProfile==4);auto target=folder/"textures.wad";
  std::set<std::string> present;for(const auto& t:ftebroom::quake1::wad(bytes))present.insert(t.name);
  if(missing)missing->clear();
  for(const auto& name:wanted)if(!present.count(name)){
    bool neutral=name=="__tb_empty";if(!neutral&&missing)missing->push_back(name);
    if(!neutral&&!placeholders){if(!missing)throw std::runtime_error("Missing Quake texture '"+name+"'. Add its WAD in Game / resources.");continue;}
    if(name.size()>15)throw std::runtime_error("WAD texture names cannot exceed 15 characters: "+name);
    auto palette=gameProfile==4?ftebroom::defaultQuakePalette():q1Palette();
    bytes=ftebroom::addWadTexture(std::move(bytes),ftebroom::placeholderTexture(name,palette,neutral),palette,false,gameProfile==4);
  }
  std::ofstream output(target,std::ios::binary);output.write(bytes.data(),bytes.size());output.close();
  if(!output)throw std::runtime_error("Cannot stage WAD for the compiler.");return "textures.wad";
}
static std::filesystem::path q1FindBspTextures(const std::string& source,const std::filesystem::path& map) {
  using namespace std::filesystem;
  if(gameProfile!=2)return {};
  const auto names=ftebroom::quake1::mapTextures(source);
  std::set<std::string> wanted(names.begin(),names.end());
  if(wanted.empty())return {};
  auto sibling=map;sibling.replace_extension(".bsp");
  const auto member=path("maps")/sibling.filename();
  std::vector<path> candidates{sibling};
  const auto addRoot=[&](const path& root){
    if(root.empty())return;
    candidates.push_back(root/member);
    for(const auto* pak:{"pak1.pak","pak0.pak"})candidates.push_back(root/pak/member);
  };
  for(auto it=gameMapResourcePaths.rbegin();it!=gameMapResourcePaths.rend();++it)addRoot(*it);
  addRoot(u8path(q1Authoring()));
  addRoot(u8path(q1BaseDirectory()));
  std::set<path> checked;
  for(const auto& bsp:candidates){
    if(!checked.insert(bsp.lexically_normal()).second||!ftebroom::pak::exists(bsp))continue;
    try{
      auto wad=ftebroom::quake1::embeddedTextureWad(q1Read(bsp),wanted);
      if(wad.empty())continue;
      char writable[4096]={};
      if(!files->NativePath("ftebroom.cfg",(fs_relative)5,writable,sizeof writable))return {};
      auto key=std::to_string(std::hash<std::string>{}(bsp.generic_u8string()))+"-"+
               std::to_string(std::hash<std::string>{}(wad));
      auto target=u8path(writable).parent_path()/"cache/wads"/u8path(key+"-"+bsp.filename().u8string()+".wad");
      if(!ftebroom::pak::exists(target)||q1Read(target)!=wad){
        create_directories(target.parent_path());
        auto temporary=target;temporary+=".tmp";
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        output.write(wad.data(),wad.size());output.close();
        if(!output)throw std::runtime_error("Cannot cache textures embedded in the BSP.");
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING))
          throw std::runtime_error("Cannot install textures embedded in the BSP.");
#else
        rename(temporary,target);
#endif
      }
      return target;
    }catch(const std::exception& error){
      core->Print(("ReyondBroom: "+bsp.u8string()+": "+error.what()+"\n").c_str());
    }
  }
  return {};
}
static bool q1DiscoverWads(const std::string& source,const std::filesystem::path& map) {
  auto previousBsp=std::exchange(q1DocumentBspWad,{});
  q1FindWads();
  auto resolved=ftebroom::quake1::mapWads(source,map,std::filesystem::u8path(q1Authoring()),ftebroom::pak::exists,q1WadPaths());
  q1DocumentBspWad=q1FindBspTextures(source,map);
  bool changed=resolved.missing!=q1MissingMapWads || resolved.found!=q1MapWads || previousBsp!=q1DocumentBspWad;
  q1MissingMapWads=std::move(resolved.missing);q1MapWads=std::move(resolved.found);
  return changed;

}
static void q1IndexAssets() {
  const auto started=std::chrono::steady_clock::now();
  q1FindWads();
  for(auto handle:q1ShaderHandles)draw->UnloadImage(handle);q1ShaderHandles.clear();q1MaterialWad.clear();q1MaterialDimensions.clear();
  auto palette=q1Palette();
  using TextureOwner=std::shared_ptr<const std::vector<ftebroom::quake1::Texture>>;
  struct IndexedTexture {ftebroom::quake1::Texture texture;std::weak_ptr<const std::vector<ftebroom::quake1::Texture>> owner;size_t index;};
  std::map<std::string,IndexedTexture> textures;
  static ftebroom::quake1::WadCache decoded;
  static ftebroom::quake1::WadImageCache images;images.prune();
  for(const auto& path:q1WadPaths()){
    auto archive=decoded.load(path,q1Read);
    for(size_t i=0;i<archive->size();i++){
      const auto& tex=(*archive)[i];
      q1MaterialWad[tex.name]=q1WadLabels.count(path.u8string())?q1WadLabels[path.u8string()]:path.filename().u8string();
      textures[tex.name]={tex,archive,i};
    }
  }
  // Retain only winning pixels, not entire source archives through texture
  // references. Weak source ownership preserves the decoded cache memory bound.
  if(!textures.count("__tb_empty")) {
    auto grey=textures.find("fb_grey");
    if(grey!=textures.end()){
      auto empty=grey->second;empty.texture.name="__tb_empty";
      textures["__tb_empty"]=std::move(empty);
    }
  }
  char writable[4096]={};if(!files->NativePath("ftebroom.cfg",(fs_relative)5,writable,sizeof writable))throw std::runtime_error("No writable editor cache.");
  auto editor=std::filesystem::u8path(writable).parent_path();
  auto cache=editor/"textures/_ftebroom_q1";std::filesystem::create_directories(cache);
  std::filesystem::create_directories(editor/"scripts");
  std::ofstream shaderFile(editor/"scripts/ftebroom-quake1.shader");
  for(const auto& pair:textures) {
    const auto& t=pair.second.texture;auto owner=pair.second.owner.lock();auto identity=ftebroom::quake1::textureCacheKey(t,palette),encoded=identity;
    q1MaterialDimensions[t.name]={int(t.width),int(t.height)};
    auto destination=cache/(encoded+".tga");
    bool unchanged=images.unchanged(destination,owner,pair.second.index,palette);
    if(!unchanged){
      auto bytes=ftebroom::quake1::tga(t,palette);unsigned collision=0;
      while(std::filesystem::exists(destination)){
        try{unchanged=q1Read(destination)==bytes;}catch(...){}
        if(unchanged)break;
        // Keep the byte comparison for new sources, corruption and collisions.
        encoded=identity+"-"+std::to_string(++collision);destination=cache/(encoded+".tga");
      }
      if(!unchanged){auto temporary=destination;temporary+=".tmp";std::ofstream out(temporary,std::ios::binary);out.write(bytes.data(),bytes.size());out.close();if(!out)throw std::runtime_error("Cannot write Quake texture cache.");std::filesystem::rename(temporary,destination);}
      if(owner)images.validated(destination,owner,pair.second.index,palette);
    }
    auto image="textures/_ftebroom_q1/"+encoded+".tga";
    int tags=t.name[0]=='*'?32:0;if(t.name.rfind("sky",0)==0)tags|=512;
    auto material="textures/"+t.name;
    // Global aliases also resolve on compiled BSP surfaces. Keep their lightmap
    // pass; editable MAP/browser previews register separate unlit shaders.
    auto script="{\n{\nmap "+image+"\n"+(t.name[0]=='{'?std::string("alphafunc GE128\ndepthwrite\n"):std::string{})+"}\n{\nmap $lightmap\nblendfunc filter\ndepthfunc equal\n}\n}";
    shaderFile<<material<<"\n"<<script<<"\n";
    materials[material]={material,image,false,tags};
  }
  shaderFile.close();command("fs_restart\nftebroom_reload_palette\nftebroom_reload_asset_models\n");
  cvars->SetString("ca_newbrushtexture","fb_grey");
  profileLoad("quake1_material_index",started);
}
