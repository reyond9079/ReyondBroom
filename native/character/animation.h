// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "humanoid.h"
#include "captured_gait.h"
#include <map>

namespace ftebroom::character {
inline constexpr char animationBank[] = "Humanoid_Core_v1";
enum class Motion {
  Neutral, Relaxed, Alert, Walk, Backward, Left, Right, Run, TurnLeft, TurnRight,
  Crouch, CrouchWalk, JumpStart, JumpLoop, JumpLand, SitDown, Sit, Stand, Wave,
  Point, Talk, Melee, Aim, HitFront, HitBack, DeathFront, DeathBack, Imported
};
struct Clip {
  const char* id;
  double seconds;
  bool loop;
  Motion motion;
};
inline constexpr Clip clips[] = {
  {"idle_neutral", 3.2, true, Motion::Neutral},
  {"idle_relaxed", 4, true, Motion::Relaxed},
  {"idle_alert", 2.2, true, Motion::Alert},
  {"walk_forward", 1.12, true, Motion::Walk},
  {"walk_backward", 1.27, true, Motion::Backward},
  {"strafe_left", 1.16, true, Motion::Left},
  {"strafe_right", 1.16, true, Motion::Right},
  {"run_forward", .74, true, Motion::Run},
  {"turn_left", .74, false, Motion::TurnLeft},
  {"turn_right", .74, false, Motion::TurnRight},
  {"crouch_idle", 2.5, true, Motion::Crouch},
  {"crouch_walk", 1.22, true, Motion::CrouchWalk},
  {"jump_start", .37, false, Motion::JumpStart},
  {"jump_loop", .59, true, Motion::JumpLoop},
  {"jump_land", .52, false, Motion::JumpLand},
  {"sit_down", .9, false, Motion::SitDown},
  {"sit_idle", 3.2, true, Motion::Sit},
  {"stand_up", .94, false, Motion::Stand},
  {"wave", 1.85, false, Motion::Wave},
  {"point", 1.35, false, Motion::Point},
  {"talk_gesture", 2.75, true, Motion::Talk},
  {"melee_attack", .68, false, Motion::Melee},
  {"aim_idle", 2.3, true, Motion::Aim},
  {"hit_front", .58, false, Motion::HitFront},
  {"hit_back", .52, false, Motion::HitBack},
  {"death_front", 1.15, false, Motion::DeathFront},
  {"death_back", 1.25, false, Motion::DeathBack}
};

struct CapturedGait {
  const CapturedGaitFrame (&frames)[41];
  double seconds, rootHeight, spinePitch;
};
inline constexpr CapturedGait walkGait{walkCapture,walkCaptureSeconds,-.25,1.5};
inline constexpr CapturedGait runGait{runCapture,runCaptureSeconds,-.27,5.};
inline constexpr CapturedGait backwardGait{backwardCapture,backwardCaptureSeconds,-.28,-2.5};
inline constexpr CapturedGait strafeGait{strafeRightCapture,strafeRightCaptureSeconds,-.31,4.};
inline constexpr CapturedGait crouchGait{crouchWalkCapture,crouchWalkCaptureSeconds,-1.12,15.};

inline const CapturedGait* capturedGait(Motion motion) {
  switch (motion) {
    case Motion::Walk: return &walkGait;
    case Motion::Run: return &runGait;
    case Motion::Backward: return &backwardGait;
    case Motion::Left: case Motion::Right: return &strafeGait;
    case Motion::CrouchWalk: return &crouchGait;
    default: return nullptr;
  }
}

inline Clip captureTiming(Clip clip,bool useCapture) {
  if (const auto* gait = useCapture ? capturedGait(clip.motion) : nullptr)
    clip.seconds = gait->seconds;
  return clip;
}

struct AnimationPose {
  Vec3 root{};
  std::vector<Mat4> rotations;
};

inline constexpr char importedClipId[] = "imported_motion";
struct ImportedMotion {
  double seconds = 0;
  bool loop = false;
  std::vector<AnimationPose> samples;
  Clip definition() const { return {importedClipId,seconds,loop,Motion::Imported}; }
};

inline std::vector<Clip> characterClips(const ImportedMotion* imported) {
  std::vector<Clip> result(std::begin(clips),std::end(clips));
  if (imported) result.push_back(imported->definition());
  return result;
}

inline bb::Vec4 rotationQuaternion(const Mat4& matrix) {
  auto diagonal = [&](int row) { return matrix.m[row][row]; };
  const auto trace = diagonal(0) + diagonal(1) + diagonal(2);
  if (trace > 0) {
    const auto factor = 2 * std::sqrt(1 + trace);
    return {(matrix.m[2][1]-matrix.m[1][2])/factor, (matrix.m[0][2]-matrix.m[2][0])/factor,
            (matrix.m[1][0]-matrix.m[0][1])/factor, factor / 4};
  }
  int i = diagonal(0) > diagonal(1) ? 0 : 1;
  if (diagonal(2) > diagonal(i)) i = 2;
  const int j = (i + 1) % 3, k = (i + 2) % 3;
  const auto factor = 2 * std::sqrt(1 + diagonal(i) - diagonal(j) - diagonal(k));
  bb::Vec4 q{};
  q[i] = factor / 4;
  q[j] = (matrix.m[j][i] + matrix.m[i][j]) / factor;
  q[k] = (matrix.m[k][i] + matrix.m[i][k]) / factor;
  q[3] = (matrix.m[k][j] - matrix.m[j][k]) / factor;
  return q;
}

inline Mat4 rotationMatrix(const bb::Vec4& q) {
  const auto [x,y,z,w] = q;
  auto matrix = Mat4::identity();
  matrix.m[0][0] -= 2*(y*y+z*z);
  matrix.m[0][1] = 2*(x*y-w*z);
  matrix.m[0][2] = 2*(x*z+w*y);
  matrix.m[1][0] = 2*(x*y+w*z);
  matrix.m[1][1] -= 2*(x*x+z*z);
  matrix.m[1][2] = 2*(y*z-w*x);
  matrix.m[2][0] = 2*(x*z-w*y);
  matrix.m[2][1] = 2*(y*z+w*x);
  matrix.m[2][2] -= 2*(x*x+y*y);
  return matrix;
}

inline Mat4 blendRotation(const Mat4& from,const Mat4& to,double amount) {
  auto a = rotationQuaternion(from), b = rotationQuaternion(to);
  double cosine = 0;
  for (unsigned i = 0; i < a.size(); ++i) cosine += a[i]*b[i];
  if (cosine < 0) {
    for (auto& component : b) component = -component;
    cosine = -cosine;
  }
  double first = 1-amount, second = amount;
  if (cosine < .9995) {
    const auto angle = std::acos(std::clamp(cosine,0.,1.));
    first = std::sin(first*angle)/std::sin(angle);
    second = std::sin(second*angle)/std::sin(angle);
  }
  double norm = 0;
  for (unsigned i = 0; i < a.size(); ++i) {
    a[i] = first*a[i] + second*b[i];
    norm += a[i]*a[i];
  }
  for (auto& component : a) component /= std::sqrt(norm);
  return rotationMatrix(a);
}

inline AnimationPose blendAnimation(const AnimationPose& from,const AnimationPose& to,double amount,
                                    const std::vector<bool>& mask = {}) {
  if (!std::isfinite(amount) || from.rotations.size() != to.rotations.size() ||
      (!mask.empty() && mask.size() != from.rotations.size()))
    throw std::runtime_error("Incompatible character animation layers");
  const auto weight = std::clamp(amount,0.,1.);
  auto pose = from;
  if (mask.empty()) pose.root = add(mul(from.root,1-weight),mul(to.root,weight));
  for (std::size_t joint = 0; joint < pose.rotations.size(); ++joint)
    if (mask.empty() || mask[joint])
      pose.rotations[joint] = blendRotation(from.rotations[joint],to.rotations[joint],weight);
  return pose;
}

inline AnimationPose sampleImportedMotion(const ImportedMotion& motion,double seconds) {
  const auto position=std::clamp(seconds/motion.seconds,0.,1.)*(motion.samples.size()-1);
  const auto frame=std::size_t(position);
  if (frame+1==motion.samples.size()) return motion.samples.back();
  return blendAnimation(motion.samples[frame],motion.samples[frame+1],position-frame);
}

inline std::vector<bool> upperBodyMask(const Humanoid& rig) {
  std::vector<bool> upper;
  upper.reserve(rig.joints.size());
  for (const auto& joint : rig.joints)
    upper.push_back(joint.name=="spine01" || (joint.parent>=0 && upper.at(joint.parent)));
  return upper;
}

inline Mat4 rotationBetween(Vec3 from,Vec3 to) {
  from = unit(from);
  to = unit(to);
  auto axis = cross(from,to);
  const auto alignment = std::clamp(dot(from,to),-1.,1.);
  if (alignment < -.99999) {
    axis = unit(cross(from,std::abs(from[0])<.8 ? Vec3{1,0,0} : Vec3{0,1,0}));
    return rotationMatrix({axis[0],axis[1],axis[2],0});
  }
  const auto half = std::sqrt((1+alignment)*.5);
  axis = mul(axis,.5/half);
  return rotationMatrix({axis[0],axis[1],axis[2],half});
}

class Animator {
  const Humanoid& rig;
  bool useCapture;
  std::map<std::string, std::size_t, std::less<>> joints;

