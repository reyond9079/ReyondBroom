// SPDX-License-Identifier: GPL-2.0-or-later
#include "export.h"
#include <algorithm>
#include <stdexcept>
namespace ftebroom::profile {
namespace {
struct Token {size_t begin,end;std::string text;bool quoted;};
std::vector<Token> tokenize(const std::string& s){
 std::vector<Token> out;
 for(size_t p=0;p<s.size();){
  if(s[p]==' '||s[p]=='\t'||s[p]=='\r'||s[p]=='\n'){++p;continue;}
  if(s[p]=='/'&&p+1<s.size()&&s[p+1]=='/'){p=s.find('\n',p);if(p==s.npos)break;continue;}
  auto begin=p;std::string text;bool quoted=s[p]=='"';
  if(quoted){++p;bool closed=false;while(p<s.size()){char c=s[p++];if(c=='"'){closed=true;break;}if(c=='\\'&&p<s.size()&&(s[p]=='"'||s[p]=='\\'))c=s[p++];text+=c;}if(!closed)throw std::runtime_error("Unclosed MAP property string.");}
  else if(s[p]=='{'||s[p]=='}')text+=s[p++];
  else {while(p<s.size()&&s[p]!=' '&&s[p]!='\t'&&s[p]!='\r'&&s[p]!='\n'&&s[p]!='{'&&s[p]!='}')text+=s[p++];}
  out.push_back({begin,p,std::move(text),quoted});
 }return out;
}
std::string quote(const std::string& s){std::string out="\"";for(char c:s){if(c=='\0')throw std::runtime_error("NUL in exported entity property.");if(c=='"'||c=='\\')out+='\\';if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else out+=c;}return out+'"';}
}
std::string exportEntities(const std::string& original,bool stripTb,const std::string& pattern,const Json& added){
 auto map=original;
 if(!added.is_null()){
  if(!added.is_array())throw std::runtime_error("entityToAdd must be a list of properties.");map+="\n{\n";
  for(const auto& p:added)map+=quote(p.at("key").get<std::string>())+" "+quote(p.at("value").get<std::string>())+"\n";
  map+="}\n";
 }
 auto tokens=tokenize(map);std::vector<std::pair<size_t,size_t>> erase;std::vector<std::pair<size_t,size_t>> properties;int depth=0;size_t begin=0;std::string classname;
 for(size_t i=0;i<tokens.size();++i){auto& t=tokens[i];
  if(!t.quoted&&t.text=="{"){if(depth++==0){begin=t.begin;classname.clear();properties.clear();}}
  else if(!t.quoted&&t.text=="}"){
   if(--depth<0)throw std::runtime_error("Unbalanced MAP braces.");
   if(depth==0){if(!pattern.empty()&&!classname.empty()&&filenameMatches(classname,pattern))erase.push_back({begin,t.end});else erase.insert(erase.end(),properties.begin(),properties.end());}
  }else if(depth==1&&t.quoted&&i+1<tokens.size()&&tokens[i+1].quoted){
   const auto& v=tokens[++i];if(t.text=="classname")classname=v.text;
   if(stripTb&&t.text.rfind("_tb_",0)==0)properties.push_back({t.begin,v.end});
  }
 }
 if(depth)throw std::runtime_error("Unbalanced MAP braces.");
 std::sort(erase.rbegin(),erase.rend());for(const auto& span:erase)map.erase(span.first,span.second-span.first);return map;
}
}
