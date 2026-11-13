// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_quake1.h"
#include <map>
#include <set>
namespace ftebroom::quake1 {
inline std::string mipWad(const std::map<std::string,std::string>& lumps,bool hl=false){
    std::string output(hl?"WAD3":"WAD2",4);output.resize(12,0);std::string directory;
    auto put=[](std::string& to,size_t at,uint32_t value){for(int b=0;b<4;b++)to[at+b]=char(value>>(b*8));};
    for(const auto& item:lumps){
        if(output.size()+item.second.size()>512u*1024u*1024u)throw std::runtime_error("Compiler WAD exceeds 512 MiB.");
        size_t entry=directory.size();directory.resize(entry+32,0);put(directory,entry,output.size());put(directory,entry+4,item.second.size());put(directory,entry+8,item.second.size());directory[entry+12]=hl?67:68;
        directory.replace(entry+16,std::min(size_t(15),item.first.size()),item.first.substr(0,15));output+=item.second;
    }
    put(output,4,lumps.size());put(output,8,output.size());output+=directory;return output;
}
// Copy original mip chains unchanged. The last archive wins, as in the editor.
inline std::string compilerWad(const std::vector<std::string>& archives,const std::set<std::string>& wanted,bool hl=false){
    std::map<std::string,std::string> lumps;
    for(const auto& bytes:archives){
        if(bytes.compare(0,4,hl?"WAD3":"WAD2"))throw std::runtime_error(hl?"Half-Life requires WAD3 resources.":"Quake requires WAD2 resources.");
        auto count=word(bytes,4),offset=word(bytes,8);if(count>65536)throw std::runtime_error("Too many WAD entries.");range(bytes,offset,uint64_t(count)*32);
        for(unsigned i=0;i<count;i++){
            size_t at=offset+i*32;if((unsigned char)bytes[at+12]!=(hl?67:68))continue;
            auto key=name(bytes,at+16,16);for(char& c:key)c=char(std::tolower((unsigned char)c));if(!wanted.count(key))continue;
            if(bytes[at+13])throw std::runtime_error("Compressed WAD textures are unsupported.");
            auto start=word(bytes,at),size=word(bytes,at+4);range(bytes,start,size);auto lump=bytes.substr(start,size);
            if(lump.size()<40)throw std::runtime_error("Truncated WAD mip texture.");lumps[key]=std::move(lump);
        }
    }
    return mipWad(lumps,hl);
}

inline std::string embeddedTextureWad(const std::string& bytes,const std::set<std::string>& wanted) {
    if(!bsp(bytes,true))return {};
    const uint64_t start=word(bytes,20),length=word(bytes,24);
    if(length<4)return {};
    const auto count=word(bytes,start);
    const uint64_t tableEnd=4+uint64_t(count)*4;
    if(count>65536 || tableEnd>length)return {};
    std::map<std::string,std::string> lumps;
    for(uint32_t i=0;i<count;i++){
        const uint64_t offset=word(bytes,start+4+i*4);
        if(offset<tableEnd || offset>length || length-offset<40)continue;
        const auto at=start+offset;
        auto key=name(bytes,at,16);
        for(auto& c:key)c=char(std::tolower((unsigned char)c));
        if(!wanted.count(key) || key.empty() || key.size()>15 ||
           key.find_first_of("/\\\";\r\n\t ")!=key.npos)continue;
        const auto width=word(bytes,at+16),height=word(bytes,at+20);
        if(!width||!height||width>4096||height>4096)continue;
        uint64_t end=40;
        bool valid=true;
        for(int mip=0;mip<4;mip++){
            const uint64_t position=word(bytes,at+24+4*mip),size=uint64_t(width>>mip)*(height>>mip);
            if(position<40||position>length-offset||size>length-offset-position){valid=false;break;}
            end=std::max(end,position+size);
        }
        if(valid)lumps.emplace(key,bytes.substr(at,end));
    }
    return lumps.empty()?std::string{}:mipWad(lumps);
}
}
