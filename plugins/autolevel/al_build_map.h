// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include "al_solver.h"
#include "al_brush.h"
namespace al {
struct MapBuild {std::string map;std::vector<float> lines;std::vector<std::string> warnings;Geometry geometry;int brushes=0,modules=0;};
bool shouldInclude(const RepoRuntime&,const LevelData&,size_t cell,int layer);
MapBuild buildMap(const RepoRuntime&,const Builder&,const EditorSettings&,int profile,const std::string& fallbackMaterial="fb_grey",std::atomic<bool>* cancel=nullptr,int idBase=1000);
MapBuild buildWorkbench(const Repo&,Vec3 origin,int spacing);
}
