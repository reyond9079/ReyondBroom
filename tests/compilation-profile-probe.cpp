// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/third_party/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
int probeMain(const std::vector<std::string>& args){
 if(args.size()>1&&args[1]=="exit")return args.size()>2?std::stoi(args[2]):7;
 if(args.size()>1&&args[1]=="sleep"){std::this_thread::sleep_for(std::chrono::milliseconds(args.size()>2?std::stoi(args[2]):10000));return 0;}
 if(args.size()>3&&args[1]=="write"){std::ofstream(std::filesystem::u8path(args[2]),std::ios::binary)<<args[3];return 0;}
 auto cwd=std::filesystem::current_path().u8string();std::string utf8cwd(cwd.begin(),cwd.end());
 std::cout<<"PROBE_RAN "<<nlohmann::json{{"argv",args},{"cwd",utf8cwd}}.dump()<<'\n';return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> args;for(int i=0;i<argc;++i){int n=WideCharToMultiByte(CP_UTF8,0,argv[i],-1,nullptr,0,nullptr,nullptr);std::string s(size_t(n),0);WideCharToMultiByte(CP_UTF8,0,argv[i],-1,s.data(),n,nullptr,nullptr);s.resize(s.size()-1);args.push_back(s);}return probeMain(args);}
#else
int main(int argc,char** argv){return probeMain(std::vector<std::string>(argv,argv+argc));}
#endif
