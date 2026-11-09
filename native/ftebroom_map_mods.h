// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace ftebroom {
// MAP mods are game resource paths, in override/load order. Parsing preserves
// TrenchBroom path tokens, including nested relative paths. The base
// game is inherited separately and is never inserted into the editable list.
inline bool mapModSame(const std::string& a,const std::string& b) {
#ifdef _WIN32
  if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(std::tolower(static_cast<unsigned char>(a[i]))!=std::tolower(static_cast<unsigned char>(b[i])))return false;return true;
#else
  return a==b;
#endif
}
inline void validateMapMod(const std::string& name) {
  if(name.empty()||name.find_first_of(";\r\n\t")!=name.npos)
    throw std::runtime_error("A mod resource path must be nonempty and cannot contain ';' or a control character.");
}
inline std::vector<std::string> mapModList(const std::string& text) {
  std::vector<std::string> result;size_t start=0;
  while(start<text.size()) {
    auto end=text.find(';',start);auto name=text.substr(start,end==text.npos?end:end-start);
    if(!name.empty())result.push_back(std::move(name));
    if(end==text.npos)break;start=end+1;
  }
  return result;
}
inline std::string mapModString(const std::vector<std::string>& mods) {
  std::string result;for(const auto& mod:mods){if(!result.empty())result+=';';result+=mod;}return result;
}
inline std::string mapModQuote(const std::string& value) {
  std::string result="\"";for(char c:value){if(c=='\\'||c=='\"')result+='\\';result+=c;}return result+'\"';
}
struct MapModToken {std::string value;size_t begin=0,end=0;bool quoted=false;};
inline bool mapModToken(const std::string& source,size_t& at,MapModToken& token) {
  for(;;){
    while(at<source.size()&&std::isspace(static_cast<unsigned char>(source[at])))++at;
    if(at==0&&source.compare(0,3,"\xef\xbb\xbf")==0){at=3;continue;}
    if(source.compare(at,2,"//")==0){at=source.find('\n',at);if(at==source.npos){at=source.size();return false;}continue;}
    if(source.compare(at,2,"/*")==0){at=source.find("*/",at+2);if(at==source.npos){at=source.size();return false;}at+=2;continue;}
    break;
  }
  if(at>=source.size())return false;
  token={};token.begin=at;
  if(source[at]=='\"'){
    token.quoted=true;++at;
    while(at<source.size()&&source[at]!='\"'){
      if(source[at]=='\\'&&at+1<source.size()&&(source[at+1]=='\\'||source[at+1]=='\"'))++at;
      token.value+=source[at++];
    }
    if(at<source.size())++at;
  }else if(source[at]=='{'||source[at]=='}')token.value=source[at++];
  else while(at<source.size()&&!std::isspace(static_cast<unsigned char>(source[at]))&&source[at]!='{'&&source[at]!='}')token.value+=source[at++];
  token.end=at;return true;
}
inline std::vector<std::string> mapModsFromSource(const std::string& source) {
  size_t at=0;MapModToken token,key;int depth=0;bool expectValue=false;std::vector<std::string> result;
  while(mapModToken(source,at,token)) {
    if(!token.quoted&&token.value=="{"){++depth;expectValue=false;continue;}
    if(!token.quoted&&token.value=="}"){if(--depth==0)break;expectValue=false;continue;}
    if(depth!=1)continue;
    if(!expectValue){key=token;expectValue=true;}
    else {if(key.value=="_tb_mod")result=mapModList(token.value);expectValue=false;}
  }
  return result;
}
// Used only while constructing a new scene. Existing maps keep their exact
// empty/nonempty list and are never silently assigned the global default mod.
inline std::string mapSourceWithMods(std::string source,const std::vector<std::string>& mods) {
  size_t at=0;MapModToken token;while(mapModToken(source,at,token))if(!token.quoted&&token.value=="{"){
    if(!mods.empty())source.insert(token.end,"\n\"_tb_mod\" "+mapModQuote(mapModString(mods))+"\n");return source;
  }
  throw std::runtime_error("New map has no worldspawn.");
}
inline std::vector<std::filesystem::path> mapModResourcePaths(const std::filesystem::path& root,const std::string& base,const std::vector<std::string>& mods) {
  validateMapMod(base);std::vector<std::filesystem::path> result{(root/std::filesystem::u8path(base)).lexically_normal()};
  for(const auto& mod:mods){validateMapMod(mod);result.push_back((root/std::filesystem::u8path(mod)).lexically_normal());}return result;
}
}