  struct Leg {
    std::size_t hip, knee, ankle;
    Vec3 thigh, shin;
  };
  Leg leg(bool left) const {
    const auto hip = joints.at(left ? "upperleg01.L" : "upperleg01.R");
    const auto knee = joints.at(left ? "lowerleg01.L" : "lowerleg01.R");
    const auto ankle = joints.at(left ? "foot.L" : "foot.R");
    return {hip,knee,ankle,sub(rig.joints[knee].bindPosition,rig.joints[hip].bindPosition),
                          sub(rig.joints[ankle].bindPosition,rig.joints[knee].bindPosition)};
  }

  void placeFoot(AnimationPose& pose,const Leg& chain,Vec3 target,double pitch,double yaw=0) const {
    const auto hip = add(rig.joints[chain.hip].bindPosition,pose.root);
    const auto ray = sub(target,hip);
    const auto axis = unit(ray);
    const double thigh = bb::length(chain.thigh), shin = bb::length(chain.shin);
    const auto length = std::clamp(bb::length(ray),std::abs(thigh-shin)+.01,thigh+shin-.016);
    const auto along = (length*length+thigh*thigh-shin*shin)/(2*length);
    const auto outward = std::sqrt(std::max(0.,thigh*thigh-along*along));
    const Vec3 forward{0,0,1};
    const auto pole = unit(sub(forward,mul(axis,dot(forward,axis))));
    const auto knee = add(mul(axis,along),mul(pole,outward));
    const auto calf = sub(mul(axis,length),knee);
    const auto upper = rotationBetween(chain.thigh,knee);
    const auto lower = rotationBetween(chain.shin,calf);
    pose.rotations[chain.hip] = upper;
    pose.rotations[chain.knee] = bb::inverse(upper)*lower;
    pose.rotations[chain.ankle] = bb::inverse(lower)*bb::rotation({pitch,yaw,0});
  }

