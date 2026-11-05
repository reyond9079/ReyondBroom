// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include "al_brush.h"
namespace al {
Repo defaultRepo();Builder defaultBuilder(const Repo&,bool empty=false);
Geometry corridorCell(const std::array<bool,6>&);Geometry roomCell(const std::array<bool,6>& walls,const std::array<bool,6>& doors,bool floor=true,bool ceiling=true,double z0=16,double z1=176,double doorZ=16);
Geometry stairGeometry();Geometry atriumGeometry();
BigBlockAsset bigFromGeometry(Repo&,const Geometry&,Int3 size,const std::string& uid,const std::string& name,const std::string& group,float weight);
}
