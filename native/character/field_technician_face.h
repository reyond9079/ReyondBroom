// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "humanoid.h"

namespace ftebroom::character {

// Shape the visible facial planes in the fitted rig's local coordinates.
// Compact envelopes keep the displacement continuous at the edges and the
// lip centre, including when neighboring facial controls are applied.
inline Vec3 fieldTechnicianFace(Vec3 point) {
  if (point[1]<14.71 || point[1]>16.02) return point;
  const double x = std::abs(point[0]), y = point[1], z = point[2];
  const auto influence = [&](Vec3 centre,Vec3 size) {
    const auto dx = (x-centre[0])/size[0], dy = (y-centre[1])/size[1], dz = (z-centre[2])/size[2];
    const auto weight = std::max(0.,1-dx*dx-dy*dy-dz*dz);
    return weight*weight;
  };
  const auto lowerLip = influence({.065,15.224,.659},{.29,.116,.26});
  const auto upperLip = influence({.063,15.291,.745},{.27,.103,.24});
  const auto mouthCorner = influence({.235,15.252,.668},{.17,.135,.26});
  const auto chin = influence({.02,15.055,.59},{.46,.31,.50});
  const auto jawAngle = influence({.57,15.23,.08},{.32,.39,.59});
  const auto jowl = influence({.48,15.18,.52},{.34,.43,.50});
  const auto cheek = influence({.45,15.59,.51},{.36,.36,.42});
  const auto belowCheek = influence({.43,15.41,.61},{.29,.29,.33});
  const auto noseTip = influence({.01,15.473,.871},{.23,.24,.37});
  const auto noseBridge = influence({0.,15.711,.70},{.21,.27,.32});
  const auto eye = influence({.285,15.792,.59},{.27,.23,.40});
  const auto outerLid = influence({.41,15.811,.56},{.22,.15,.41});
  const auto beneathChin = influence({.06,15.025,.29},{.53,.31,.39});
  const auto jawFloor = influence({.035,15.012,.49},{.41,.21,.25});
  point[0] += std::copysign(1.,point[0])*(.054*jawAngle-.043*jowl-.010*cheek);
  point[1] += -.007*lowerLip-.017*upperLip-.047*mouthCorner-.089*chin+
              .113*jawAngle+.015*outerLid+.089*beneathChin+.041*jawFloor;
  point[2] += .032*lowerLip-.028*upperLip+.054*chin-.085*belowCheek-.024*noseTip+
              .012*noseBridge-.041*eye;
  return point;
}
}