  double standingHeight(const AnimationPose& pose,double stance,double bendReserve) const {
    double height = 1e6;
    for (bool left : {true,false}) {
      const auto chain = leg(left);
      auto ankle = rig.joints[chain.ankle].bindPosition;
      ankle[0] = std::copysign(stance,ankle[0]);
      const auto hip = rig.joints[chain.hip].bindPosition;
      const auto dx = ankle[0]-hip[0]-pose.root[0];
      const auto dz = ankle[2]-hip[2]-pose.root[2];
      const auto reach = bb::length(chain.thigh)+bb::length(chain.shin)-bendReserve;
      height = std::min(height,ankle[1]+std::sqrt(std::max(0.,reach*reach-dx*dx-dz*dz))-hip[1]);
    }
    return height;
  }

  static double transition(double t) {
    t = std::clamp(t,0.,1.);
    return t*t*(3-2*t);
  }

  Vec3 clearArm(Vec3 start, Vec3 direction, double travel, double side, double radius,
                const std::array<double,5>& samples) const {
    direction = unit(direction);
    // Keep the swing in the sagittal plane. Extend laterally just enough to
    // clear the fitted torso while keeping each segment at its original length.
    for (unsigned pass=0; pass<4; ++pass) {
      double required = side*direction[0];
      for (const auto step : samples) {
        const double length = travel*step;
        const auto point = add(start,mul(direction,length));
        const auto clearance = rig.torso.armClearance(point,radius);
        if (clearance>0) required = std::max(required,(clearance-side*start[0])/length);
      }
      if (required<=side*direction[0]+1e-4) break;
      const auto lateral = std::min(.76,required);
      const auto swing = unit({0,direction[1],direction[2]});
      direction = add({side*lateral,0,0},mul(swing,std::sqrt(1-lateral*lateral)));
    }
    return direction;
  }

  struct Hand {
    std::size_t wrist;
    std::array<std::array<std::size_t,3>,5> digits;
    Vec3 forward, across, palm;
  };
  std::array<Hand,2> handRig;

  static Mat4 rotateAround(Vec3 axis,double radians) {
    const auto halfAxis = mul(axis,std::sin(radians*.5));
    return rotationMatrix({halfAxis[0],halfAxis[1],halfAxis[2],std::cos(radians*.5)});
  }

