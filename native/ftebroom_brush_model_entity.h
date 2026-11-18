// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_entity_defs.h"

namespace ftebroom {
struct BrushModelEntity {
  const char* classname=nullptr;
  bool writeMdlKey=false;
  bool fteQuake=false;
};

inline BrushModelEntity brushModelEntity(int profile,bool mdl,bool allowFteQuake,
  const EntityDefinitions& definitions)
{
  const auto declared=[&](const char* classname){
    auto definition=definitions.find(classname);
    if(definition==definitions.end()||definition->second.brush||definition->second.base)
      return BrushModelEntity{};
    const auto& entity=definition->second;
    const bool usesMdl=entity.modelKey=="mdl"||entity.properties.count("mdl");
    if(usesMdl||entity.modelKey=="model"||entity.properties.count("model"))
      return BrushModelEntity{classname,usesMdl,false};
    return BrushModelEntity{};
  };
  if(profile==4&&mdl)return {"cycler",false,false};
  if(profile==1)return declared("func_model");
  if(profile==2){
    const auto native=declared("misc_model");
    if(native.classname)return native;
    if(allowFteQuake)return {"misc_model",false,true};
  }
  // These BSP exports are standalone files. An inline brush entity such as
  // GoldSrc func_wall cannot stand in for a verified external-model spawner.
  return {};
}
}
