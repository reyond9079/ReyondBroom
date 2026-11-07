// SPDX-License-Identifier: GPL-3.0-or-later
// Build against frozen baseline 9e5b30a headers, never the current native tree.
// Comparing both binaries on the same OS keeps libm differences out of the
// compatibility contract while still requiring exact original output.
#include <plant/generator.h>
#include <plant/presets.h>

#include <iostream>
#include <plant/textures.h>
using namespace ftebroom::plant;
static uint64_t hash(const std::string& s){uint64_t h=1469598103934665603ULL;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;}
static std::string fingerprint(const Mesh& m){std::string bytes;auto append=[&](auto x){bytes.append(reinterpret_cast<const char*>(&x),sizeof x);};for(auto& v:m.vertices){for(double x:v.p)append(x);for(double x:v.n)append(x);for(double x:v.uv)append(x);append(v.wind);append(v.phase);}for(auto& f:m.faces){for(unsigned i:f.i)append(i);append(f.material);append(int(f.part));append(f.organ);}std::ostringstream s;s<<std::hex<<hash(bytes);return s.str();}
int main(){Json out=Json::array();for(auto d:presets()){auto j=serialize(d);Json seeds=Json::array();for(unsigned seed:{137u,138u,424242u}){d.seed=seed;Json lods=Json::array();for(int l=0;l<3;l++)lods.push_back(fingerprint(generate(d,l)));auto maps=textureMaps(d,nullptr,nullptr,48);Json tex=Json::array();for(auto* im:{&maps.albedo,&maps.normal,&maps.orm}){std::ostringstream id;id<<std::hex<<hash(std::string(im->rgba.begin(),im->rgba.end()));tex.push_back(id.str());}seeds.push_back({{"seed",seed},{"lods",lods},{"textures",tex}});}out.push_back({{"project",j},{"seeds",seeds}});}std::cout<<out.dump(2);}
