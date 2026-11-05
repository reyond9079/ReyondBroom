// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include "al_math.h"
namespace al {
bool rayBox(Vec3 origin,Vec3 direction,Vec3 low,Vec3 high,double& distance,int* face=nullptr);
}

namespace alui {extern bool viewportBoundsEnabled,bigViewportEnabled,blockPickEnabled;extern int boundsHandleMode,bigEditMode,blockEditSelection;extern al::Int3 bigCell;}