  void poseFingers(AnimationPose& pose,bool left,double curl,double point = 0) const {
    const auto& hand = handRig[left ? 1 : 0];
    const auto bind = [&](std::size_t i) { return rig.joints[i].bindPosition; };
    constexpr double flex[]{.71,1.12,.53};
    for (unsigned finger=1; finger<hand.digits.size(); ++finger) {
      const auto& chain = hand.digits[finger];
      const auto active = finger==1 ? 1-point : 1.;
      const auto degrees = curl*(.89+.07*double(finger))*active;
      for (unsigned joint=0; joint<chain.size(); ++joint) {
        const auto bone = joint<2 ? sub(bind(chain[joint+1]),bind(chain[joint]))
                                  : sub(bind(chain[2]),bind(chain[1]));
        const auto direction = unit(bone);
        const auto hinge = unit(cross(direction,hand.palm));
        const auto bend = rotateAround(hinge,degrees*flex[joint]*bb::pi/180.);
        if (joint) {
          pose.rotations[chain[joint]] = bend;
          continue;
        }
        // The bind pose spreads for weighting. Preserve knuckle spacing while
        // drawing the tips in, with deeper flexion towards the little finger.
        const auto spread = dot(direction,hand.across);
        const auto converge = .12+.28*std::clamp(curl/38.,0.,1.)*active;
        const auto relaxed = unit(sub(direction,mul(hand.across,spread*converge)));
        pose.rotations[chain[joint]] = rotationBetween(direction,relaxed)*bend;
      }
    }
    const auto& thumb = hand.digits.front();
    const auto thumbAxis = sub(bind(thumb[1]),bind(thumb[0]));
    const auto toIndex = sub(bind(hand.digits[1][1]),bind(thumb[0]));
    const auto fold = .12+.37*std::clamp(curl/40.,0.,1.);
    const auto opposed = unit(add(mul(unit(thumbAxis),1-fold),
                                  mul(unit(add(toIndex,mul(hand.palm,.13))),fold)));
    pose.rotations[thumb[0]] = rotationBetween(thumbAxis,opposed);
    for (unsigned joint=1; joint<thumb.size(); ++joint) {
      const auto bone = sub(bind(thumb[2]),bind(thumb[1]));
      const auto inward = add(mul(hand.across,-.65),mul(hand.palm,.35));
      const auto hinge = unit(cross(bone,inward));
      pose.rotations[thumb[joint]] = rotateAround(hinge,curl*(joint==1 ? .25 : .16)*bb::pi/180.);
    }
  }

