// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include "reyondbroom_plugin.h"
#include "al_storage.h"
#include "al_build_map.h"
#include "al_default_repo.h"
#include "al_group_solver.h"
#include "al_viewport.h"
#include <future>
#include <cstdio>
#include <functional>
namespace alui {
using namespace al;
extern const RBHostV1* host;extern int page,repoIndex,builderIndex,blockIndex,variantIndex,bigIndex,groupIndex;extern bool ready,initialized;
extern std::vector<Repo> repos;extern std::vector<Builder> builders;extern EditorSettings settings;extern std::string message;extern MapBuild preview;extern bool replaceSelected;extern int captureMode;extern std::atomic<bool> cancel;extern std::atomic<float> progress;
extern std::map<std::string,std::shared_ptr<const RepoRuntime>> compiled;
#define AL_HAS(field) (host && host->struct_size >= offsetof(RBHostV1,field)+sizeof(host->field) && host->field)
Repo* currentRepo();Builder* currentBuilder();BlockAsset* currentBlock();Variant* currentVariant();BigBlockAsset* currentBig();
void changed();void startJob(bool selection=false,bool group=false,bool validate=false);void capture(int mode);void applyPreview();void saveAll(bool explicitSave=false);void loadAll();void clearPreview();void drawBuilder();void drawRepo();void drawBlock();void drawBig();void drawBuilders();void viewport();
bool button(const std::string&);bool integer(const std::string&,int&);bool real(const std::string&,float&);bool real64(const std::string&,double&);bool textInput(const std::string&,std::string&);bool checkbox(const std::string&,bool&);bool combo(const std::string&,int&,const std::vector<std::string>&);bool section(const std::string&,int=0);void line(const std::string&);void vectorInput(const std::string&,Vec3&);void int3Input(const std::string&,Int3&);void error(const std::exception&);std::string uniqueId(const char*);std::string readData(const std::string&);void writeData(const std::string&,const std::string&);uint64_t stateKey();
struct Disable {explicit Disable(bool on=true):on(on){if(on)host->disabled(1);}~Disable(){if(on)host->disabled(0);}bool on;};
}
