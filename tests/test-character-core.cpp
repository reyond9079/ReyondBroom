// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/character/glb.h"
#include "../native/character/iqm.h"
#include "../native/character/studio.h"
#include "../native/character/presets.h"
#include "../native/character/preview.h"
#include "../native/character/animation_import.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <filesystem>

using namespace ftebroom::character;

static void check(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

template<class Function>
static void rejects(Function action, const char* message) {
  try {
    action();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(message);
}

static Vec3 skinReference(const Humanoid& rig, const AnimationPose& pose, const Vertex& vertex) {
  Vec3 result{};
  for (std::size_t i=0; i<vertex.weights.size(); ++i) {
    if (vertex.weights[i] == 0) continue;
    auto point = vertex.position;
    // Apply each ancestor directly around its bind pivot. The production
    // implementation instead composes joint and inverse-bind matrices first.
    for (int joint=vertex.joints[i]; joint>=0; joint=rig.joints[joint].parent) {
      const auto& pivot = rig.joints[joint].bindPosition;
      const auto& rotation = pose.rotations[joint].m;
      const auto input = sub(point,pivot);
      for (int axis=0; axis<3; ++axis)
        point[axis] = pivot[axis] + rotation[axis][0]*input[0] +
            rotation[axis][1]*input[1] + rotation[axis][2]*input[2];
    }
    for (int axis=0; axis<3; ++axis)
      result[axis] += (point[axis]+pose.root[axis])*vertex.weights[i];
  }
  return result;
}

static void exerciseSkinnedPoses(const Humanoid& fitted) {
  const Animator animator(fitted);
  double maxError = 0;
  for (const auto& vertex : fitted.vertices) {
    double sum = 0;
    for (std::size_t i=0; i<vertex.weights.size(); ++i) {
      const auto weight = vertex.weights[i];
      check(std::isfinite(weight) && weight >= 0 &&
            (weight == 0 || vertex.joints[i] < fitted.joints.size()),
            "Fitted mesh has an invalid joint weight");
      sum += weight;
    }
    check(std::abs(sum-1) <= 1e-4,"Fitted joint weights are not normalized");
  }
  for (const auto& clip : clips) {
    for (const double phase : {0.,.25,.5,.75,1.}) {
      const auto pose = animator.sample(clip,phase*clip.seconds);
      const auto matrices = skinMatrices(fitted,pose.rotations,pose.root);
      for (const auto& vertex : fitted.vertices) {
        const auto actual = deform(vertex,vertex.position,matrices);
        const auto expected = skinReference(fitted,pose,vertex);
        const auto error = ftebroom::bb::length(sub(actual,expected));
        if (!std::isfinite(error))
          throw std::runtime_error("Nonfinite skinned vertex in "+std::string(clip.id));
        maxError = std::max(maxError,error);
      }
    }
  }
  check(maxError < 1e-4,"Fitted positions differ from the independent skinning reference");
  std::cout << "PASS every fitted body, garment and hair vertex against independent skinning across "
            << std::size(clips) << " clips, max error " << maxError << " authoring units\n";
}

static std::string motionFixture(const Humanoid& rig,bool wrongOrder=false) {
  GlbBuffer buffer;
  Json nodes=Json::array(), joints=Json::array();
  for (const auto& bone : rig.joints) {
    const auto index=nodes.size();
    if (bone.parent>=0) nodes.at(bone.parent)["children"].push_back(index);
    nodes.push_back({{"name",bone.name},{"translation",bone.parent<0 ? mul(bone.bindPosition,.085) :
        mul(sub(bone.bindPosition,rig.joints[bone.parent].bindPosition),.085)}});
    joints.push_back(index);
  }
  if (wrongOrder) std::swap(joints[1],joints[2]);
  const auto wrist=socketJoint(rig.joints,gearSockets[0]);
  nodes.at(wrist)["rotation"]={0,0,std::sin(.25),std::cos(.25)};
  const auto input=buffer.add(std::vector<float>{0,1},1,"SCALAR");
  const auto keys=buffer.add(std::vector<float>{0,0,float(std::sin(.25)),float(std::cos(.25)),
                                               0,0,float(std::sin(.62)),float(std::cos(.62))},4,"VEC4");
  Json animation{{"samplers",Json::array({{{"input",input},{"output",keys}}})},
                 {"channels",Json::array({{{"sampler",0},{"target",{{"node",wrist},{"path","rotation"}}}}})}};
  return buffer.finish({{"asset",{{"version","2.0"}}},{"nodes",nodes},
                         {"skins",Json::array({{{"joints",joints}}})},{"animations",Json::array({animation})}});
}

template<class Parse>
static void exerciseParserMutations(std::string_view original,std::mt19937& random,Parse parse) {
  for (unsigned trial=0;trial<192;++trial) {
    std::string input(original);
    const auto offset=random()%input.size();
    switch (trial%3) {
      case 0: input.erase(offset,1+random()%31); break;
      case 1: input.insert(offset,size_t(1+random()%8),char(random()%256)); break;
      case 2: input[offset]=char(random()%256); break;
    }
    // Most changed inputs should fail. Some remain legal, for example a
    // changed ASCII digit in a JSON property or an unused GLB chunk byte.
    try { parse(input); }
    catch (const std::runtime_error&) {}
    catch (const Json::exception&) {}
  }
}

int main(int argc, char** argv) try {
  check(argc >= 2 && argc <= 5, "Expected humanoid asset and optional GLB, IQM and legacy fixture outputs");
  std::ifstream file(argv[1], std::ios::binary);
  check(file.good(), "Unable to open the packaged humanoid");
  const std::string bytes((std::istreambuf_iterator<char>(file)), {});
  auto rig = readHumanoid(bytes);
  const auto assets = std::filesystem::path(argv[1]).parent_path();
  std::ifstream shapeAsset(assets/"adult-face-shapes.rbf",std::ios::binary);
  const std::string shapes((std::istreambuf_iterator<char>(shapeAsset)),{});
  readFaceShapes(rig,shapes);
  rejects([&] { readFaceShapes(rig,std::string_view(shapes).substr(0,100)); },
          "Accepted a truncated face shape asset");
  std::ifstream bodyAsset(assets/"adult-body-shapes.rbs",std::ios::binary);
  const std::string bodyShapes((std::istreambuf_iterator<char>(bodyAsset)),{});
  readBodyShapes(rig,bodyShapes);
  rejects([&] { readBodyShapes(rig,std::string_view(bodyShapes).substr(0,100)); },
          "Accepted a truncated body shape asset");
  check(rig.joints.size() == 79, "Unexpected canonical joint count");
  check(rig.rigHash == "cde0140324705662f4d60659adaf5966d74c8eb40506b7770403c6037a2be10b",
        "Unexpected rig digest");
  {
    const auto bytes=motionFixture(rig);
    const auto motion=MotionGlb(bytes).retarget(rig);
    const auto wrist=socketJoint(rig.joints,gearSockets[0]);
    const auto rest=sampleImportedMotion(motion,0).rotations[wrist].vector({1,0,0});
    const auto end=sampleImportedMotion(motion,1).rotations[wrist].vector({1,0,0});
    check(std::abs(rest[1])<1e-5 && std::abs(end[1]-std::sin(.74))<1e-5,
          "The import must remove the source rest rotation and preserve the motion");
    rejects([&] { auto bad=motionFixture(rig,true); MotionGlb(bad).retarget(rig); },
            "Accepted mismatching source rig order");
    rejects([&] { MotionGlb(std::string_view(bytes).substr(0,100)); },
            "Accepted a truncated GLB clip");
    Document doc;
    doc.animationAsset=std::string(64,'b');
    doc.clip=importedClipId;
    doc.customWardrobe=std::string(64,'c');
    const auto loaded=readDocument(writeDocument(doc,rig).dump(),rig);
    check(loaded.animationAsset==doc.animationAsset && loaded.clip==doc.clip &&
          loaded.customWardrobe==doc.customWardrobe, "Imported asset references did not round trip");
    auto bad=writeDocument(loaded,rig);
    bad["customWardrobe"]="../outside";
    rejects([&] { decodeDocument(bad,rig); }, "Accepted a wardrobe path instead of a digest");
  }
  const auto clothingDir = std::filesystem::path(argv[1]).parent_path() / "wardrobe";
  std::map<std::string,Wardrobe> wardrobe;
  for (const auto& file : std::filesystem::directory_iterator(clothingDir)) {
    if (file.path().extension() != ".rbw") continue;
    std::ifstream asset(file.path(),std::ios::binary);
    check(asset.good(),"Cannot read wardrobe asset");
    const std::string input((std::istreambuf_iterator<char>(asset)), {});
    wardrobe.emplace(file.path().stem().string(),readWardrobe(input,rig,file.path().stem().string()));
  }
  check(wardrobe.size() == 32,"Bundled wardrobe has missing entries");
  auto lookup = [&](const auto& id) -> const auto& { return wardrobe.at(id); };
  {
    check(std::none_of(std::begin(presets),std::end(presets),[](const auto& preset) {
      return std::string_view(preset.name)=="Field Technician";
    }),"The retired character must be absent from the preset list");
    check(std::size(faceTemplateNames)==4 && fieldTechnicianFaceTemplate==4 &&
          std::string_view(faceTemplateLabel(4))=="Legacy saved template",
          "The legacy face ID must remain loadable outside the face template list");
    Document savedWorkwear;
    savedWorkwear.name = "Field Technician";
    savedWorkwear.outfit = 12;
    savedWorkwear.hairstyle = 17;
    savedWorkwear.headwear = 1;
    savedWorkwear.faceTemplate = fieldTechnicianFaceTemplate;
    savedWorkwear.faceProfile = .73;
    savedWorkwear.bodyProfile = .9855;
    for (const auto& [morph,amount] : std::initializer_list<std::pair<MorphIndex,double>>{
        {Height,.32},{BodyFat,-.48},{Muscle,.08},{ShoulderWidth,.54},{NeckWidth,.14},
        {BodyBulk,-.21},{ChestDepth,-.39},{WaistWidth,.07},{BellyDepth,-1.},{HipWidth,-.84},
        {UpperArm,.46},{Forearm,.13}}) savedWorkwear.values[morph] = amount;
    auto legacy = writeDocument(savedWorkwear,rig);
    legacy["faceTemplate"] = 4;
    const auto reference = readDocument(legacy.dump(),rig);
    check(reference.faceTemplate==fieldTechnicianFaceTemplate &&
          writeDocument(reference,rig).at("faceTemplate")==4 &&
          surfaceMaps(reference,true)[3]=="charcoal-utility" &&
          surfaceMaps(reference,true)[5]=="tousled-fringe",
          "A saved workwear character must retain its face and clothing");
    const auto& oldUniform = lookup("male_casualsuit03");
    const auto& uniform = lookup("male_utility_field");
    const auto waistband = std::find_if(oldUniform.vertices.begin(),oldUniform.vertices.end(),
        [](const auto& entry) { return entry.vertex.source==2260; });
    check(waistband!=oldUniform.vertices.end(),"Missing front waistband vertex");
    const auto i = std::size_t(waistband-oldUniform.vertices.begin());
    const auto rise = uniform.vertices.at(i).vertex.position[1]-waistband->vertex.position[1];
    check(rise>.12 && rise<.28 && uniform.triangles.size()>oldUniform.triangles.size()+40,
          "The workwear mesh must include the fitted waistband and folded collar faces");
    const auto edgesOf = [](const Wardrobe& item) {
      std::map<std::pair<unsigned,unsigned>,unsigned> edges;
      for (const auto& face : item.triangles)
        for (unsigned i=0;i<face.size();++i) {
          const auto a = item.vertices[face[i]].vertex.source;
          const auto b = item.vertices[face[(i+1)%face.size()]].vertex.source;
          ++edges[std::minmax(a,b)];
        }
      return edges;
    };
    std::map<unsigned,double> heights;
    for (const auto& entry : oldUniform.vertices)
      heights[entry.vertex.source] = entry.vertex.position[1];
    const auto uniformEdges = edgesOf(uniform);
    std::set<unsigned> seamSources;
    unsigned sewn = 0;
    for (const auto& [edge, count] : edgesOf(oldUniform)) {
      if (count!=1 || std::min(heights.at(edge.first),heights.at(edge.second))<13.52) continue;
      check(uniformEdges.at(edge)==2,"The collar must share the shirt's neckline edges");
      seamSources.insert(edge.first);
      seamSources.insert(edge.second);
      ++sewn;
    }
    check(sewn>=20,"The collar seam must run around the neck");
    std::map<unsigned,const Wardrobe::Entry*> seamEntries;
    for (const auto& entry : uniform.vertices) {
      if (!seamSources.count(entry.vertex.source)) continue;
      const auto [match,first] = seamEntries.emplace(entry.vertex.source,&entry);
      if (first) continue;
      check(entry.vertex.position==match->second->vertex.position &&
            entry.anchors==match->second->anchors && entry.factors==match->second->factors,
            "The collar's UV seam must keep the shirt's position and skinning anchors");
    }
    check(bodyScale(reference)>1.04,"The saved workwear character must retain its proportion settings");
    std::array<unsigned,4> beltQuadrants{};
    double beltLow = 100., beltHigh = 0.;
    for (const auto& entry : uniform.vertices) {
      const auto& vertex = entry.vertex;
      if (std::abs(vertex.uv[1]-.987)>.0001 || vertex.uv[0]<.845) continue;
      const auto& p = vertex.position;
      ++beltQuadrants[(p[0]>0 ? 1u : 0u)+(p[2]>.12 ? 2u : 0u)];
      beltLow = std::min(beltLow,p[1]);
      beltHigh = std::max(beltHigh,p[1]);
    }
    check(*std::min_element(beltQuadrants.begin(),beltQuadrants.end())>=15 &&
          beltHigh-beltLow<.22,
          "The belt should wrap through the front, sides and back at a steady height");
    const auto trouserSection = [&](double height) {
      double inside = 100., outside = 0.;
      for (const auto& face : uniform.triangles) {
        for (unsigned i=0;i<face.size();++i) {
          const auto& a = uniform.vertices[face[i]].vertex.position;
          const auto& b = uniform.vertices[face[(i+1)%face.size()]].vertex.position;
          if ((a[1]<height)==(b[1]<height)) continue;
          const auto x = std::abs(a[0]+(b[0]-a[0])*(height-a[1])/(b[1]-a[1]));
          inside = std::min(inside,x);
          outside = std::max(outside,x);
        }
      }
      return std::array<double,2>{inside,outside};
    };
    const auto [seam, thighWidth] = trouserSection(6.35);
    const auto upperHipWidth = trouserSection(8.55)[1];
    check(upperHipWidth >= .91*thighWidth && upperHipWidth < thighWidth,
          "The work trousers should taper gradually from the thighs to the waistband");
    check(seam>.11 && seam<.34,
          "The inseam should narrow the gap without joining the legs below the rise");
    for (const auto& sample : {applyPreset({},presets[0]),applyPreset({},presets[1]),
                               applyPreset({},presets[2]),applyPreset({},presets[10]),reference}) {
      const bool workwear = sample.outfit==reference.outfit;
      const auto fitted = assemble(rig,sample,lookup,true);
      const Animator mover(fitted,true);
      auto joint = [&](const std::string& name) {
        const auto it = std::find_if(fitted.joints.begin(),fitted.joints.end(),
            [&](const auto& value) { return value.name==name; });
        check(it!=fitted.joints.end(),"Missing joint: "+name);
        return std::size_t(it-fitted.joints.begin());
      };
      auto point = [&](const auto& skin,std::size_t id) {
        return skin[id].point(fitted.joints[id].bindPosition);
      };
      const auto rest = mover.sample(clips[0],0);
      const auto idle = skinMatrices(fitted,rest.rotations,rest.root);
      const auto spine = joint("spine01");
      const auto neutralTorso = ftebroom::bb::inverse(idle[spine]);
      const auto run = captureTiming(clips[7],true);
      if (workwear) {
        for (const auto side : {std::string(".L"),std::string(".R")}) {
          const auto wrist = joint("wrist"+side);
          const auto shoulder = joint("upperarm01"+side), elbow = joint("lowerarm01"+side);
          const auto armLength = [&](std::size_t a,std::size_t b) {
            return ftebroom::bb::length(sub(fitted.joints[a].bindPosition,fitted.joints[b].bindPosition));
          };
          const auto ratio = armLength(shoulder,elbow)/armLength(elbow,wrist);
          check(ratio>1.07 && ratio<1.21,"Workwear arm proportions must be retargeted around the elbow");
          const auto distal = joint("finger3-3"+side);
          const auto thumb = joint("finger1-1"+side);
          const auto walking = mover.sample(captureTiming(clips[3],true),.2);
          const auto running = mover.sample(run,.2);
          const Vec3 local{0,1,0};
          check(dot(running.rotations[distal].vector(local), walking.rotations[distal].vector(local))<.998,
                "The distal finger should flex more in a running grip than in a walking hand");
          check(dot(running.rotations[thumb].vector(local), walking.rotations[thumb].vector(local))<.998,
                "The thumb should follow the running grip");
          for (const auto& source : clips) {
            const auto clip = captureTiming(source,true);
            Vec3 previous;
            for (unsigned frame=0; frame<=120; ++frame) {
              const auto pose = mover.sample(clip,clip.seconds*frame/120.);
              const auto skin = skinMatrices(fitted,pose.rotations,pose.root);
              const auto normal = skin[wrist].vector(local);
              if (frame) check(dot(previous,normal)>.94,"Wrist snaps during "+std::string(clip.id));
              previous = normal;
            }
          }
        }
      }
      for (const auto side : {std::string(".L"),std::string(".R")}) {
        const auto direction = side==".L" ? 1. : -1.;
        const auto shoulder = joint("upperarm01"+side);
        check(neutralTorso.point(point(idle,shoulder))[1] <
              fitted.joints[shoulder].bindPosition[1]-.085,
              "Neutral shoulders should sit below the raised bind pose");
        const auto elbow = joint("lowerarm01"+side), wrist = joint("wrist"+side);
        const auto digits = {joint("finger2-1"+side),joint("finger3-2"+side),joint("finger5-2"+side)};
        std::set<unsigned> cuff;
        if (workwear)
          for (const auto& face : fitted.triangles) {
            if (face.material!=3) continue;
            for (const auto id : face.indices) {
              const auto& vertex = fitted.vertices[id];
              double weight = 0;
              for (unsigned j=0; j<vertex.weights.size(); ++j)
                if (vertex.joints[j]==elbow || vertex.joints[j]==wrist)
                  weight += vertex.weights[j];
              if (weight>=.8) cuff.insert(id);
            }
          }
        if (workwear)
          check(!cuff.empty(),"The fitted utility uniform should have skinned sleeves");
        for (unsigned frame=0; frame<120; ++frame) {
          const auto pose = mover.sample(run,run.seconds*frame/120.);
          const auto skin = skinMatrices(fitted,pose.rotations,pose.root);
          const auto torso = ftebroom::bb::inverse(skin[spine]);
          const auto a = torso.point(point(skin,elbow));
          const auto b = torso.point(point(skin,wrist));
          auto clear = [&](Vec3 p,double radius) {
            const auto limit = fitted.torso.armClearance(p,radius);
            check(limit<=0 || direction*p[0]>=limit-.03,"Run arm or hand crosses the fitted torso");
          };
          for (unsigned step=0; step<=24; ++step)
            clear(add(a,mul(sub(b,a),step/24.)),.24);
          for (const auto digit : digits) clear(torso.point(point(skin,digit)),.09);
          for (const auto id : cuff) {
            const auto& vertex = fitted.vertices[id];
            clear(torso.point(deform(vertex,vertex.position,skin)),0.);
          }
        }
      }
    }
  }
  {
    auto feminine = applyPreset({},presets[1]);
    auto masculine = feminine;
    feminine.bodyProfile = -1.;
    masculine.bodyProfile = 1.;
    auto previous = writeDocument(feminine,rig);
    previous.erase("bodyProfile");
    check(decodeDocument(previous,rig).bodyProfile==0. &&
          readDocument(writeDocument(feminine,rig).dump(),rig).bodyProfile==-1.,
          "Body shape must round-trip and previous documents must keep their geometry");
    const auto slim = assemble(rig,feminine,lookup,true);
    const auto broad = assemble(rig,masculine,lookup,true);
    const auto original = assemble(rig,feminine,lookup);
    const auto legacy = assemble(rig,masculine,lookup);
    check(slim.vertices.size()==broad.vertices.size() && original.vertices.size()==legacy.vertices.size(),
          "Body profile must preserve clothing and visible skin topology");
    const auto joint = [&](std::string_view name) {
      return std::size_t(std::find_if(rig.joints.begin(),rig.joints.end(),
          [&](const auto& value){ return value.name==name; })-rig.joints.begin());
    };
    const auto shoulder = joint("upperarm01.L");
    const auto head = joint("head");
    check(broad.joints.at(shoulder).bindPosition[0]>slim.joints.at(shoulder).bindPosition[0]+.22 &&
          broad.joints.at(head).bindPosition==slim.joints.at(head).bindPosition &&
          original.joints.at(shoulder).bindPosition==legacy.joints.at(shoulder).bindPosition,
          "Torso and limbs should follow the anatomical profile without moving the face or changing legacy exports");
    double fit=0.;
    for (std::size_t i=0; i<slim.vertices.size(); ++i) {
      check(original.vertices[i].position==legacy.vertices[i].position,
            "The anatomical profile should remain local to the FTE fit");
      if (slim.vertices[i].source>=65536)
        fit+=ftebroom::bb::length(sub(slim.vertices[i].position,broad.vertices[i].position));
    }
    check(fit>60., "Garment geometry did not follow the body profile");
    const auto colors = surfaceMaps(masculine,true);
    check(colors[3]=="hd-female_casualsuit02" && colors[4]=="hd-shoes04" && colors[5]=="hd-bob02" &&
          colors[1]=="eyes-pbr-blue","FTE must select wardrobe and iris atlases at full resolution");
  }
  {
    // Sample the fitted outer scalp in both directions on each horizontal axis
    // and from above. The same radial constraint is used for all hair assets.
    const auto doc = applyPreset({},presets[3]);
    const auto points = morphBody(rig,doc);
    const auto crown = fittedScalp(rig,points);
    for (const auto direction : {Vec3{0,1,0},Vec3{1,.38,0},Vec3{-1,.38,0},
                                 Vec3{0,.58,-1},Vec3{0,.80,1}}) {
      const auto axis = unit(direction);
      const auto scalp = crown.farthest(crown.center,axis,0,std::numeric_limits<double>::infinity());
      check(scalp>.3,"Missing scalp surface along a supported view direction");
      const auto hair = clearScalp(crown,add(crown.center,mul(axis,scalp-.075)),.05);
      check(dot(sub(hair,crown.center),axis)>=scalp+.049,"Fitted hair intersects scalp");
    }
    const auto sidePart = applyPreset({},presets[0]);
    check(hairRootCoverage({.74,15.91,.55},sidePart,scalpHairEdge(hairstyles[sidePart.hairstyle]))>.8 &&
          hairRootCoverage({.74,15.32,.55},sidePart,scalpHairEdge(hairstyles[sidePart.hairstyle]))==0,
          "Side part should cover the temples above the ear");
    const auto fitted = assemble(rig,doc,lookup);
    for (unsigned i=0; i<rig.joints.size(); ++i) {
      const auto& base = rig.joints[i];
      const auto& joint = fitted.joints[i];
      check(base.name==joint.name && base.parent==joint.parent &&
            ftebroom::bb::length(joint.inverseBind.point(joint.bindPosition))<1e-8,
            "Proportion adjustment must preserve the rig contract and inverse bind");
    }
    check(fitted.joints.front().bindPosition[1]>rig.joints.front().bindPosition[1]+.38,
          "Adult rest proportions did not rebalance the pelvis");
  }
  {
    check(lipCoverage({.075,15.02,1.54})>.95 &&
          lipCoverage({.31,15.02,1.54})==0 && lipCoverage({.075,15.16,1.54})==0,
          "Lip shading must stay within the mouth contour");
    auto shaped = applyPreset({},presets[3]);
    shaped.values[EyeHeight] = -.70;
    shaped.values[EyeDepth] = .65;
    const auto fitted = assemble(rig,shaped,lookup);
    std::set<unsigned> lashVertices;
    for (const auto& face : fitted.triangles) {
      for (const auto id : face.indices) {
        if (face.material==lashMaterial) lashVertices.insert(id);
        if (face.material==lashMaterial)
          check(fitted.vertices[id].lipMask==0,"The lip mask leaked onto eyelashes");
      }
    }
    check(lashVertices.size()==lookup("eyelashes01").vertices.size(),
          "Fitting must retain all eyelash cards");
    const auto neutral = assemble(rig,applyPreset({},presets[3]),lookup);
    check(hairRootCoverage({0,15.51,1.48},shaped,16.06)==0 &&
          hairRootCoverage({0,16.38,1.25},shaped,16.06)>.95 &&
          hairRootCoverage({0,16.38,1.25},shaped,0)==0,
          "Hair root pigment must feather across the hairline and leave shaved heads clear");
    for (const auto& vertex : fitted.vertices) {
      check(vertex.lipMask*vertex.scalpMask==0,"Skin material masks must not overlap");
      const auto mask = skinVertexMask(vertex);
      check(mask>=0 && mask<=1,"Skin material mask must fit in normalized IQM alpha");
    }
    double lashMotion = 0;
    for (const auto id : lashVertices) {
      const auto motion = sub(fitted.vertices[id].position,neutral.vertices[id].position);
      lashMotion += std::abs(motion[1])+std::abs(motion[2]);
    }
    check(lashMotion>lashVertices.size()*.025,"Eyelashes did not follow the fitted eyelids");
  }
  {
    for (unsigned style=0; style+1<eyebrowCount; ++style) {
      const auto doc = applyPreset({},presets[3]);
      auto shaped = doc;
      shaped.eyebrow = style;
      shaped.faceAsymmetry = 1;
      shaped.values[BrowHeight] = -.7;
      const auto fitted = assemble(rig,shaped,lookup);
      std::array<unsigned,2> faceCount{};
      std::array<std::multiset<Vec2>,2> coordinates;
      for (const auto& face : fitted.triangles) {
        if (face.material!=browMaterial) continue;
        const unsigned side = fitted.vertices[face.indices[0]].position[0]<0;
        ++faceCount[side];
        for (auto id : face.indices) {
          const auto& vertex = fitted.vertices[id];
          check((vertex.position[0]<0)==bool(side),"Eyebrow crossed the bridge of the nose");
          coordinates[side].insert(vertex.uv);
        }
      }
      check(faceCount[0]>0 && faceCount[0]*2==lookup(eyebrowMeshes[style]).triangles.size() &&
            faceCount[0]==faceCount[1] && coordinates[0]==coordinates[1],
            "Eyebrows must keep the complete, mirrored strand pattern");
    }
  }
  {
    Document neutral, feminine, masculine;
    feminine.faceTemplate = masculine.faceTemplate = 2;
    feminine.faceProfile = -1;
    masculine.faceProfile = 1;
    const auto first = morphBody(rig,feminine), last = morphBody(rig,masculine);
    auto templateVariation = feminine;
    templateVariation.faceTemplate = 3;
    const auto alternative = morphBody(rig,templateVariation);
    const auto base = morphBody(rig,neutral);
    double structure = 0, variation = 0;
    for (unsigned i=0; i<rig.vertices.size(); ++i) {
      if (rig.vertices[i].position[1]<=13.75)
        check(first[i]==base[i] && last[i]==base[i],"Face template moved the body");
      structure += dot(sub(first[i],last[i]),sub(first[i],last[i]));
      variation += dot(sub(first[i],alternative[i]),sub(first[i],alternative[i]));
    }
    check(structure>10 && variation>10,"Face templates must change actual head geometry");
    const auto a = assemble(rig,feminine,lookup), b = assemble(rig,masculine,lookup);
    check(a.vertices.size()==b.vertices.size(),"A face shape changed its vertex layout");
    double hairMotion = 0;
    for (const auto& face : a.triangles) if (face.material==5)
      for (auto id : face.indices) hairMotion += ftebroom::bb::length(sub(a.vertices[id].position,b.vertices[id].position));
    check(hairMotion>1,"Hair failed to follow the face template");
    feminine.eyebrow = 1;
    feminine.hairstyle = 11;
    feminine.hairline = -.35;
    feminine.hairVolume = .65;
    feminine.hairLength = -.45;
    feminine.values[EyeHeight] = .25;
    feminine.faceAsymmetry = -.73;
    auto roundTrip = writeDocument(feminine,rig);
    const auto restored = readDocument(roundTrip.dump(),rig);
    check(restored.faceTemplate==2 && restored.faceProfile==-1 && restored.eyebrow==1 &&
          restored.hairstyle==11 && restored.hairline==-.35 && restored.hairVolume==.65 &&
          restored.hairLength==-.45 && restored.values[EyeHeight]==.25 &&
          restored.faceAsymmetry==-.73,
          "Appearance settings did not round trip");
    roundTrip.erase("faceProfile"); roundTrip.erase("faceTemplate"); roundTrip.erase("eyebrow");
    roundTrip.erase("hairline"); roundTrip.erase("hairVolume"); roundTrip.erase("hairLength");
    roundTrip.erase("faceAsymmetry");
    for (auto i=legacyMorphCount; i<morphCount; ++i) roundTrip["morphs"].erase(morphs[i].id);
    const auto old = readDocument(roundTrip.dump(),rig);
    check(old.faceProfile==0 && old.faceTemplate==0 && old.eyebrow==0 &&
          old.hairline==0 && old.hairVolume==0 && old.hairLength==0 &&
          old.values[EyeHeight]==0 && old.faceAsymmetry==0,"Legacy face migrated incorrectly");
    const auto untuned = assemble(rig,old,lookup);
    auto tuned = old;
    tuned.hairline = .8;
    tuned.hairVolume = .8;
    const auto adjusted = assemble(rig,tuned,lookup);
    std::set<unsigned> hairVertices;
    for (const auto& face : untuned.triangles)
      if (face.material==5) hairVertices.insert(face.indices.begin(),face.indices.end());
    double motion = 0;
    for (unsigned i=0; i<untuned.vertices.size(); ++i) {
      const auto delta = sub(untuned.vertices[i].position,adjusted.vertices[i].position);
      if (hairVertices.count(i)) motion += dot(delta,delta);
      else check(dot(delta,delta)==0,"Hair controls changed a non-hair vertex");
    }
    check(motion>.5,"Hair controls do not change the fitted mesh");
    auto asymmetric = old;
    asymmetric.faceAsymmetry = 1;
    const Vec3 leftEye{.31,15.47,1.40}, rightEye{-.31,15.47,1.40};
    const auto left = sub(morphPoint(asymmetric,leftEye),morphPoint(old,leftEye));
    const auto right = sub(morphPoint(asymmetric,rightEye),morphPoint(old,rightEye));
    check(left[1]>.001 && right[1]<-.001,"Asymmetry must still move each eye independently");
    auto inverted = asymmetric;
    inverted.faceAsymmetry = -1;
    double maxDisplacement = 0;
    for (const auto& vertex : rig.vertices) {
      const auto& position = vertex.position;
      const auto baseline = morphPoint(old,position);
      const auto positive = sub(morphPoint(asymmetric,position),baseline);
      const auto negative = sub(morphPoint(inverted,position),baseline);
      maxDisplacement = std::max(maxDisplacement,std::abs(positive[1]));
      check(std::abs(positive[1]+negative[1])<1e-10,
            "Asymmetry slider must reverse direction without changing strength");
    }
    check(maxDisplacement<.006,"Maximum facial asymmetry exceeded 0.6 mm");
  }
  {
    std::set<std::string> names;
    std::set<unsigned> hairstyles;
    std::set<unsigned> complexions;
    std::set<double> heights;
    std::size_t minTriangles = std::numeric_limits<std::size_t>::max(), maxTriangles = 0;
    for (const auto& preset : presets) {
      const auto doc = applyPreset({},preset);
      check(names.insert(doc.name).second,"Duplicate preset name");
      check(std::abs(doc.faceAsymmetry)<=.16,"Preset facial asymmetry must be subtle");
      hairstyles.insert(doc.hairstyle);
      complexions.insert(doc.complexion);
      heights.insert(doc.values[Height]);
      const auto restored = readDocument(writeDocument(doc,rig).dump(),rig);
      check(restored.values==doc.values && restored.name==doc.name &&
            restored.complexion==doc.complexion && restored.beard==doc.beard,"Preset did not round trip");
      const auto count = assemble(rig,restored,lookup).triangles.size();
      check(count>0,"Preset could not be fitted");
      minTriangles = std::min(minTriangles,count);
      maxTriangles = std::max(maxTriangles,count);
    }
    check(names.size()>=32 && hairstyles.size()>=15 && complexions.size()==complexionCount-1 &&
          heights.size()>=10,"Preset catalog lacks variety");
    check(lookup("short02").triangles.size()==3022 && lookup("short04").triangles.size()==1002,
          "Transparent hair faces were not removed");
    std::cout << "PASS preset preview triangle counts " << minTriangles << "-" << maxTriangles << '\n';
  }
  std::set<std::pair<std::size_t,std::size_t>> outfitsSeen;
  for (unsigned outfit = 0; outfit < outfitCount; ++outfit) {
    Document a;
    a.outfit = outfit;
    const auto defaultFit = assemble(rig,a,lookup);
    if (outfit==0 || outfit==4 || outfit==9) {
      for (const auto budget : {15000u,9000u}) {
        const auto lod=previewLod(defaultFit,budget);
        check(lod.triangles.size()<=budget && lod.triangles.size()>1000 &&
              lod.rigHash==rig.rigHash && lod.joints.size()==rig.joints.size(),
              "Preview detail must reduce triangles and retain the rig");
        for (const auto& socket : gearSockets) socketJoint(lod.joints,socket);
      }
    }
    const auto total = defaultFit.triangles.size();
    const auto garments = std::count_if(defaultFit.triangles.begin(),defaultFit.triangles.end(),
                                        [](const auto& face) { return face.material >= 3; });
    check(garments > 500 && outfitsSeen.emplace(total,garments).second,"Empty or duplicate outfit");
    for (unsigned seed : {5u,107u}) {
      randomize(a,seed);
      const auto fit = assemble(rig,a,lookup);
      check(fit.triangles.size() == total,"Body parameters changed wardrobe topology");
      for (const auto& v : fit.vertices)
        check(std::isfinite(v.position[0]) && std::isfinite(v.position[1]) && std::isfinite(v.position[2]),
              "Non-finite garment deformation");
    }
  }
  auto pose = std::vector<Mat4>(rig.joints.size(), Mat4::identity());
  const auto rest = skinMatrices(rig, pose);
  for (const auto& vertex : rig.vertices)
    check(ftebroom::bb::length(sub(deform(vertex, vertex.position, rest), vertex.position)) < 2e-6,
          "Rest deformation moved a vertex");
  const auto root = rig.joints.front().bindPosition;
  const Vec3 offset{1, 2, 3};
  pose.front() = ftebroom::bb::rotation({0, 90, 0});
  const auto turned = skinMatrices(rig, pose, offset);
  for (const auto& vertex : rig.vertices) {
    const auto point = sub(vertex.position, root);
    const auto expected = add(add(root, offset), {point[2], point[1], -point[0]});
    check(ftebroom::bb::length(sub(deform(vertex, vertex.position, turned), expected)) < 3e-6,
          "Root motion or weighted rotation failed");
  }
  auto corrupt = bytes;
  corrupt[52 + 48 + 32] = 1;
  rejects([&] { readHumanoid(corrupt); }, "Accepted cyclic joint");
  rejects([&] { readHumanoid(std::string_view(bytes).substr(0, bytes.size() - 1)); }, "Accepted truncated asset");

  Document doc;
  const auto base = morphBody(rig, doc);
  check(bodyScale(doc) == 1, "Default scale changed the model");
  doc.values[0] = 1;
  check(bodyScale(doc) > 1 && morphBody(rig, doc) == base, "Height changed canonical rest shape");
  for (std::size_t morph = 1; morph < morphCount; ++morph) {
    Document one;
    one.values[morph] = 1;
    const auto points = morphBody(rig, one);
    unsigned moved = 0;
    for (std::size_t i = 0; i < points.size(); ++i)
      moved += ftebroom::bb::length(sub(points[i], base[i])) > 1e-5;
    check(moved >= 4, std::string("Morph has no observable mesh support: ") + morphs[morph].id);
  }
  std::set<std::vector<Vec3>> expressionsSeen;
  for (unsigned expression = 0; expression < std::size(expressions); ++expression) {
    Document one;
    one.expression = expression;
    check(expressionsSeen.insert(morphBody(rig,one)).second,"Duplicate facial expression");
  }
  std::set<unsigned> facialHairFaces;
  for (unsigned beard = 0; beard < std::size(beards); ++beard) {
    Document one;
    one.beard = beard;
    const auto fit = assemble(rig,one,lookup);
    const unsigned faces = std::count_if(fit.triangles.begin(),fit.triangles.end(),
                                        [](const auto& face) { return face.material == 7; });
    check((beard ? faces > 3 : faces == 0) && facialHairFaces.insert(faces).second,
          "Facial hair preset has no distinct geometry: "+std::string(beards[beard]));
  }
  for (unsigned seed = 0; seed < 1000; ++seed) {
    Document a, b;
    randomize(a, seed);
    randomize(b, seed);
    check(a.values == b.values, "Seeded morphs are not deterministic");
    const auto json = writeDocument(a, rig).dump();
    check(writeDocument(readDocument(json, rig), rig).dump() == json, "Document round trip drifted");
  }
  auto json = writeDocument(doc, rig);
  json["rigSha256"] = "different";
  rejects([&] { readDocument(json.dump(), rig); }, "Accepted incompatible rig");
  json = writeDocument(doc, rig);
  json["morphs"]["height"] = 20;
  rejects([&] { readDocument(json.dump(), rig); }, "Accepted out-of-range morph");
  const auto originalDocument = writeDocument(doc,rig).dump();
  auto duplicate = originalDocument;
  duplicate.insert(1,"\"schemaVersion\":2,");
  rejects([&] { parseDocument(duplicate); },"Accepted duplicate schema version");
  auto deeplyNested = std::string("{\"extension\":") + std::string(12,'[') + "0" +
                      std::string(12,']') + "}";
  rejects([&] { parseDocument(deeplyNested); },"Accepted excessive nesting in a future document");
  rejects([&] { parseDocument(std::string(65537,' ')); },"Accepted oversized document");
  for (std::size_t length=0; length<originalDocument.size(); ++length)
    rejects([&] { readDocument(std::string_view(originalDocument).substr(0,length),rig); },
            "Accepted a truncated canonical document");
  {
    std::mt19937 random(0x52423231);
    exerciseParserMutations(originalDocument,random,[&](const auto& input) { readDocument(input,rig); });
    exerciseParserMutations(bytes,random,[](const auto& input) { readHumanoid(input); });
    const auto sample=motionFixture(rig);
    exerciseParserMutations(sample,random,[&](const auto& input) { MotionGlb(input).retarget(rig); });
    std::ifstream garmentAsset(clothingDir/"male_worksuit01.rbw",std::ios::binary);
    check(garmentAsset.good(),"Unable to open the wardrobe fuzz seed");
    const std::string garment((std::istreambuf_iterator<char>(garmentAsset)),{});
    exerciseParserMutations(garment,random,[&](const auto& input) { readWardrobe(input,rig); });
    std::cout << "PASS 768 deterministic mutations across character, rig, wardrobe and GLB parsers\n";
  }
  const Animator animator(rig);
  {
    const auto doc = applyPreset({},presets[0]);
    auto build = doc;
    build.values[ShoulderWidth] = .75;
    const auto fitted = assemble(rig,build,lookup,true);
    const auto classic = assemble(rig,build,lookup);
    const auto shoulder = std::find_if(fitted.joints.begin(),fitted.joints.end(),
                                        [](const auto& joint) {return joint.name=="upperarm01.L";});
    check(shoulder!=fitted.joints.end() &&
          ftebroom::bb::length(sub(shoulder->bindPosition,
              classic.joints[std::size_t(shoulder-fitted.joints.begin())].bindPosition))>.015,
          "FTE shoulders must stay inside the morphed body");
    const Animator mocap(fitted,true);
    std::vector<Vec3> points;
    for (const auto& vertex : fitted.vertices) points.push_back(vertex.position);
    for (const auto& definition : {clips[3],clips[4],clips[5],clips[6],clips[7],clips[11]}) {
      const auto* gait = capturedGait(definition.motion);
      const auto clip = captureTiming(definition,true);
      const auto legacy = captureTiming(definition,false);
      check(gait && clip.seconds==gait->seconds && legacy.seconds==definition.seconds,
            "FTE must use the capture cadence while legacy profiles use their original timings");
      const auto pose = mocap.sample(clip,clip.seconds*.31);
      const auto start = mocap.sample(clip,0);
      const auto end = mocap.sample(clip,clip.seconds);
      check(start.root==end.root && ftebroom::bb::length(sub(start.root,pose.root))>.015,
            "Captured gait should move and loop continuously");
      std::array<Vec3,materialCount> tints;
      tints.fill({1,1,1});
      const auto buffer = previewMesh(fitted,doc,points,pose,{},tints,{0,0,1},true);
      AssetReader bytes(buffer);
      check(bytes.read(4)=="RBB2","Preview must submit smooth vertex normals");
      const auto parts = bytes.word();
      for (unsigned part=0; part<parts; ++part) {
        const auto vertices = bytes.word(), indices = bytes.word();
        bytes.read(128);
        for (unsigned i=0; i<vertices; ++i) {
          bytes.read(9*4);
          const Vec3 normal{bytes.scalar(1.001),bytes.scalar(1.001),bytes.scalar(1.001)};
          const auto length = ftebroom::bb::length(normal);
          check(length<.001 || std::abs(length-1)<1e-4,"Invalid preview normal");
        }
        for (unsigned i=0; i<indices; ++i)
          check(bytes.word()<vertices,"Preview index past smooth vertex stream");
      }
      check(!bytes.remaining(),"Preview stride does not match the dynamic mesh interface");
    }
    check(surfaceMaps(doc,true)[1].rfind("eyes-pbr-",0)==0 &&
          surfaceMaps(doc)[1].rfind("eyes-pbr-",0)!=0,
          "Physical eye textures must be scoped to the FTE profile");
  }
  {
    const auto run = animator.sample(clips[7],clips[7].seconds*.3);
    const auto skin = skinMatrices(rig,run.rotations,run.root);
    for (bool left : {true,false}) {
      const double sign = left ? 1 : -1;
      auto position = [&](std::string_view name) {
        const auto joint = std::find_if(rig.joints.begin(),rig.joints.end(),[&](const auto& joint) {
          return joint.name==name;
        });
        check(joint!=rig.joints.end(),"Missing arm joint");
        return skin[joint-rig.joints.begin()].point(joint->bindPosition);
      };
      const auto shoulder = position(left ? "upperarm01.L" : "upperarm01.R");
      const auto elbow = position(left ? "lowerarm01.L" : "lowerarm01.R");
      const auto wrist = position(left ? "wrist.L" : "wrist.R");
      check(sign*(wrist[0]-shoulder[0])<.85 && wrist[2]-elbow[2]>1.1,
            "Running arms should drive forward close to the body");
    }
  }
  for (const auto motion : {Motion::Walk,Motion::Backward,Motion::Left,Motion::Right,Motion::CrouchWalk}) {
    const auto clip = *std::find_if(std::begin(clips),std::end(clips),[motion](const auto& clip) {
      return clip.motion==motion;
    });
    for (bool left : {true,false}) {
      const auto side = left ? "foot.L" : "foot.R";
      const auto foot = std::find_if(rig.joints.begin(),rig.joints.end(),[side](const auto& joint) {
        return joint.name==side;
      });
      auto footHeight = [&](double phase) {
        const auto sample = animator.sample(clip,clip.seconds*std::fmod(phase+(left ? 0 : .5),1.));
        return skinMatrices(rig,sample.rotations,sample.root)[foot-rig.joints.begin()].point(foot->bindPosition)[1];
      };
      check(std::abs(footHeight(.24)-foot->bindPosition[1])<.09 &&
            footHeight(.83)>footHeight(.24)+.24,"Gait must support weight before clearing the ground");
    }
  }
  std::set<std::vector<double>> poses;
  for (const auto& clip : clips) {
    const auto start = animator.sample(clip, 0);
    const auto end = animator.sample(clip, clip.seconds);
    const auto middle = animator.sample(clip, clip.seconds * .37);
    const auto firstSkin = skinMatrices(rig, start.rotations, start.root);
    const auto endSkin = skinMatrices(rig, end.rotations, end.root);
    const auto middleSkin = skinMatrices(rig, middle.rotations, middle.root);
    std::vector<double> signature;
    double motion = 0;
    for (std::size_t i = 0; i < rig.vertices.size(); i += 19) {
      const auto& vertex = rig.vertices[i];
      const auto moved = deform(vertex, vertex.position, middleSkin);
      motion += ftebroom::bb::length(sub(moved, deform(vertex, vertex.position, firstSkin)));
      if (clip.loop)
        check(deform(vertex, vertex.position, firstSkin) == deform(vertex, vertex.position, endSkin),
              "Loop endpoints differ");
      signature.insert(signature.end(), moved.begin(), moved.end());
    }
    check(motion > .1 && poses.insert(std::move(signature)).second, "Static or duplicate animation");
  }
  {
    const auto from = animator.sample(clips[3],.35);
    const auto to = animator.sample(clips[18],.79);
    const auto mask = upperBodyMask(rig);
    const auto blended = blendAnimation(from,to,1.,mask);
    bool movedArm = false, keptLeg = false;
    for (std::size_t j = 0; j < rig.joints.size(); ++j) {
      const auto& joint = rig.joints[j];
      const auto expected = (mask[j] ? to : from).rotations[j].vector({.5,.4,.3});
      check(ftebroom::bb::length(sub(expected,blended.rotations[j].vector({.5,.4,.3})))<1e-8,
            "Masked blend does not preserve the selected animation channels");
      if (joint.name=="upperarm01.R") movedArm = mask[j];
      if (joint.name=="upperleg01.R") keptLeg = !mask[j];
    }
    check(movedArm && keptLeg && blended.root==from.root,"Upper-body mask must keep locomotion");
    const auto oppositeA = ftebroom::bb::rotation({0,0,170});
    const auto oppositeB = ftebroom::bb::rotation({0,0,-170});
    const auto halfway = blendRotation(oppositeA,oppositeB,.5).vector({1,0,0});
    check(ftebroom::bb::length(sub(halfway,{-1,0,0}))<1e-8,"Quaternion blend took the long path");
  }
  const auto fitted = assemble(rig,doc,lookup);
  exerciseSkinnedPoses(fitted);
  {
    auto layered = doc;
    layered.footwear = 1;
    layered.headwear = 2;
    const auto changed = assemble(rig,layered,lookup);
    const auto countMaterial = [](const Humanoid& shape,unsigned id) {
      return std::count_if(shape.triangles.begin(),shape.triangles.end(),[id](const auto& face) {
        return face.material==id;
      });
    };
    check(countMaterial(fitted,4)>0 && countMaterial(changed,4)==0,"Footwear override hides shoes");
    check(countMaterial(fitted,5)>0 && countMaterial(changed,5)==0 &&
          countMaterial(changed,6)>0,"Hat replaces hair");
    auto saved = writeDocument(layered,rig);
    auto restored = readDocument(saved.dump(),rig);
    check(restored.footwear==1 && restored.headwear==2,"Wardrobe layers persist");
    saved.erase("footwear");
    saved.erase("headwear");
    saved.erase("wardrobeVersion");
    restored = readDocument(saved.dump(),rig);
    check(restored.footwear==0 && restored.headwear==0,"Older documents keep outfit defaults");
  }
  const auto textureDir = std::filesystem::path(argv[1]).parent_path()/"materials";
  std::ifstream registry(textureDir/"provenance.json");
  const auto materialRegistry = Json::parse(registry).at("textures");
  const auto maps = surfaceMaps(doc);
  MaterialImages images;
  for (unsigned i = 0; i < maps.size(); ++i) if (!maps[i].empty()) {
    std::ifstream bytes(textureDir/materialRegistry.at(maps[i]).at("file").get<std::string>(),std::ios::binary);
    check(bytes.good(),"Missing UV-matched character texture");
    images[i].assign(std::istreambuf_iterator<char>(bytes),{});
  }
  auto mature = doc;
  mature.complexion = 5;
  check(surfaceMaps(readDocument(writeDocument(mature,rig).dump(),rig))[0]=="skin-mature",
        "Complexion map did not survive document round trip");
  auto expressionDoc=doc;
  expressionDoc.expression=0;
  const auto expressionBase=assemble(rig,expressionDoc,lookup);
  const auto expressionTargets=glbExpressionTargets(rig,expressionBase,expressionDoc,lookup,false);
  expressionDoc.expression=1;
  const auto exported = exportGlb(expressionBase,expressionDoc,true,images,false,nullptr,&expressionTargets);
  if (argc >= 3) {
    std::ofstream out(argv[2], std::ios::binary);
    out.write(exported.data(), exported.size());
    check(out.good(), "Failed to write GLB fixture");
  }
  AssetReader reader(exported);
  check(reader.read(4) == "glTF" && reader.word() == 2 && reader.word() == exported.size(),
        "Invalid GLB header");
  const auto jsonSize = reader.word();
  check(reader.read(4) == "JSON", "Missing GLB JSON chunk");
  const auto gltf = Json::parse(reader.read(jsonSize));
  check(gltf.at("meshes").at(0).at("weights") == Json::array({1.,0.,0.,0.,0.}) &&
        gltf.at("meshes").at(0).at("primitives").at(0).at("targets").size()==5,
        "Animated GLB lost editable expressions or the selected expression weight");
  check(gltf.at("animations").size() == std::size(clips) &&
        gltf.at("skins").at(0).at("joints").size() == rig.joints.size(),
        "GLB lost rig or animations");
  const auto binarySize = reader.word();
  check(reader.read(4) == std::string_view("BIN\0", 4) && reader.remaining() == binarySize,
        "Invalid GLB binary chunk");
  std::array<std::string,materialCount> colorOnly;
  colorOnly.fill("$whiteimage");
  const auto iqm = exportIqm(fitted,doc,colorOnly,true);
  if (argc >= 4) {
    std::ofstream out(argv[3],std::ios::binary);
    out.write(iqm.data(),iqm.size());
    check(out.good(),"Failed to write IQM fixture");
  }
  AssetReader iqmReader(iqm);
  check(iqmReader.read(16) == std::string_view("INTERQUAKEMODEL\0",16) &&
        iqmReader.word() == 2 && iqmReader.word() == iqm.size(),"Invalid IQM header");
  iqmReader.read(11 * 4);
  check(iqmReader.word() == rig.joints.size(),"IQM joint count");
  iqmReader.read(4);
  check(iqmReader.word() == rig.joints.size(),"IQM pose count");
  iqmReader.read(4);
  check(iqmReader.word() == std::size(clips),"IQM animation count");
  iqmReader.read(4);
  check(iqmReader.word() > 500 && iqmReader.word() > 20,"Missing sampled IQM channels");
  check(exported.size() > 1000000,"Dressed GLB export lost geometry");
  if (argc == 5) {
    std::array<std::string,materialCount> materials;
    materials.fill("models/character");
    std::array<ftebroom::modelExport::Skin,materialCount> textured;
    for (auto& image : textured) {
      image = {32,32,{}};
      for (unsigned y=0; y<32; ++y) for (unsigned x=0; x<32; ++x) {
        image.rgba.push_back(std::uint8_t(x*7));
        image.rgba.push_back(std::uint8_t(y*7));
        image.rgba.push_back(std::uint8_t((x+y)*3));
        image.rgba.push_back(255);
      }
    }
    const auto quake1 = bakeLegacy(fitted,doc,2,materials,&textured);
    const auto quake2 = bakeLegacy(fitted,doc,3,materials,&textured);
    const auto quake3 = bakeLegacy(fitted,doc,0,materials);
    std::string palette;
    for (unsigned i = 0; i < 256; ++i)
      for (unsigned channel = 0; channel < 3; ++channel) palette.push_back(char(i));
    const auto md2 = ftebroom::modelExport::md2(quake2.mesh,quake2.skins,palette,"models/character.pcx");
    const auto directory = std::filesystem::path(argv[4]);
    std::filesystem::create_directories(directory);
    auto write = [&](const char* name, const std::string& data) {
      std::ofstream out(directory/name,std::ios::binary);
      out.write(data.data(),data.size());
      check(out.good(),"Cannot write legacy fixture");
    };
    write("quake1.mdl",ftebroom::modelExport::mdl(quake1.mesh,quake1.skins,palette));
    write("quake2.md2",md2.model);
    write("character.pcx",md2.skin);
    write("quake3.md3",ftebroom::modelExport::md3(quake3.mesh));
    textured[5].rgba[3] = 0;
    write("goldsrc.mdl",exportStudio(fitted,doc,&textured));
    write("framegroups.json",quake1.frameGroups.dump(2));
    write("quake3.md3.framegroups",engineFrameGroups(quake3.frameGroups).dump(2));
    std::filesystem::create_directories(directory/"characters");
    for (std::size_t i=0; i<10; ++i) {
      const auto sample = applyPreset({},presets[i]);
      const auto name = "characters/"+sample.name+".rbchar";
      write(name.c_str(),writeDocument(sample,rig).dump(2)+"\n");
    }
    check(quake1.mesh.frames > 200 && quake2.mesh.frames == quake1.mesh.frames &&
          quake3.mesh.frames == quake1.mesh.frames,"Legacy vertex animation frame count");
  }
  std::cout << "PASS canonical rig, skinning, " << morphCount
            << " morphs, 1000 document round trips and " << std::size(clips)
            << " distinct animations in GLB and IQM\n";
} catch (const std::exception& error) {
  std::cerr << error.what() << '\n';
  return 1;
}