  void poseArm(AnimationPose& pose,bool left,Vec3 upperDirection,Vec3 lowerDirection,
               double depression = 0,bool avoidTorso = false,double palmForward = 0) const {
    const auto shoulder = joints.at(left ? "upperarm01.L" : "upperarm01.R");
    const auto elbow = joints.at(left ? "lowerarm01.L" : "lowerarm01.R");
    const auto& hand = handRig[left ? 1 : 0];
    const auto wrist = hand.wrist;
    const auto upperBind = sub(rig.joints[elbow].bindPosition,rig.joints[shoulder].bindPosition);
    const auto lowerBind = sub(rig.joints[wrist].bindPosition,rig.joints[elbow].bindPosition);
    const double side = left ? 1. : -1.;
    auto shoulderPoint = rig.joints[shoulder].bindPosition;
    auto collarInverse = Mat4::identity();
    if (depression) {
      const auto clavicle = joints.at(left ? "clavicle.L" : "clavicle.R");
      const auto origin = rig.joints[clavicle].bindPosition;
      const auto collar = bb::rotation({0,0,-side*depression});
      pose.rotations[clavicle] = collar;
      collarInverse = bb::rotation({0,0,side*depression});
      shoulderPoint = add(origin,collar.vector(sub(shoulderPoint,origin)));
    }
    if (avoidTorso) {
      const auto upperLength = bb::length(upperBind);
      upperDirection = clearArm(shoulderPoint,upperDirection,upperLength,side,rig.torso.upperArmPadding,
                                {.42,.56,.70,.85,1.});
      const auto elbowPoint = add(shoulderPoint,mul(upperDirection,upperLength));
      const auto lengthToHand = bb::length(lowerBind)+.88;
      lowerDirection = clearArm(elbowPoint,lowerDirection,lengthToHand,side,rig.torso.forearmPadding,
                                {.14,.34,.56,.78,1.});
    }
    const auto upper = rotationBetween(upperBind,upperDirection);
    const auto lower = rotationBetween(lowerBind,lowerDirection);
    const auto axis = unit(lowerDirection);
    const auto straight = rotationBetween(lower.vector(hand.forward),axis)*lower;
    const auto currentPalm = straight.vector(hand.palm);
    const Vec3 facing{-side*(1-palmForward),-.18*(1-palmForward),palmForward};
    const auto towards = sub(facing,mul(axis,dot(facing,axis)));
    const auto roll = bb::length(towards)>.15 ?
        std::atan2(dot(axis,cross(currentPalm,towards)),dot(currentPalm,towards)) : 0.;
    const auto twist = std::clamp(roll,-1.45,1.45);
    const auto forearm = rotateAround(axis,twist*.72)*lower;
    pose.rotations[shoulder] = collarInverse*upper;
    pose.rotations[elbow] = bb::inverse(upper)*forearm;
    pose.rotations[wrist] = bb::inverse(forearm)*rotateAround(axis,twist)*straight;
  }
public:
  explicit Animator(const Humanoid& rig,bool useCapture=false) : rig(rig),useCapture(useCapture) {
    for (std::size_t i = 0; i < rig.joints.size(); ++i) joints.emplace(rig.joints[i].name, i);
    const auto bind = [&](std::size_t i) { return rig.joints[i].bindPosition; };
    for (unsigned side=0; side<handRig.size(); ++side) {
      const std::string suffix = side ? ".L" : ".R";
      auto& hand = handRig[side];
      hand.wrist = joints.at("wrist"+suffix);
      for (unsigned digit=0; digit<hand.digits.size(); ++digit)
        for (unsigned part=0; part<hand.digits[digit].size(); ++part)
          hand.digits[digit][part] = joints.at("finger"+std::to_string(digit+1)+"-"+std::to_string(part+1)+suffix);
      const auto index = bind(hand.digits[1][0]), little = bind(hand.digits[4][0]);
      hand.forward = unit(sub(mul(add(index,little),.5),bind(hand.wrist)));
      const auto span = sub(index,little);
      hand.across = unit(sub(span,mul(hand.forward,dot(hand.forward,span))));
      hand.palm = mul(unit(cross(hand.forward,hand.across)),side ? 1. : -1.);
    }
  }
  AnimationPose comparisonPose() const {
    AnimationPose pose{{},std::vector<Mat4>(joints.size(),Mat4::identity())};
    pose.root[1] = standingHeight(pose,1.38,.017);
    for (bool left : {true,false}) {
      const auto side = left ? 1. : -1.;
      poseArm(pose,left,{side,-.203,0},{side,-.103,0},2.);
      poseFingers(pose,left,3.);
      const auto chain = leg(left);
      auto target = rig.joints[chain.ankle].bindPosition;
      target[0] = side*1.38;
      placeFoot(pose,chain,target,0.,side*4.);
    }
    return pose;
  }
  AnimationPose sample(const Clip& clip, double seconds) const {
    if (!std::isfinite(seconds) || !std::isfinite(clip.seconds) || clip.seconds <= 0)
      throw std::runtime_error("Invalid animation time");
    const auto phase = clip.loop ? std::fmod(std::max(0., seconds), clip.seconds) / clip.seconds
                                 : std::clamp(seconds / clip.seconds, 0., 1.);
    const auto turn = phase * 2 * bb::pi;
    const auto a = std::sin(turn);
    const auto b = std::cos(turn);
    const auto pulse = std::sin(phase * bb::pi);
    const auto smooth = phase * phase * (3 - 2 * phase);
    AnimationPose pose{{}, std::vector<Mat4>(joints.size(), Mat4::identity())};
    auto set = [&](const char* joint, Vec3 angles) {
      const auto id = joints.find(joint);
      if (id == joints.end()) throw std::runtime_error("Animation joint is missing from the canonical rig");
      pose.rotations[id->second] = bb::rotation(angles);
    };
    auto hands = [&](double curl) {
      poseFingers(pose,true,curl);
      poseFingers(pose,false,curl);
    };
    auto arms = [&](double left, double right, double reach = 0,double depression = 0) {
      for (bool l : {true,false}) {
        const double pitch = l ? left : right;
        const auto swing = bb::rotation({pitch,0,0});
        const auto elbow = bb::rotation({pitch-18,0,0});
        const double side = (l ? 1. : -1.)*(.115+.016*reach);
        poseArm(pose,l,swing.point({side,-1,0}),elbow.point({-side*.24,-1,0}),depression);
      }
    };
    auto legs = [&](double left, double right, double bend, double spread = 0) {
      set("upperleg01.L", {left, 0, spread});
      set("upperleg01.R", {right, 0, -spread});
      const auto leftKnee = bend + std::max(0., left * .52);
      const auto rightKnee = bend + std::max(0., right * .52);
      set("lowerleg01.L", {leftKnee, 0, 0});
      set("lowerleg01.R", {rightKnee, 0, 0});
      set("foot.L", {-left - leftKnee, 0, 0});
      set("foot.R", {-right - rightKnee, 0, 0});
    };
    auto groundedRest = [&](bool upright = false) {
      if (upright)
        pose.root[1] = standingHeight(pose,1.22,.028)-.008*(1-b);
      for (bool left : {true,false}) {
        const auto chain = leg(left);
        auto target = rig.joints[chain.ankle].bindPosition;
        target[0] = std::copysign(1.22,target[0]);
        placeFoot(pose,chain,target,0,left ? 5 : -5);
      }
    };
    arms(-2,1);
    hands(10);
    switch (clip.motion) {
      case Motion::Neutral:
        pose.root = {.055*a,-.055,0};
        if (useCapture) {
          // Keep the shoulders unloaded and let the wrists fall to either side
          // of the thighs. The weight shift happens above planted feet.
          pose.root = {.13+.036*a,-.075+.017*b,.024*a};
          for (bool left : {true,false}) {
            const double side = left ? 1. : -1.;
            // Drop the upper arm just behind the ribcage, bend gently at
            // the elbow and leave the wrist in line with the forearm.
            poseArm(pose,left,{side*.11,-1,-.104+.017*b},
                              {side*.075,-1,.184+.026*a},8.5,true);
          }
          hands(18);
        }
        groundedRest(useCapture);
        set("spine02", {a*.8, b*.8, a*.32-(useCapture ? 1.4 : 0.)});
        set("head", {b*.55, a*1.8, b*.65});
        break;
      case Motion::Relaxed:
        pose.root = {.24 + .12 * a, -.1, 0};
        arms(4 + a * 3, -3,0,useCapture ? 7. : 0.);
        groundedRest();
        set("spine04", {3, 0, -5 + 2 * b});
        set("head", {2, -8 + a * 5, 4});
        break;
      case Motion::Alert:
        pose.root = {0, -.25 + a * .04, .16};
        arms(-16, -22, 5);
        legs(-8, 5, 14);
        set("head", {-4, a * 22, 3 * b});
        break;
      case Motion::Walk:
      case Motion::Backward:
      case Motion::Run:
      case Motion::Left:
      case Motion::Right:
      case Motion::CrouchWalk: {
        const bool run = clip.motion == Motion::Run;
        if (const auto* gait = useCapture ? capturedGait(clip.motion) : nullptr) {
          const auto& track = gait->frames;
          const bool mirror = clip.motion==Motion::Left;
          const double frame = phase*(std::size(track)-1);
          const auto index = std::min(std::size(track)-2,std::size_t(frame));
          const auto fraction = frame-index;
          const auto& from = track[index];
          const auto& to = track[index+1];
          auto interpolate = [fraction,mirror](Vec3 x,Vec3 y) {
            auto result = add(mul(x,1-fraction),mul(y,fraction));
            if (mirror) result[0] = -result[0];
            return result;
          };
          pose.root = interpolate(from.root,to.root);
          pose.root[1] += gait->rootHeight;
          for (unsigned side=0; side<2; ++side) {
            const auto source = mirror ? 1-side : side;
            const auto chain = leg(side==0);
            auto target = interpolate(from.feet[source],to.feet[source]);
            const auto bind = rig.joints[chain.ankle].bindPosition;
            target[1] += bind[1];
            target[2] += bind[2];
            placeFoot(pose,chain,target,from.pitch[source]*(1-fraction)+to.pitch[source]*fraction,
                      side==0 ? 5 : -5);
            poseArm(pose,side==0,interpolate(from.upperArm[source],to.upperArm[source]),
                    interpolate(from.forearm[source],to.forearm[source]),run ? 5.5 : 0.,true);
          }
          hands(run ? 42 : 13);
          const double side = mirror ? -1. : 1.;
          set("spine03",{gait->spinePitch,side*2.3*a,side*.8*b});
          set("head",{-.55*gait->spinePitch,-side*1.3*a,-side*.4*b});
          break;
        }
        const bool crouch = clip.motion == Motion::CrouchWalk;
        const bool backward = clip.motion == Motion::Backward;
        const bool lateral = clip.motion == Motion::Left || clip.motion == Motion::Right;
        const double direction = backward || clip.motion==Motion::Right ? -1 : 1;
        const double stance = run ? .44 : crouch ? .68 : .62;
        const auto stride = run ? 6.3 : crouch ? 2.35 : backward ? 2.9 : lateral ? 2.2 : 4.25;
        const auto lift = run ? 2.12 : crouch ? .63 : .91;
        const auto bounce = std::cos(2*turn-.72);
        pose.root = {lateral ? -.16*direction*a : -.17*b,
                     (crouch ? -2.02 : run ? -.33 : -.27) -
                       (run ? .20 : .115)*bounce,
                     crouch ? -.12 : run ? .18 : .02};
        for (bool left : {true,false}) {
          const auto chain = leg(left);
          const auto cycle = std::fmod(phase+(left ? 0. : .5),1.);
          const auto swing = std::clamp((cycle-stance)/(1-stance),0.,1.);
          const auto stanceTime = std::min(cycle/stance,1.);
          const auto travel = .5-stanceTime+transition(swing);
          const auto clearance = lift*std::pow(std::sin(bb::pi*swing),1.5);
          auto target = rig.joints[chain.ankle].bindPosition;
          target[0] = std::copysign(lateral ? 1.85 : run ? 1.06 : 1.24,target[0]);
          target[lateral ? 0 : 2] += direction*stride*travel;
          target[1] += clearance;
          const auto push = transition((stanceTime-.78)/.22)*(1-transition(swing/.44));
          const auto heel = 1-transition(stanceTime/.12) + transition((swing-.73)/.27);
          const auto pitch = lateral ? 0 : direction*(push*(run ? 20 : 16)-heel*8);
          target[1] += std::max(0.,-pitch)*.016 + std::max(0.,pitch)*.008;
          placeFoot(pose,chain,target,pitch,left ? 5 : -5);
        }
        const auto swing = std::cos(turn-.21)*(run ? 32 : crouch ? 11 : lateral ? 7 : 19)*direction;
        arms(swing-(run ? 12 : 1),-swing-(run ? 12 : 1),run ? 4 : 0);
        if (run || crouch)
          for (bool left : {true,false}) {
            const double sign = left ? 1 : -1;
            const double armPitch = sign*swing-(run ? 10 : 5);
            const auto upper = bb::rotation({armPitch,0,0});
            const auto forearm = bb::rotation({armPitch-(run ? 78. : 42.),0,0});
            poseArm(pose,left,upper.point({sign*.14,-1,0}),
                    forearm.point({-sign*.10,-1,0}));
          }
        hands(run ? 42 : crouch ? 18 : 13);
        set("spine05",{0,-a*(run ? 6. : 3.8),-b*1.4});
        set("spine03",{run ? 9. : crouch ? 17. : 2.5,a*(run ? 8. : 5.),b*1.7});
        set("head",{run ? -5. : crouch ? -8. : -1.,-a*1.6,-b*1.1});
        break;
      }
      case Motion::TurnLeft:
      case Motion::TurnRight: {
        const double direction = clip.motion == Motion::TurnLeft ? 1 : -1;
        set("root", {0, smooth * 90 * direction, 0});
        legs(a * 15, -a * 15, pulse * 8);
        set("spine02", {0, pulse * 12 * direction, 0});
        set("head", {0, pulse * 18 * direction, 0});
        break;
      }
      case Motion::Crouch:
        arms(-12, -17);
        pose.root = {0, -1.72 + .05 * a, -.22};
        groundedRest();
        if (useCapture) {
          // In a low crouch the knees rise into the standing hand path.
          // Keep both wrists forward and the elbows gently bent for balance.
          for (bool left : {true,false}) {
            const auto side=left ? 1. : -1.;
            poseArm(pose,left,{side*.14,-.92,.31+.04*a},
                    {side*.09,-.20,.93},3.,true);
            poseFingers(pose,left,16.);
          }
        }
        set("spine04", {17 + a, 0, 0});
        break;
      case Motion::JumpStart:
      {
        const auto load = std::sin(bb::pi*transition(phase/.83));
        const auto launch = transition((phase-.64)/.36);
        pose.root = {0,-1.42*load+.26*launch,-.2*load};
        groundedRest();
        arms(34*load-76*launch,29*load-68*launch,10*launch);
        set("spine03",{16*load-3*launch,0,0});
        break;
      }
      case Motion::JumpLoop:
        legs(-23+2*a,-36-2*a,51+2*b,2);
        arms(-44+2*a,-36-3*a,10);
        set("spine03",{7,2*a,2});
        pose.root = {0,.30+.019*b,0};
        break;
      case Motion::JumpLand:
      {
        const auto absorb = transition(phase/.23)*(1-transition((phase-.24)/.76));
        pose.root = {0,-1.56*absorb+.16*(1-transition(phase/.2)),.23*absorb};
        groundedRest();
        arms(-46*absorb,-35*absorb,9*absorb);
        set("spine03",{24*absorb,-3*absorb,-2*absorb});
        break;
      }
      case Motion::SitDown:
      case Motion::Sit:
      case Motion::Stand: {
        const auto amount = clip.motion == Motion::Sit ? 1 : clip.motion == Motion::SitDown ? smooth : 1 - smooth;
        pose.root = {0,-3.61*amount,-1.46*amount};
        for (bool left : {true,false}) {
          const auto chain = leg(left);
          auto foot = rig.joints[chain.ankle].bindPosition;
          foot[0] = std::copysign(1.31,foot[0]);
          foot[2] += 1.6*amount;
          placeFoot(pose,chain,foot,0,left ? 7 : -7);
        }
        arms(-30 * amount, -28 * amount);
        set("spine04", {amount * (7 + (clip.motion == Motion::Sit ? 1.5 * a : 10 * pulse)), 0, 0});
        break;
      }
      case Motion::Wave:
      {
        const auto lift = transition(phase/.24)*(1-transition((phase-.75)/.25));
        const auto swing = std::sin((phase-.24)*11*bb::pi)*lift;
        groundedRest(useCapture);
        poseArm(pose,false,{-.115-.80*lift,-1+.74*lift,.42*lift},
                {.03-.19*lift-swing*.28,-.95+1.92*lift,.31-.09*lift},0,useCapture,lift);
        if (useCapture)
          poseArm(pose,true,{.13,-1,-.08},{.045,-1,.17},6.,true);
        pose.rotations[joints.at("wrist.R")] =
            pose.rotations[joints.at("wrist.R")]*bb::rotation({lift*8,swing*5,swing*11});
        poseFingers(pose,false,10-7*lift);
        set("spine02",{0,-4*lift,-3*lift});
        set("head", {0,-9*lift,3*lift});
        break;
      }
      case Motion::Point:
      {
        const auto reach = transition(phase/.36)*(1-transition((phase-.73)/.27));
        groundedRest();
        poseArm(pose,true,{.115+.12*reach,-1+.91*reach,reach},
                  {-.03-.07*reach,-.95+.92*reach,.31+.69*reach});
        poseFingers(pose,true,10+26*reach,reach);
        set("head", {2*reach,13*reach,0});
        break;
      }
      case Motion::Talk:
        groundedRest();
        arms(-12-11*a-6*std::sin(2*turn),-8+13*a,6);
        set("jaw", {2.5+1.4*std::sin(turn*5)+1.1*std::sin(turn*8),0,0});
        set("head", {2*b+1.5*std::sin(3*turn),5*a,1.3*b});
        break;
      case Motion::Melee:
        arms(-18 * pulse, -50 * pulse + 42 * a, 23 * pulse);
        set("spine03", {8 * pulse, 36 * a, 9 * pulse});
        legs(-11 * pulse, 13 * pulse, 13 * pulse);
        pose.root = {0, -.24 * pulse, .52 * pulse};
        break;
      case Motion::Aim:
        arms(-58 + 2 * a, -57 + 2 * a, 13);
        set("spine03", {4, -16, 0});
        set("head", {-4, 16, a});
        legs(-8, 9, 10);
        pose.root[1] = -.25;
        break;
      case Motion::HitFront:
      case Motion::HitBack: {
        const double direction = clip.motion == Motion::HitFront ? -1 : 1;
        set("spine04", {direction * pulse * 28, pulse * 12, pulse * 6});
        set("head", {direction * pulse * 14, 0, -pulse * 8});
        arms(-25 * pulse, -32 * pulse, pulse * 17);
        pose.root = {0, -.5 * pulse, .56 * direction * pulse};
        break;
      }
      case Motion::DeathFront:
      case Motion::DeathBack: {
        const double direction = clip.motion == Motion::DeathFront ? -1 : 1;
        const auto buckle = transition(phase/.37)*(1-transition((phase-.46)/.38));
        const auto fall = transition((phase-.21)/.67);
        const auto brace = transition((phase-.14)/.31)*(1-transition((phase-.58)/.40));
        set("root", {direction*fall*89,fall*9,fall*5});
        legs(-42*buckle-4*fall,-32*buckle-11*fall,62*buckle+8*fall);
        arms(-brace*(direction>0 ? 72 : 28)-fall*10,-brace*41-fall*8,brace*24+fall*9);
        set("spine03",{direction*buckle*13,fall*7,brace*4});
        set("head",{-direction*buckle*15,fall*9,brace*3});
        pose.root = {.28*fall,-1.8*buckle-fall*7.86,1.65*direction*fall};
        break;
      }
    }
    return pose;
  }
};
inline AnimationPose sampleCharacterClip(const Animator& animator,const Clip& clip,double seconds,
                                         const ImportedMotion* imported) {
  if (clip.motion!=Motion::Imported) return animator.sample(clip,seconds);
  if (!imported) throw std::runtime_error("The character's imported animation is missing.");
  return sampleImportedMotion(*imported,seconds);
}
} // namespace ftebroom::character
