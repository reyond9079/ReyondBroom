#include "../native/ftebroom_pak.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main(){
  namespace fs=std::filesystem;auto p=fs::temp_directory_path()/("pak-names-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".pak");
  {std::ofstream out(p,std::ios::binary);out.write("PACK",4);ftebroom::pak::put(out,14);ftebroom::pak::put(out,128);out.write("ab",2);
    for(auto name:{"gfx/safe.lmp","models/tank/../ctank/skin.pcx"}){std::string row(56,0);row.replace(0,std::strlen(name),name);out.write(row.data(),56);ftebroom::pak::put(out,12);ftebroom::pak::put(out,1);}}
  bool strict=false;try{ftebroom::pak::directory(p);}catch(...){strict=true;}assert(strict);
  auto entries=ftebroom::pak::directory(p,true);assert(entries.size()==1&&entries[0].name=="gfx/safe.lmp");
  auto listed=ftebroom::pak::list(p);assert(listed.size()==1&&listed[0].first&&listed[0].second.filename()=="gfx");
  listed=ftebroom::pak::list(p/"gfx");assert(listed.size()==1&&!listed[0].first&&listed[0].second.filename()=="safe.lmp");
  assert(ftebroom::pak::read(p/"gfx/safe.lmp")=="a");
  bool writeBlocked=false;try{ftebroom::pak::replace(p/"new.cfg","test");}catch(...){writeBlocked=true;}assert(writeBlocked);
  assert(!ftebroom::pak::exists(p/"new.cfg")&&ftebroom::pak::read(p/"gfx/safe.lmp")=="a");
  bool unsafe=false;try{ftebroom::pak::read(p/"models/tank/../ctank/skin.pcx");}catch(...){unsafe=true;}assert(unsafe);
  fs::remove(p);std::cout<<"PASS: safe Q2 PAK browsing/resource reads, strict writes, traversal rejection\n";
}
