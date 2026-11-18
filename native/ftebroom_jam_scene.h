// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "third_party/json.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <string>

namespace ftebroom::jam {
struct WorldPreview {
  std::string revision, model;
  nlohmann::json faces=nlohmann::json::array();
};
inline std::array<float,3> worldHalfSize(const nlohmann::json& rules)
{
  std::array<float,3> result{1024,1024,512};
  if (!rules.contains("bounds") || !rules["bounds"].contains("maxSize")) return result;
  const auto& size=rules["bounds"]["maxSize"];
  if (!size.is_array() || size.size()!=3) return result;
  for (int axis=0;axis<3;++axis) {
    if (!size[axis].is_number()) return {1024,1024,512};
    const auto value=size[axis].get<float>();
    if (!std::isfinite(value) || value<=0 || value>2048) return {1024,1024,512};
    result[axis]=value*.5f;
  }
  return result;
}
inline std::array<float,3> worldPlotCenter(const std::string& plot)
{
  return {float(plot[1]-'1')*2048.f,-float(plot[0]-'A')*2048.f,0};
}
inline bool worldMaterial(const std::string& name)
{
  return !name.empty() && name.size()<=64 && name.find("..")==name.npos &&
    name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+*/_.{}!-")==name.npos;
}
inline const WorldPreview* scenePreview(const nlohmann::json& item, const std::string& local,
                                       const std::map<std::string,WorldPreview>& previews)
{
  const auto plot=item.value("plot",std::string{}), mode=item.value("mode",std::string{});
  if (plot==local || plot.size()!=2 || plot[0]<'A' || plot[0]>'E' || plot[1]<'1' || plot[1]>'5' ||
      (mode!="map" && mode!="bsp")) return nullptr;
  const auto found=previews.find(plot);
  // Announcing a revision is not a render update. Retain the last complete
  // preview until its replacement arrives; an explicit empty plot removes it.
  return found==previews.end()?nullptr:&found->second;
}
inline std::string worldSceneKey(const nlohmann::json& state, const std::string& local,
                                const std::string& hub, const std::map<std::string,WorldPreview>& previews)
{
  std::string key=local+"|"+hub;
  for (const auto& item:state.at("plots")) if (const auto* preview=scenePreview(item,local,previews))
    key+="|"+item.at("plot").get<std::string>()+":"+preview->revision+":"+preview->model;
  return key;
}
inline std::string worldScene(const nlohmann::json& state, const std::string& local,
                             const std::string& hub, const std::map<std::string,WorldPreview>& previews,
                             int generation=0)
{
  std::ostringstream scene;
  scene.imbue(std::locale::classic());scene<<std::setprecision(7)<<"RBJAMSCENE 1 "<<generation<<'\n';
  const auto own=worldPlotCenter(local);
  if (!hub.empty()) {
    const auto center=worldPlotCenter("C3");
    scene<<"model "<<hub<<' '<<center[0]-own[0]<<' '<<center[1]-own[1]<<" 0 0\n";
  }
  for (const auto& item:state.at("plots")) {
    const auto* preview=scenePreview(item,local,previews);
    if (!preview) continue;
    auto offset=worldPlotCenter(item.at("plot").get<std::string>());
    for (int axis=0;axis<3;++axis) offset[axis]-=own[axis];
    if (!preview->model.empty()) {
      scene<<"model "<<preview->model<<' '<<offset[0]<<' '<<offset[1]<<" 0 0\n";
      continue;
    }
    for (const auto& face:preview->faces) {
      const auto material=face.at(0).get<std::string>();
      if (!worldMaterial(material)) continue;
      scene<<"face "<<material<<' '<<face.at(1).size()<<'\n';
      for (const auto& p:face.at(1))
        scene<<p.at(0).get<float>()+offset[0]<<' '<<p.at(1).get<float>()+offset[1]<<' '<<p.at(2).get<float>()
             <<' '<<(p.size()==5?p[3].get<float>():p[0].get<float>()/64)
             <<' '<<(p.size()==5?p[4].get<float>():p[2].get<float>()/64)<<'\n';
    }
  }
  return scene.str()+"RBJAMSCENE-END "+std::to_string(generation)+"\n";
}
}
