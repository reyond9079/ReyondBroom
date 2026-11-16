// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "document.h"
#include "field_technician_face.h"
#include <functional>
#include <optional>

namespace ftebroom::character {
struct Outfit {
  const char* id;
  const char* label;
  const char* clothes;
  const char* shoes;
  const char* hat;
  Vec3 color;
  const char* atlas = nullptr;
  bool utilityCut = false;
  const char* footwearAtlas = nullptr;
};
inline constexpr Outfit outfits[]{
  {"casual","Casual","male_casualsuit03","shoes02","",{.14,.23,.30}},
  {"weekend","Weekend","female_casualsuit02","shoes04","",{.28,.21,.32}},
  {"office","Office","male_elegantsuit01","shoes03","",{.17,.21,.24}},
  {"formal","Formal","female_elegantsuit01","shoes03","",{.29,.13,.17}},
  {"runner","Runner","female_sportsuit01","shoes02","",{.18,.36,.30}},
  {"worker","Worker","male_worksuit01","shoes04","",{.25,.31,.32}},
  {"outdoors","Outdoors","male_casualsuit05","shoes04","fedora01",{.31,.26,.16}},
  {"summer","Summer","male_casualsuit04","shoes02","",{.37,.37,.21}},
  {"traveler","Traveler","female_casualsuit01","shoes04","fedora01",{.23,.32,.34}},
  {"winter","Winter","male_casualsuit06","shoes04","",{.21,.23,.39}},
  {"street","Street","male_casualsuit02","shoes02","",{.26,.27,.28}},
  {"survivor","Survivor","male_casualsuit01","shoes04","",{.32,.26,.18}},
  {"charcoal-uniform","Charcoal utility uniform","male_utility_field","shoes04","",{.24,.27,.30},
    "charcoal-utility",true,"field-boots"}
};
enum class HairTexture { Any, Straight, Wavy, Curly, Coily, Braided };
inline constexpr const char* hairTextureNames[]{"All","Straight","Wavy","Curly","Coily","Braided"};
struct Hairstyle {
  const char* mesh;
  const char* label;
  HairTexture texture;
  double volume = .84, length = 1, sweep = 0;
  const char* fringeAtlas = nullptr;
};
inline constexpr Hairstyle hairstyles[]{
  {"","Shaved",HairTexture::Any},
  {"short01","Side part",HairTexture::Straight},
  {"short02","Textured crop",HairTexture::Wavy},
  {"short03","Swept fringe",HairTexture::Straight},
  {"short04","Close crop",HairTexture::Straight,.99},
  {"afro01","Natural afro",HairTexture::Coily,.95},
  {"bob01","Layered bob",HairTexture::Wavy},
  {"braid01","Braided updo",HairTexture::Braided},
  {"long01","Long layers",HairTexture::Straight},
  {"ponytail01","Ponytail",HairTexture::Straight,1.04},
  {"bob02","Straight bob",HairTexture::Straight,1.02},
  {"afro01","Compact coils",HairTexture::Coily,.48},
  {"afro01","Rounded curls",HairTexture::Curly,.69},
  {"afro01","Full coils",HairTexture::Coily,.84},
  {"short01","Brushed side part",HairTexture::Straight,.63,1,.37},
  {"short02","Short waves",HairTexture::Wavy,.60,1,-.28},
  {"long01","Shoulder-length layers",HairTexture::Straight,.78,.45},
  {"short02-tousled","Tousled fringe",HairTexture::Wavy,.76,1.01,-.17,"tousled-fringe"}
};
inline constexpr const char* faceTemplateNames[]{"Blend","European","East Asian","African"};
inline constexpr const char* eyebrowNames[]{"Natural","Fine","Full","None"};
inline constexpr const char* eyebrowMeshes[]{"eyebrow010","eyebrow001","eyebrow012",""};
static_assert(std::size(outfits)==outfitCount && std::size(hairstyles)==hairstyleCount);
static_assert(std::size(faceTemplateNames)==selectableFaceTemplateCount &&
              fieldTechnicianFaceTemplate>=selectableFaceTemplateCount &&
              std::size(eyebrowNames)==eyebrowCount);

inline constexpr const char* faceTemplateLabel(unsigned id) {
  return id<selectableFaceTemplateCount ? faceTemplateNames[id] : "Legacy saved template";
}
struct Wardrobe {
  struct Entry {
    Vertex vertex;
    std::array<double,3> factors;
    std::array<Vec3,3> anchors;
    std::array<unsigned,3> faceAnchors{UINT32_MAX,UINT32_MAX,UINT32_MAX};
  };
  std::vector<Entry> vertices;
  std::vector<std::array<std::uint32_t,3>> triangles;
  std::vector<std::uint32_t> hidden;
};

inline void refineClothingCoverage(Wardrobe& item,const Humanoid& rig);

inline Wardrobe readWardrobe(std::string_view bytes, const Humanoid& rig,std::string_view asset = {}) {
  AssetReader reader(bytes);
  if (reader.read(4) != "RBW1" || reader.word() != 1)
    throw std::runtime_error("Unsupported wardrobe asset");
  const auto vertices = reader.word(), faces = reader.word(), hide = reader.word();
  if (vertices < 3 || vertices > 25000 || faces == 0 || faces > 50000 || hide > 20000 ||
      reader.remaining() != 32 + vertices * 92 + faces * 12 + hide * 4)
    throw std::runtime_error("Invalid wardrobe asset dimensions");
  constexpr char hex[] = "0123456789abcdef";
  std::string digest;
  for (const std::uint8_t byte : reader.read(32)) {
    digest += hex[byte >> 4];
    digest += hex[byte & 15];
  }
  if (digest != rig.rigHash) throw std::runtime_error("Wardrobe uses an incompatible skeleton");
  Wardrobe item;
  item.vertices.reserve(vertices);
  for (std::uint32_t i = 0; i < vertices; ++i) {
    Wardrobe::Entry entry{};
    auto& v = entry.vertex;
    v.source = reader.word();
    if (v.source >= 25000) throw std::runtime_error("Invalid wardrobe topology index");
    v.position = {reader.scalar(100),reader.scalar(100),reader.scalar(100)};
    v.uv = {reader.scalar(10),reader.scalar(10)};
    const auto joints = reader.read(4);
    double total = 0;
    for (unsigned index = 0; index < 4; ++index) {
      v.joints[index] = std::uint8_t(joints[index]);
      v.weights[index] = reader.scalar(1);
      if (v.joints[index] >= rig.joints.size() || v.weights[index] < 0)
        throw std::runtime_error("Invalid wardrobe skin weight");
      total += v.weights[index];
    }
    if (std::abs(total-1) > 1e-4) throw std::runtime_error("Unnormalized wardrobe skin weights");
    for (auto& factor : entry.factors) factor = reader.scalar(8);
    if (std::abs(entry.factors[0]+entry.factors[1]+entry.factors[2]-1) > 1e-4)
      throw std::runtime_error("Unnormalized wardrobe anchor");
    for (unsigned index=0; index<entry.anchors.size(); ++index) {
      auto& anchor = entry.anchors[index];
      for (auto& coordinate : anchor) coordinate = reader.scalar(100);
      if (!rig.faceShapes || anchor[1]<=13.75) continue;
      const auto match = rig.faceShapes->anchors.find(FaceShapes::anchorKey(anchor));
      if (match!=rig.faceShapes->anchors.end()) entry.faceAnchors[index] = match->second;
    }
    item.vertices.push_back(std::move(entry));
  }
  item.triangles.reserve(faces);
  for (std::uint32_t i = 0; i < faces; ++i) {
    const std::array<std::uint32_t,3> face{reader.word(),reader.word(),reader.word()};
    if (std::max({face[0],face[1],face[2]}) >= vertices || face[0]==face[1] || face[1]==face[2] || face[0]==face[2])
      throw std::runtime_error("Invalid wardrobe face index");
    item.triangles.push_back(face);
  }
  for (std::uint32_t i = 0; i < hide; ++i) {
    const auto id = reader.word();
    if (id >= 65536 || (!item.hidden.empty() && id <= item.hidden.back()))
      throw std::runtime_error("Invalid wardrobe coverage mask");
    item.hidden.push_back(id);
  }
  if (!asset.empty() && std::any_of(std::begin(outfits),std::end(outfits),[&](const Outfit& outfit) {
    return !outfit.utilityCut && outfit.clothes==asset;
  })) refineClothingCoverage(item,rig);
  return item;
}

using WardrobeReader = std::function<const Wardrobe&(const std::string&)>;
inline constexpr unsigned materialCount = 10;
inline constexpr unsigned browMaterial = 8, lashMaterial = 9;
inline constexpr const char* skinProgramPath = "glsl/reyond_character_skin_v2.glsl";
inline bool cutoutMaterial(unsigned id) {
  return id==5 || id==browMaterial || id==lashMaterial;
}
inline Vec3 materialColor(unsigned id, const Document& doc) {
  const Vec3 colors[]{doc.skin,doc.eyes,{.82,.80,.73},
                      outfits[doc.outfit].color,{.105,.085,.07},doc.hairColor,{.20,.16,.12},doc.hairColor,doc.hairColor,{.055,.035,.027}};
  return colors[id];
}

inline double smoothBand(double lower,double upper,double value) {
  const auto t = std::clamp((value-lower)/(upper-lower),0.,1.);
  return t*t*(3-2*t);
}

inline constexpr double scalpHairEdge(const Hairstyle& style) {
  const std::string_view mesh(style.mesh);
  if (mesh=="afro01") return 16.007;
  if (mesh=="ponytail01") return 16.14;
  if (mesh=="short04") return 15.98;
  if (mesh.substr(0,5)=="short") return 16.06;
  if (mesh=="braid01") return 16.03;
  return *style.mesh ? 16.18 : 0.;
}

inline double lipCoverage(const Vec3& p) {
  const auto x = std::abs(p[0]);
  if (x>=.31 || p[1]<14.90 || p[1]>15.13 || p[2]<1.33) return 0;
  const auto corners = smoothBand(.17,.30,x);
  const auto upper = 15.076-.055*corners+.016*std::exp(-std::pow((x-.080)/.049,2));
  const auto lower = 14.928+.071*corners;
  return smoothBand(lower-.021,lower+.027,p[1])*
      (1-smoothBand(upper-.023,upper+.015,p[1]))*
      (1-smoothBand(.229,.307,x))*smoothBand(1.37,1.53,p[2]);
}

inline double hairRootCoverage(const Vec3& p,const Document& doc,double scalpEdge) {
  if (!scalpEdge || p[1]<15.39) return 0;
  const auto irregularity = .022*std::sin(18*p[0])+.017*std::sin(31*p[0]+1.);
  const auto temples = smoothBand(.24,.58,std::abs(p[0]));
  const auto back = 1-smoothBand(.20,1.0,p[2]);
  const auto hairEdge = scalpEdge-.11*temples+.078*doc.hairline+irregularity-.21*back;
  const auto hairline = smoothBand(15.55,15.66,p[1])*smoothBand(hairEdge-.15,hairEdge+.22,p[1]);
  if (std::string_view(hairstyles[doc.hairstyle].mesh)!="short01") return hairline;
  // This side-part mesh uses separate alpha cards at the temples. Match
  // the scalp to the short roots visible between those cards, above the ear.
  const auto side = smoothBand(.47,.69,std::abs(p[0]))*(1-smoothBand(.88,1.13,p[2]));
  const auto roots = side*smoothBand(15.46+.06*doc.hairline,15.72+.06*doc.hairline,p[1]);
  return std::max(hairline,roots);
}

inline Vec3 headSurfaceColor(const Vec3& p,const Document& doc,double rootCoverage) {
  if (p[1]<14.52) return {1,1,1};
  const double x = std::abs(p[0]);
  const auto local = [&](double cx,double cy,double rx,double ry) {
    return std::exp(-std::pow((x-cx)/rx,2)-std::pow((p[1]-cy)/ry,2));
  };
  const auto lid = local(.305,15.506,.172,.039)*smoothBand(1.27,1.40,p[2]);
  const auto innerEye = local(.175,15.447,.053,.058)*smoothBand(1.28,1.41,p[2]);
  const auto cheek = local(.48,15.27,.23,.23)*smoothBand(.85,1.34,p[2]);
  const auto nose = local(.075,15.22,.14,.20)*smoothBand(1.39,1.67,p[2]);
  const auto underEye = local(.324,15.351,.17,.058)*smoothBand(1.24,1.38,p[2]);
  const auto ear = local(.773,15.37,.19,.27)*(1-smoothBand(.72,1.06,p[2]));
  const auto fair = smoothBand(.51,.78,doc.skin[0]);
  const auto lips = lipCoverage(p);
  const auto upperLip = smoothBand(15.009,15.060,p[1]);
  const bool fieldTechnician = doc.faceTemplate==fieldTechnicianFaceTemplate;
  const auto noseTint = nose*(fieldTechnician ? .64 : 1.);
  const auto lipTint = fieldTechnician ?
      Vec3{.062+.045*upperLip,.073+.045*upperLip,.063+.044*upperLip} :
      Vec3{.036+.065*upperLip,.095+.059*upperLip,.071+.054*upperLip};
  Vec3 tint{1-.075*lid-.025*fair*underEye+.018*cheek+.013*noseTint,
            1-.14*lid-.09*innerEye-.040*cheek-.046*noseTint-.058*fair*underEye-.105*fair*ear,
            1-.12*lid-.035*innerEye-.012*cheek-.030*noseTint-.020*fair*underEye-.064*fair*ear};
  tint = sub(tint,mul(lipTint,lips));
  // Match the exposed scalp between strands to the roots while fading out
  // before the visible hairline.
  const auto amount = rootCoverage;
  // Counter the underlying complexion where the hair cards reveal short roots.
  // The visible color there should be driven by the hair, including on fair skin.
  const Vec3 scalpTint{.54,.70,.85};
  for (unsigned i=0; i<3; ++i) {
    const auto root = std::clamp(doc.hairColor[i]*scalpTint[i]/std::max(.28,doc.skin[i]),.07,.92);
    tint[i] *= 1-amount*(1-root);
  }
  return tint;
}

struct FaceSurface {
  const Humanoid& rig;
  std::array<std::vector<const Triangle*>,2> patches;

  explicit FaceSurface(const Humanoid& rig) : rig(rig) {
    for (const auto& face : rig.triangles) {
      if (face.material!=0) continue;
      for (unsigned side=0; side<2; ++side) {
        const double sign = side ? -1 : 1;
        // Only the front of the face can support a brow. Leave some margin
        // around the longest style for the triangles at its tapered ends.
        if (std::all_of(face.indices.begin(),face.indices.end(),[&](unsigned id) {
          const auto& p = rig.vertices[id].position;
          return p[1]>15.31 && p[1]<15.82 && p[2]>1.02 &&
                 p[0]*sign>0 && p[0]*sign<.75;
        })) patches[side].push_back(&face);
      }
    }
  }

  struct Position {
    std::array<unsigned,3> ids{};
    std::array<double,3> weights{};
    bool found = false;
  };

  // Bind in rest space once per assembled brow vertex. Applying the same
  // barycentric weights to the fitted face keeps the brow on the surface
  // even when a face preset changes the forehead or eye socket depth.
  Position bind(Vec3 point) const {
    Position result;
    double closestDepth = -1;
    for (const auto* face : patches[point[0]<0]) {
      const auto [a,b,c] = face->indices;
      const auto& origin = rig.vertices[a].position;
      const auto ab = sub(rig.vertices[b].position,origin);
      const auto ac = sub(rig.vertices[c].position,origin);
      const auto ray = sub(point,origin);
      const double area = ab[0]*ac[1]-ab[1]*ac[0];
      if (std::abs(area)<1e-9) continue;
      const double v = (ray[0]*ac[1]-ray[1]*ac[0])/area;
      const double w = (ab[0]*ray[1]-ab[1]*ray[0])/area;
      const double u = 1-v-w;
      if (std::min({u,v,w}) < -1e-6) continue;
      const double depth = origin[2]+v*ab[2]+w*ac[2];
      if (depth<=closestDepth) continue;
      closestDepth = depth;
      result = {{a,b,c},{u,v,w},true};
    }
    return result;
  }
};

inline void appendBrows(Humanoid& dressed,const Humanoid& rig,const std::vector<Vec3>& fitted,
                        const FaceSurface& surface,const Wardrobe& brow,std::uint32_t& nextSource) {
  std::vector<unsigned> right(brow.vertices.size(),UINT32_MAX);
  std::vector<unsigned> left(brow.vertices.size(),UINT32_MAX);
  for (unsigned i=0; i<brow.vertices.size(); ++i) {
    const auto& templateVertex = brow.vertices[i].vertex;
    if (templateVertex.position[0]<=0) continue;
    auto other = templateVertex.position;
    other[0] = -other[0];
    const auto paired = std::find_if(brow.vertices.begin(),brow.vertices.end(),[&](const auto& v) {
      const auto difference = sub(v.vertex.position,other);
      return dot(difference,difference)<1e-9;
    });
    if (paired==brow.vertices.end()) continue;
    const auto a = surface.bind(templateVertex.position);
    const auto b = surface.bind(paired->vertex.position);
    if (!a.found || !b.found) continue;
    auto conform = [&](const Vertex& original,const FaceSurface::Position& bind) {
      auto vertex = original;
      vertex.uv = templateVertex.uv;
      vertex.source = nextSource++;
      vertex.position = {};
      for (unsigned j=0; j<3; ++j)
        vertex.position = add(vertex.position,mul(fitted[bind.ids[j]],bind.weights[j]));
      const auto p0 = fitted[bind.ids[0]], p1 = fitted[bind.ids[1]], p2 = fitted[bind.ids[2]];
      auto outward = unit(cross(sub(p1,p0),sub(p2,p0)));
      if (outward[2]<0) outward = mul(outward,-1);
      vertex.position = add(vertex.position,mul(outward,.014));
      vertex.color = {1,1,1};
      return vertex;
    };
    right[i] = std::uint32_t(dressed.vertices.size());
    dressed.vertices.push_back(conform(templateVertex,a));
    left[i] = std::uint32_t(dressed.vertices.size());
    dressed.vertices.push_back(conform(paired->vertex,b));
  }
  for (const auto& face : brow.triangles) {
    const auto [a,b,c] = face;
    if (std::max({right[a],right[b],right[c]})==UINT32_MAX) continue;
    dressed.triangles.push_back({{right[a],right[b],right[c]},browMaterial});
    dressed.triangles.push_back({{left[a],left[c],left[b]},browMaterial});
  }
}

inline void attachLashes(Humanoid& dressed,const Humanoid& rig,const FaceSurface& surface,
                         const std::vector<Vec3>& fitted,const Wardrobe& lashes,unsigned first) {
  // The contact curves run along the visible eyelid rims in rest space.
  // Keep the tip offsets relative to those curves so lip/eye sculpting and
  // facial expressions cannot leave a strip suspended above an eyelid.
  constexpr Vec3 upper[]{{.178,15.461,1.397},{.214,15.486,1.402},{.269,15.499,1.407},
                         {.327,15.505,1.409},{.381,15.499,1.395},{.432,15.478,1.377},
                         {.474,15.452,1.345},{.498,15.450,1.321}};
  constexpr Vec3 lower[]{{.182,15.423,1.388},{.214,15.407,1.382},{.267,15.399,1.394},
                         {.327,15.400,1.398},{.383,15.415,1.383},{.426,15.436,1.363},
                         {.469,15.449,1.340},{.49,15.450,1.322}};
  for (unsigned i=0; i<lashes.vertices.size(); ++i) {
    const auto p = lashes.vertices[i].vertex.position;
    const bool upperLid = p[1]>15.444;
    const auto& curve = upperLid ? upper : lower;
    const auto x = std::abs(p[0]);
    const auto step = std::upper_bound(std::begin(curve),std::end(curve),x,
        [](double x,const Vec3& point) { return x<point[0]; });
    const auto segment = std::clamp<std::ptrdiff_t>(step-std::begin(curve),1,std::size(curve)-1);
    const auto a = curve[segment-1], b = curve[segment];
    const auto fraction = std::clamp((x-a[0])/(b[0]-a[0]),0.,1.);
    auto contact = add(a,mul(sub(b,a),fraction));
    contact[0] = std::copysign(contact[0],p[0]);
    const auto binding = surface.bind(contact);
    if (!binding.found) continue;
    auto& vertex = dressed.vertices[first+i];
    Vec3 delta{};
    double fittedDepth = 0;
    for (unsigned j=0; j<3; ++j) {
      const auto id = binding.ids[j];
      delta = add(delta,mul(sub(fitted[id],rig.vertices[id].position),binding.weights[j]));
      fittedDepth += fitted[id][2]*binding.weights[j];
    }
    vertex.position = add(p,delta);
    const auto away = std::abs(p[1]-contact[1]);
    const auto curl = .026*smoothBand(.004,.05,away);
    vertex.position[2] = std::max(vertex.position[2]+.007,fittedDepth+.010+curl);
  }
}

struct FittingSurface {
  struct Face {
    Vec3 point, edge1, edge2, low, high;
    double area;
  };
  struct Box {
    Vec3 low, high;
    unsigned begin, end, right=0;
  };
  struct Ray {
    Vec3 origin,direction,reciprocal;
    double limit,normalAlignment;
  };
  Vec3 center{};
  std::vector<Face> faces;
  std::vector<Box> boxes;

  void addTriangle(Vec3 a,Vec3 b,Vec3 c) {
    a=sub(a,center);
    b=sub(b,center);
    c=sub(c,center);
    Face face{a,sub(b,a),sub(c,a),{},{},bb::length(cross(sub(b,a),sub(c,a)))};
    for (unsigned axis=0;axis<3;++axis) {
      face.low[axis]=std::min({a[axis],b[axis],c[axis]})-1e-5;
      face.high[axis]=std::max({a[axis],b[axis],c[axis]})+1e-5;
    }
    faces.push_back(face);
  }

  void subdivide(unsigned begin,unsigned end) {
    Box box{faces[begin].low,faces[begin].high,begin,end};
    for (unsigned index=begin+1;index<end;++index) {
      for (unsigned axis=0;axis<3;++axis) {
        box.low[axis]=std::min(box.low[axis],faces[index].low[axis]);
        box.high[axis]=std::max(box.high[axis],faces[index].high[axis]);
      }
    }
    const auto id=boxes.size();
    boxes.push_back(box);
    if (end-begin<=12) return;
    const auto extent=sub(box.high,box.low);
    const auto axis=unsigned(std::max_element(extent.begin(),extent.end())-extent.begin());
    const auto middle=(begin+end)/2;
    std::nth_element(faces.begin()+begin,faces.begin()+middle,faces.begin()+end,
                     [axis](const Face& a,const Face& b) {
      return a.low[axis]+a.high[axis]<b.low[axis]+b.high[axis];
    });
    subdivide(begin,middle);
    boxes[id].right=unsigned(boxes.size());
    subdivide(middle,end);
  }

  void trace(unsigned id,const Ray& ray,double& outer) const {
    const auto& box=boxes[id];
    double enter=outer, exit=ray.limit;
    for (unsigned axis=0;axis<3;++axis) {
      if (std::abs(ray.direction[axis])<1e-12) {
        if (box.low[axis]>ray.origin[axis] || box.high[axis]<ray.origin[axis]) return;
        continue;
      }
      const auto a=(box.low[axis]-ray.origin[axis])*ray.reciprocal[axis];
      const auto b=(box.high[axis]-ray.origin[axis])*ray.reciprocal[axis];
      enter=std::max(enter,std::min(a,b));
      exit=std::min(exit,std::max(a,b));
      if (enter>exit) return;
    }
    if (box.right) {
      trace(id+1,ray,outer);
      trace(box.right,ray,outer);
      return;
    }
    for (unsigned index=box.begin;index<box.end;++index) {
      const auto& face=faces[index];
      const auto p = cross(ray.direction,face.edge2);
      const auto det = dot(face.edge1,p);
      if (std::abs(det)<1e-9 || (ray.normalAlignment>0 && -det<face.area*ray.normalAlignment)) continue;
      const auto from=sub(ray.origin,face.point);
      const auto q = cross(from,face.edge1);
      const auto u = dot(from,p)/det, v = dot(ray.direction,q)/det;
      const auto hit=dot(face.edge2,q)/det;
      if (u>=-1e-6 && v>=-1e-6 && u+v<=1+1e-6 && hit<=ray.limit)
        outer = std::max(outer,hit);
    }
  }

  double farthest(Vec3 origin,Vec3 direction,double minimum,double maximum,double normalAlignment=0) const {
    if (boxes.empty()) return minimum;
    Ray ray{sub(origin,center),direction,{},maximum,normalAlignment};
    for (unsigned coordinate=0;coordinate<3;++coordinate)
      if (std::abs(direction[coordinate])>=1e-12) ray.reciprocal[coordinate]=1/direction[coordinate];
    double outer=minimum;
    trace(0,ray,outer);
    return outer;
  }

};

inline void refineClothingCoverage(Wardrobe& item,const Humanoid& rig) {
  if (item.hidden.empty()) return;
  constexpr unsigned sourceCount=65536;
  std::vector<bool> covered(sourceCount);
  for (const auto index : item.hidden) covered[index]=true;
  std::vector<Vec3> normal(sourceCount);
  std::vector<std::array<unsigned,3>> skinFaces;
  skinFaces.reserve(rig.triangles.size());
  for (const auto& face : rig.triangles) {
    if (face.material!=0) continue;
    const auto& a=rig.vertices[face.indices[0]];
    const auto& b=rig.vertices[face.indices[1]];
    const auto& c=rig.vertices[face.indices[2]];
    const auto n=cross(sub(b.position,a.position),sub(c.position,a.position));
    const std::array<unsigned,3> sources{a.source,b.source,c.source};
    skinFaces.push_back(sources);
    for (const auto source : sources) {
      normal[source]=add(normal[source],n);
    }
  }
  FittingSurface shell;
  for (const auto& face : item.triangles)
    shell.addTriangle(item.vertices[face[0]].vertex.position,item.vertices[face[1]].vertex.position,
                      item.vertices[face[2]].vertex.position);
  shell.subdivide(0,unsigned(shell.faces.size()));
  std::vector<bool> tested=covered;
  // Some sleeve masks end several rows before the actual cuff. Only grow
  // from already covered skin, without crossing a point the shell exposes.
  for (unsigned ring=0;ring<8;++ring) {
    const auto previousSize=item.hidden.size();
    std::vector<bool> nearby(sourceCount);
    for (const auto& face : skinFaces)
      if (std::any_of(face.begin(),face.end(),[&](unsigned source){return covered[source];}))
        for (const auto source : face) nearby[source]=true;
    for (const auto& vertex : rig.vertices) {
      const auto source=vertex.source;
      if (tested[source] || !nearby[source]) continue;
      tested[source]=true;
      const auto p=vertex.position;
      const bool belowNeck = p[1]<=13.65 ||
                             (p[1]<=14.20 && (std::abs(p[0])>=.60 || p[2]<.35));
      if (!belowNeck || dot(normal[source],normal[source])<1e-10) continue;
      const auto n=unit(normal[source]);
      const auto tangent=mul(unit(cross(n,std::abs(n[1])<.87 ? Vec3{0,1,0} : Vec3{1,0,0})),.040);
      const auto bitangent=cross(n,tangent);
      const auto offsets={Vec3{},tangent,mul(tangent,-1),bitangent,mul(bitangent,-1)};
      // A seam can lie just inside the skin in the authored mesh. Its front
      // surface still indicates coverage when it is close and faces the same
      // direction. Keep the tighter tolerance around the exposed neck.
      const double inset = p[1]<13.30 ? -.19 : -.055;
      if (std::all_of(offsets.begin(),offsets.end(),[&](const Vec3& offset){
            return shell.farthest(add(p,offset),n,inset,.38,.28)>inset;
          })) {
        covered[source]=true;
        item.hidden.push_back(source);
      }
    }
    if (item.hidden.size()==previousSize) break;
  }
  std::sort(item.hidden.begin(),item.hidden.end());
}

inline FittingSurface fittedScalp(const Humanoid& rig,const std::vector<Vec3>& positions,bool followsNeck=false) {
  FittingSurface scalp;
  scalp.center={0,15.53,.40};
  for (const auto& face : rig.triangles) {
    if (face.material!=0 || !std::all_of(face.indices.begin(),face.indices.end(),[&](unsigned id) {
      const auto& p = rig.vertices[id].position;
      if (p[1]>14.57 && (p[2]<1.03 || p[1]>15.39)) return true;
      return followsNeck && ((p[1]>13.40 && p[2]<.73 && std::abs(p[0])<1.15) ||
                             (p[1]>14.60 && p[2]>1.03));
    })) continue;
    const auto [a,b,c] = face.indices;
    scalp.addTriangle(positions[a],positions[b],positions[c]);
  }
  if (!scalp.faces.empty()) scalp.subdivide(0,unsigned(scalp.faces.size()));
  return scalp;
}

inline Vec3 scalpOffsetDirection(const FittingSurface& surface,Vec3 point,bool followsNeck) {
  // Long strands at the nape and face need depth clearance. Moving them
  // radially here can push the tips down into the shoulders or cheeks.
  if (followsNeck && point[1]<14.73 && point[2]<.73) return {0,0,-1};
  if (followsNeck && point[1]<15.46 && point[2]>1.03) return {0,0,1};
  return unit(sub(point,surface.center));
}

inline Vec3 clearScalp(const FittingSurface& surface,Vec3 point,double clearance,bool followsNeck=false) {
  if (followsNeck && point[1]>=13.39 && point[1]<14.73 && point[2]<.73 && std::abs(point[0])<1.30) {
    const Vec3 back{0,0,-1};
    const auto hit=surface.farthest(point,back,-clearance,.39,.23);
    return add(point,mul(back,hit+clearance));
  }
  if (followsNeck && point[1]>=14.61 && point[1]<15.46 && point[2]>1.03) {
    const Vec3 front{0,0,1};
    const auto hit=surface.farthest(point,front,-clearance,.34,.23);
    return add(point,mul(front,hit+clearance));
  }
  if (point[1]<14.53 || (point[1]<15.46 && point[2]>1.03)) return point;
  const auto delta=sub(point,surface.center);
  const auto distance=bb::length(delta);
  if (distance<.1 || distance>1.94) return point;
  const auto axis=mul(delta,1/distance);
  const auto scalp=surface.farthest(surface.center,axis,0,std::numeric_limits<double>::infinity());
  return scalp>0 && distance<scalp+clearance ? add(surface.center,mul(axis,scalp+clearance)) : point;
}

inline void fitHairFaces(std::vector<Vertex>& vertices,const Wardrobe& hair,
                        unsigned start,unsigned sourceOffset,unsigned largestSource,const FittingSurface& scalp,
                        bool followsNeck) {
  // A long hair card can still pass through the scalp when all its corners
  // are clear. Check its interior and lift the entire card smoothly.
  std::vector<double> lifts(largestSource+1);
  for (const auto& face : hair.triangles) {
    const auto a = vertices[start+face[0]].position;
    const auto b = vertices[start+face[1]].position;
    const auto c = vertices[start+face[2]].position;
    const auto ab = sub(a,b), bc = sub(b,c), ca = sub(c,a);
    const auto longest = std::max({dot(ab,ab),dot(bc,bc),dot(ca,ca)});
    if (longest<.005) continue;
    double lift = 0;
    for (const auto point : {mul(add(add(a,b),c),1./3),mul(add(a,b),.5),
                             mul(add(b,c),.5),mul(add(c,a),.5)}) {
      const auto temple=smoothBand(.64,.98,std::abs(point[0]))*
                        smoothBand(14.94,15.39,point[1])*(1-smoothBand(15.65,16.05,point[1]));
      const auto corrected = clearScalp(scalp,point,.027+.036*temple,followsNeck);
      lift = std::max(lift,bb::length(sub(corrected,point)));
    }
    if (lift<.006) continue;
    for (const auto corner : face) {
      auto& vertexLift = lifts[vertices[start+corner].source-sourceOffset];
      vertexLift = std::max(vertexLift,std::min(.28,1.35*lift));
    }
  }
  for (unsigned i=start; i<vertices.size(); ++i) {
    auto& vertex = vertices[i];
    const auto lift = lifts[vertex.source-sourceOffset];
    if (lift>0) vertex.position=add(vertex.position,mul(scalpOffsetDirection(scalp,vertex.position,followsNeck),lift));
  }
}

struct ClothingAxes {
  struct Limb { Vec3 root, middle, end; };
  std::array<Limb,2> arms, legs;
  Vec3 neckBase{}, neckTop{};

  explicit ClothingAxes(const Humanoid& rig) {
    const auto find = [&](std::string name) {
      const auto joint = std::find_if(rig.joints.begin(),rig.joints.end(),
                                      [&](const Joint& j) { return j.name==name; });
      if (joint==rig.joints.end()) throw std::runtime_error("Missing wardrobe joint: "+name);
      return joint->bindPosition;
    };
    for (unsigned side=0; side<arms.size(); ++side) {
      const std::string suffix = side ? ".L" : ".R";
      arms[side] = {find("upperarm01"+suffix),find("lowerarm01"+suffix),find("wrist"+suffix)};
      legs[side] = {find("upperleg01"+suffix),find("lowerleg01"+suffix),find("foot"+suffix)};
    }
    neckBase=find("neck01");
    neckTop=find("neck03");
  }

  std::vector<Vec3> armOffsets(const Humanoid& fitted) const {
    std::vector<Vec3> offsets(fitted.joints.size());
    const auto index = [&](std::string_view name) {
      return std::size_t(std::find_if(fitted.joints.begin(),fitted.joints.end(),
          [&](const auto& joint) { return joint.name==name; })-fitted.joints.begin());
    };
    for (const std::string suffix : {".L",".R"}) {
      const auto middle = index("lowerarm01"+suffix);
      const auto shoulder = fitted.joints.at(index("upperarm01"+suffix)).bindPosition;
      const auto elbow = fitted.joints.at(middle).bindPosition;
      const auto wrist = fitted.joints.at(index("wrist"+suffix)).bindPosition;
      const auto upper = sub(elbow,shoulder), lower = sub(wrist,elbow);
      const auto upperAxis = unit(upper), lowerAxis = unit(lower);
      const auto direction = unit(add(upperAxis,lowerAxis));
      constexpr double upperToLower = 1.145;
      const auto shift = (upperToLower*bb::length(lower)-bb::length(upper))/
                         std::max(.15,dot(direction,upperAxis)+upperToLower*dot(direction,lowerAxis));
      const auto elbowOffset = mul(direction,std::clamp(shift,0.,.45));
      // Bring the span closer to the standing height, preserving the elbow
      // ratio and moving the complete hand chain with the wrist.
      const auto shortening = mul(add(upper,elbowOffset),-.115);
      const auto wristOffset = add(shortening,mul(sub(lower,elbowOffset),-.13));
      offsets[middle] = add(elbowOffset,shortening);
      offsets.at(index("upperarm02"+suffix)) = mul(offsets[middle],.30);
      offsets.at(index("lowerarm02"+suffix)) = add(mul(offsets[middle],.51),mul(wristOffset,.49));
      const auto wristIndex = index("wrist"+suffix);
      offsets.at(wristIndex) = wristOffset;
      for (std::size_t i=wristIndex+1;i<fitted.joints.size();++i) {
        const auto& joint = fitted.joints[i];
        if (joint.name.rfind("finger",0)==0 && joint.name.size()>suffix.size() &&
            joint.name.compare(joint.name.size()-suffix.size(),suffix.size(),suffix)==0)
          offsets[i] = wristOffset;
      }
    }
    return offsets;
  }

  static Vec3 ease(const Vec3& base,Vec3 from,Vec3 to,double fullness) {
    const auto axis = sub(to,from);
    const auto along = std::clamp(dot(sub(base,from),axis)/dot(axis,axis),0.,1.);
    const auto radial = sub(base,add(from,mul(axis,along)));
    // Do not push the inner thigh or armpit through the other garment panel.
    return mul(radial,fullness/std::max(.46,bb::length(radial)));
  }

  Vec3 sleeveClearance(const Vec3& base,const Vec3& anchor) const {
    const auto width=std::abs(base[0]);
    const auto weight=smoothBand(1.74,2.32,width)*smoothBand(9.42,10.25,base[1]);
    if (weight==0) return {};
    const auto& arm=arms[base[0]>0 ? 1 : 0];
    const auto radial=[](const Vec3& point,const Vec3& from,const Vec3& to) {
      const auto axis=sub(to,from);
      const auto along=std::clamp(dot(sub(point,from),axis)/dot(axis,axis),0.,1.);
      return sub(point,add(from,mul(axis,along)));
    };
    const auto upper=radial(anchor,arm.root,arm.middle), lower=radial(anchor,arm.middle,arm.end);
    const auto outward=unit(dot(upper,upper)<dot(lower,lower) ? upper : lower);
    const auto clearance=.10-dot(sub(base,anchor),outward);
    return mul(outward,weight*std::clamp(clearance,0.,.19));
  }

  Vec3 collarClearance(const Vec3& base,const Vec3& anchor,double collarBase) const {
    const auto width=std::abs(base[0]);
    const auto weight=smoothBand(collarBase,collarBase+.61,base[1])*(1-smoothBand(14.16,14.66,base[1]))*
                      (1-smoothBand(.68,1.25,width));
    if (weight==0) return {};
    const auto axis=unit(sub(neckTop,neckBase));
    const auto delta=sub(anchor,neckBase);
    const auto outward=unit(sub(delta,mul(axis,dot(delta,axis))));
    const auto clearance=.12-dot(sub(base,anchor),outward);
    return mul(outward,weight*std::clamp(clearance,0.,.22));
  }

  Vec3 utilityEase(const Vec3& base) const {
    const auto [x,y,z] = base;
    const auto width = std::abs(x);
    const auto side = x>0 ? 1u : 0u;
    const auto sleeve = smoothBand(1.60,2.88,width);
    if (y>9.33) {
      const auto& arm = arms[side];
      const auto axis = sub(arm.end,arm.root);
      const auto along = dot(sub(base,arm.root),axis)/dot(axis,axis);
      const auto cuffs = 1-smoothBand(.82,.99,along);
      const auto cap = 1-smoothBand(.07,.21,along);
      const auto bicep = smoothBand(.10,.24,along)*(1-smoothBand(.36,.53,along));
      const auto elbow = smoothBand(.35,.51,along)*(1-smoothBand(.56,.72,along));
      const auto forearm = smoothBand(.60,.71,along)*(1-smoothBand(.78,.94,along));
      const auto sleeveEase = (.037-.119*cap+.135*bicep+.043*elbow+.074*forearm)*cuffs;
      const auto upper = ease(base,arm.root,arm.middle,sleeveEase);
      const auto lower = ease(base,arm.middle,arm.end,sleeveEase);
      auto armOffset = add(mul(upper,1-smoothBand(.39,.61,along)),
                           mul(lower,smoothBand(.39,.61,along)));
      // A woven sleeve hangs a little under the upper arm and draws in
      // at the cuff. Keep its elbow close to the hinge, even in profile.
      const auto nearCuff = smoothBand(.73,.93,along)*(1-smoothBand(.97,1.06,along));
      const auto taperedCuff = ease(base,arm.middle,arm.end,-.026*nearCuff);
      armOffset = add(armOffset,taperedCuff);
      armOffset[1] -= .217*bicep+.177*elbow+.090*forearm;
      armOffset[2] -= .022*elbow;
      const auto chest = smoothBand(10.32,11.86,y)*(1-smoothBand(13.17,13.93,y));
      const auto shoulder = smoothBand(12.74,13.78,y)*smoothBand(.78,1.82,width);
      const auto chestFront = smoothBand(.35,1.02,z);
      const auto collar = (1-smoothBand(.20,.57,width))*smoothBand(12.66,13.43,y);
      const auto collarEase = .054*collar*(1-smoothBand(13.92,14.15,y));
      const auto torso = Vec3{std::copysign(.047*chest-.033*shoulder,x)*smoothBand(.2,1.27,width),
                              -.123*shoulder,
                              (collarEase-.14*chest*(1-collar))*chestFront};
      return add(mul(torso,1-sleeve),mul(armOffset,sleeve));
    }
    const auto& leg = legs[side];
    const auto thigh = smoothBand(3.89,5.64,y);
    const auto ankle = smoothBand(.87,1.38,y)*(1-smoothBand(1.56,2.54,y));
    const auto knee = smoothBand(3.21,3.66,y)*(1-smoothBand(3.69,4.10,y));
    const auto upper = ease(base,leg.root,leg.middle,.032);
    const auto lower = ease(base,leg.middle,leg.end,.052+.045*ankle+.024*knee);
    const auto fit = smoothBand(.70,1.1,y)*(1-smoothBand(8.11,9.22,y));
    const auto offset = mul(add(mul(upper,thigh),mul(lower,1-thigh)),fit);
    const auto outer = smoothBand(1.05,1.67,width);
    const auto hips = smoothBand(7.56,8.32,y)*(1-smoothBand(8.55,9.11,y));
    return {offset[0]*outer-.054*std::copysign(outer*hips,x),offset[1]*.35,offset[2]*.65};
  }

  static Vec3 utilityBoot(const Vec3& base) {
    const auto [x,y,z] = base;
    const auto sole = smoothBand(-.01,.15,y)*(1-smoothBand(.40,1.02,y));
    const auto forefoot = smoothBand(.49,1.23,z)*(1-smoothBand(.55,1.03,y));
    const auto center = std::copysign(2.23,x);
    return {(.047*sole+.13*forefoot)*(x-center), .076*sole, .085*forefoot};
  }
};

inline Humanoid assemble(const Humanoid& rig, const Document& doc, const WardrobeReader& assets,
                         bool fitBodyRig = false) {
  Humanoid dressed = rig;
  auto points = morphBody(rig,doc);
  const auto* body = fitBodyRig && doc.bodyProfile!=0 ? rig.bodyShapes.get() : nullptr;
  if (body)
    for (std::size_t i=0; i<points.size(); ++i)
      points[i] = add(points[i],mul(body->vertices[rig.vertices[i].source],doc.bodyProfile));
  auto outfit = outfits[doc.outfit];
  if (!doc.customWardrobe.empty()) {
    outfit.clothes=doc.customWardrobe.c_str();
    outfit.utilityCut=false;
  }
  const auto hat = doc.headwear==2 ? "fedora01" : !doc.headwear && doc.hat ? outfit.hat : "";
  const auto& hairstyle = hairstyles[doc.hairstyle];
  const auto scalpEdge = *hat ? 0. : scalpHairEdge(hairstyle);
  const bool openCollar = doc.customWardrobe.empty() && (doc.outfit==1 || doc.outfit==4 || doc.outfit==7 || doc.outfit==8);
  const auto collarBase = std::string_view(outfit.clothes)=="male_casualsuit01" ? 12.85 : 13.25;
  const bool closedCasualShoulders = outfit.utilityCut || std::string_view(outfit.clothes)=="male_casualsuit03";
  std::vector<int> eyeSide(points.size());
  for (const auto& face : rig.triangles) if (face.material==1)
    for (const auto id : face.indices) eyeSide[id] = rig.vertices[id].position[0]>0 ? 1 : -1;
  for (std::size_t i = 0; i < points.size(); ++i) {
    auto& vertex = dressed.vertices[i];
    vertex.position = points[i];
    if (!eyeSide[i]) {
      const auto& position = rig.vertices[i].position;
      vertex.scalpMask = hairRootCoverage(position,doc,scalpEdge);
      vertex.color = headSurfaceColor(position,doc,vertex.scalpMask);
      vertex.lipMask = lipCoverage(position);
    }
  }
  // The bundled iris texture is frontal. Project the eyeball helpers from
  // their original position so the UV stays fixed through face morphs.
  for (std::size_t i=0; i<eyeSide.size(); ++i) {
    if (!eyeSide[i]) continue;
    const auto& pos = rig.vertices[i].position;
    auto& eye = dressed.vertices[i];
    const auto dx = pos[0]-.3078*eyeSide[i];
    const auto dy = pos[1]-15.452;
    const auto cornea = std::exp(-92*(dx*dx+dy*dy))*smoothBand(1.27,1.39,pos[2]);
    eye.position[2] += .02*cornea;
    eye.uv = {std::clamp(.5+dx*3.52,0.,1.),std::clamp(.5+(pos[1]-15.462)*3.52,0.,1.)};
  }
  const ClothingAxes garmentAxes(rig);
  const auto* axes = outfit.utilityCut ? &garmentAxes : nullptr;
  std::vector<bool> hide(65536);
  std::uint32_t sourceOffset = 65536;
  const std::string_view hairMesh=hairstyle.mesh;
  const bool followsNeck=!*hat && (hairMesh.rfind("bob",0)==0 || hairMesh.rfind("long",0)==0 ||
                                  hairMesh=="ponytail01");
  const auto scalp = *hairstyle.mesh || *hat ? fittedScalp(rig,points,followsNeck) : FittingSurface{};
  auto append = [&](const char* id, unsigned material, const Hairstyle* style=nullptr) {
    if (!*id) return;
    const auto& item = assets(id);
    const auto offset = std::uint32_t(dressed.vertices.size());
    for (auto source : item.hidden) hide[source] = true;
    std::uint32_t largestSource = 0;
    for (const auto& entry : item.vertices) {
      auto vertex = entry.vertex;
      largestSource = std::max(largestSource,vertex.source);
      vertex.source += sourceOffset;
      Vec3 anchor{}, delta{};
      for (unsigned i = 0; i < entry.anchors.size(); ++i) {
        if (!entry.factors[i]) continue;
        const auto base = entry.anchors[i];
        anchor = add(anchor,mul(base,entry.factors[i]));
        auto changed = add(morphPoint(doc,base),faceShapeDelta(rig,doc,entry.faceAnchors[i]));
        if (body) changed = add(changed,mul(body->at(base),doc.bodyProfile));
        delta = add(delta,mul(sub(changed,base),entry.factors[i]));
      }
      if (style) {
        auto offset = sub(vertex.position,anchor);
        const double offsetLength = bb::length(offset);
        const auto roots = std::clamp(offsetLength/.16,0.,1.);
        const auto front = std::clamp((anchor[2]-.84)/.5,0.,1.);
        const auto volume = std::clamp(style->volume+doc.hairVolume*.34,.25,1.3);
        const auto growth = std::clamp((15.43-anchor[1])/.85,0.,1.);
        const auto length = std::clamp(style->length+doc.hairLength*.41,.30,1.4);
        // Scale the distance from the original scalp attachment. Keep the
        // skin-facing roots in place while retaining the outer strand shape.
        const auto normal = unit(sub(anchor,{0,15.61,.34}));
        const auto lift = dot(offset,normal);
        const bool texturedLayers = style->texture==HairTexture::Straight ||
                                    style->texture==HairTexture::Wavy;
        if (anchor[1]>15.14) {
          const auto crown = smoothBand(15.81,16.18,anchor[1]);
          const auto strand = smoothBand(.012,.075,offsetLength);
          const auto clearance = (style->texture==HairTexture::Braided ? .20 : .078)*crown*strand +
                                 (texturedLayers && lift>0 ? .051*roots*front : 0.);
          offset = add(offset,mul(normal,std::max(0.,lift)*(volume-1)+clearance));
        }
        offset[1] *= 1 + (length-1) * growth;
        vertex.position = add(anchor,offset);
        vertex.position[0] += style->sweep*std::max(0.,lift)*roots;
        const auto edge = std::clamp(1-std::abs(anchor[1]-15.93)/.59,0.,1.);
        vertex.position[1] += (.17*doc.hairline+.017*std::sin(17*anchor[0])+
                               .009*std::sin(34*anchor[0]+1))*front*edge;

      }
      vertex.position = add(vertex.position,delta);
      if (material==3 && axes)
        vertex.position = add(vertex.position,axes->utilityEase(entry.vertex.position));
      if (material==3 && !axes)
        vertex.position = add(vertex.position,add(garmentAxes.sleeveClearance(entry.vertex.position,anchor),
                                                  garmentAxes.collarClearance(entry.vertex.position,anchor,collarBase)));
      if (material==4 && outfit.utilityCut && !doc.footwear)
        vertex.position = add(vertex.position,ClothingAxes::utilityBoot(entry.vertex.position));
      if (style || material==6) {
        const double crown = smoothBand(15.34,16.05,vertex.position[1]);
        const auto clearance = .016 + .041*crown;
        vertex.position = clearScalp(scalp,vertex.position,clearance,followsNeck);
      }
      dressed.vertices.push_back(vertex);
    }
    if (style || material==6)
      fitHairFaces(dressed.vertices,item,offset,sourceOffset,largestSource,scalp,followsNeck);
    sourceOffset += largestSource + 1;
    for (auto face : item.triangles) {
      for (auto& index : face) index += offset;
      dressed.triangles.push_back({face,material});
    }
  };
  append(outfit.clothes,3);
  constexpr const char* shoes[]{"","","shoes02","shoes03","shoes04"};
  static_assert(std::size(shoes)==std::size(footwearLabels));
  append(doc.footwear ? shoes[doc.footwear] : outfit.shoes,4);
  if (*hat) append(hat,6);
  else append(hairstyle.mesh,5,&hairstyle);
  const FaceSurface faceSurface(rig);
  if (const auto* brow = eyebrowMeshes[doc.eyebrow]; *brow)
    appendBrows(dressed,rig,points,faceSurface,assets(brow),sourceOffset);
  const auto lashStart = unsigned(dressed.vertices.size());
  append("eyelashes01",lashMaterial);
  attachLashes(dressed,rig,faceSurface,points,assets("eyelashes01"),lashStart);
  if (doc.beard) {
    const auto before = dressed.triangles.size();
    const auto jaw = std::find_if(rig.joints.begin(),rig.joints.end(),
                                 [](const auto& joint) { return joint.name == "jaw"; });
    if (jaw == rig.joints.end()) throw std::runtime_error("The character rig lacks a jaw joint.");
    const auto center = jaw->bindPosition;
    std::map<unsigned,unsigned> overlay;
    for (std::size_t i = 0; i < before; ++i) {
      auto face = dressed.triangles[i];
      if (face.material != 0) continue;
      auto chinHair = [&](unsigned id) {
        const auto& p = rig.vertices[id].position;
        const auto x = std::abs(p[0]);
        if (p[1] < center[1]-.68 || p[2] < center[2]+.45) return false;
        if (doc.beard == 1) return p[1] > 15.045 && p[1] < 15.17 && x < .37;
        if (doc.beard == 2) return p[1] < 14.91 && x < .32;
        return p[1] < 15.08 - .40*std::exp(-20*x*x) + .16*x && x < .73;
      };
      if (!std::all_of(face.indices.begin(),face.indices.end(),chinHair)) continue;
      for (auto& id : face.indices) {
        const auto [it,inserted] = overlay.emplace(id,unsigned(dressed.vertices.size()));
        if (inserted) {
          auto v = dressed.vertices[id];
          v.position = add(v.position,mul(unit(sub(v.position,center)),doc.beard == 3 ? .09 : .034));
          v.source += sourceOffset;
          dressed.vertices.push_back(v);
        }
        id = it->second;
      }
      face.material = 7;
      dressed.triangles.push_back(face);
    }
  }
  const auto covered = [&](unsigned index) {
    const auto& base = rig.vertices[index].position;
    const auto shoulderWidth = std::abs(base[0]);
    const auto collarWidth = fitBodyRig && doc.outfit==1 ?
        .58 + .30*smoothBand(13.74,14.10,base[1]) : .58;
    const bool collarSkin = openCollar && base[1]>13.62 && base[2]>.36 && shoulderWidth<collarWidth;
    // The casual shirt covers the shoulders outside the collar even where its
    // authoring mask ends early.
    const bool coveredShoulder = closedCasualShoulders && shoulderWidth>.62 &&
                                 shoulderWidth<2.25 && base[1]>13.02 && base[1]<14.12;
    const auto neckEdge = .42+.18*smoothBand(13.74,14.28,base[1]);
    const bool coveredCollar = outfit.utilityCut && shoulderWidth>neckEdge && shoulderWidth<1.52 &&
                               base[1]>13.34 && base[1]<14.30 && base[2]<.81;
    return !collarSkin && (coveredShoulder || coveredCollar || hide[dressed.vertices[index].source]);
  };
  if (fitBodyRig && !outfit.utilityCut && doc.customWardrobe.empty()) {
    // Leave each exposed corner in place and inset its covered neighbours.
    // Mixed triangles then stay connected under collars, cuffs and hems.
    std::vector<Vec3> coveredNormals(65536);
    std::vector<bool> mixedBoundary(65536);
    for (const auto& face : rig.triangles) {
      if (face.material!=0) continue;
      const auto [a,b,c]=face.indices;
      const auto normal=cross(sub(points[b],points[a]),sub(points[c],points[a]));
      const bool mixed=!std::all_of(face.indices.begin(),face.indices.end(),covered);
      for (unsigned index : face.indices) {
        if (!covered(index)) continue;
        const auto source=rig.vertices[index].source;
        coveredNormals[source]=add(coveredNormals[source],normal);
        mixedBoundary[source]=mixedBoundary[source] || mixed;
      }
    }
    for (unsigned i=0;i<rig.vertices.size();++i) {
      const auto source=rig.vertices[i].source;
      const auto& normal=coveredNormals[source];
      if (!mixedBoundary[source] || dot(normal,normal)<1e-10) continue;
      dressed.vertices[i].position=sub(dressed.vertices[i].position,mul(unit(normal),.10));
    }
  }
  dressed.triangles.erase(std::remove_if(dressed.triangles.begin(),dressed.triangles.end(),
      [&](const Triangle& face) {
        return face.material == 0 && std::all_of(face.indices.begin(),face.indices.end(),covered);
      }),dressed.triangles.end());
  std::vector<bool> used(dressed.vertices.size());
  for (const auto& face : dressed.triangles)
    for (const auto id : face.indices) used[id] = true;
  std::vector<std::uint32_t> remap(dressed.vertices.size());
  std::size_t count = 0;
  for (std::size_t i = 0; i < used.size(); ++i) if (used[i]) {
    remap[i] = std::uint32_t(count);
    dressed.vertices[count++] = dressed.vertices[i];
  }
  dressed.vertices.resize(count);
  for (auto& face : dressed.triangles)
    for (auto& index : face.indices) index = remap[index];
  const auto legRise = fitBodyRig ? .69*doc.values[Height] : 0.;
  for (std::size_t i=0; i<dressed.joints.size(); ++i) {
    auto& joint = dressed.joints[i];
    if (fitBodyRig) {
      // Keep articulations centred as the torso and limbs grow. Local surface
      // detail changes above the neck do not translate the head or eye rig.
      const auto bind = joint.bindPosition;
      for (std::size_t morph = 1; morph < bodyMorphCount; ++morph)
        if (doc.values[morph]) joint.bindPosition = add(joint.bindPosition,
                                                       mul(morphDelta(morphs[morph],bind),doc.values[morph]));
      if (body) joint.bindPosition = add(joint.bindPosition,mul(body->joints[i],doc.bodyProfile));
    }
  }
  const auto armOffset = axes ? axes->armOffsets(dressed) : std::vector<Vec3>{};
  const auto torsoShift = [](double height) { return .61*smoothBand(9.37,14.29,height); };
  std::vector<double> armShift(dressed.joints.size(),-1.);
  if (fitBodyRig && axes) {
    for (std::size_t i=0;i<dressed.joints.size();++i) {
      const auto& joint = dressed.joints[i];
      if (joint.name.rfind("upperarm01.",0)==0)
        armShift[i] = torsoShift(joint.bindPosition[1]);
      else if (joint.parent>=0)
        armShift[i] = armShift[joint.parent];
    }
  }
  const bool utilityFit = fitBodyRig && axes;
  const auto proportions = [](Vec3 point,double shift,bool utility,double legRise) {
    if (utility) {
      const auto y = point[1];
      const auto width = std::abs(point[0]);
      const auto knee = smoothBand(1.77,4.52,y)*(1-smoothBand(4.52,7.58,y));
      const auto shin = smoothBand(1.14,2.40,y)*(1-smoothBand(3.24,4.64,y));
      const auto thighLine = smoothBand(2.95,5.05,y)*(1-smoothBand(6.06,8.95,y));
      const auto calfLine = smoothBand(.54,1.88,y)*(1-smoothBand(3.0,5.20,y));
      const auto legSet = .158*(1-smoothBand(7.62,10.15,y))+.445*thighLine+.267*calfLine;
      const auto neck = smoothBand(13.92,14.78,y);
      const auto headWidth = .050+.052*(1-smoothBand(15.18,15.93,y));
      // The chin tapers into the neck below and stops at the lower lip above.
      const auto lowerJaw = smoothBand(13.98,14.30,y)*(1-smoothBand(14.32,14.88,y));
      const auto chin = lowerJaw*smoothBand(.68,1.37,point[2])*(1-smoothBand(.40,.76,width));
      const auto jawSides = lowerJaw*smoothBand(.23,.54,width)*smoothBand(.48,.90,point[2]);
      const auto neckSlope = (1-smoothBand(.48,2.27,width))*
                             smoothBand(12.98,13.85,y)*(1-smoothBand(14.03,14.66,y));
      const auto nape = smoothBand(13.57,14.06,y)*(1-smoothBand(14.17,14.58,y))*
                        (1-smoothBand(.42,.86,width))*(1-smoothBand(-.56,-.11,point[2]));
      point[0] += neck*(-headWidth*point[0]+.017)+std::copysign(.040*jawSides,point[0]);
      point[1] += .17*knee+.182*neckSlope+neck*.144*(16.58-y)+.226*chin;
      point[2] += .116*nape-.29*knee-.205*shin-legSet-shift-
                  neck*(.220+.104*(point[2]-.50))-.226*chin;
    }
    return adultProportions(point,legRise);
  };
  const auto fittedPosition = [&](Vec3 point,double shift) {
    auto fitted = proportions(point,shift,utilityFit,legRise);
    if (doc.faceTemplate==fieldTechnicianFaceTemplate && point[1]>13.9) {
      // Sample the same local deformation for every outfit. The utility
      // outfit uses the reference coordinates directly, while other outfits
      // keep their own global neck offset and scale.
      const auto reference = utilityFit ? fitted :
        proportions(point,torsoShift(point[1]),true,.69*doc.values[Height]);
      fitted = add(fitted,sub(fieldTechnicianFace(reference),reference));
    }
    return fitted;
  };
  for (auto& vertex : dressed.vertices) {
    const auto lean = torsoShift(vertex.position[1]);
    double shift = 0.;
    if (axes) {
      for (std::size_t i=0;i<vertex.weights.size();++i) {
        const auto joint = vertex.joints[i];
        const auto weight = vertex.weights[i];
        vertex.position = add(vertex.position,mul(armOffset[joint],weight));
        shift += weight*(armShift[joint]>=0 ? armShift[joint] : lean);
      }
    }
    vertex.position = fittedPosition(vertex.position,shift);
  }
  for (std::size_t i=0; i<dressed.joints.size(); ++i) {
    auto& joint = dressed.joints[i];
    if (axes) joint.bindPosition = add(joint.bindPosition,armOffset[i]);
    const auto shift = armShift[i]>=0 ? armShift[i] : torsoShift(joint.bindPosition[1]);
    joint.bindPosition = fittedPosition(joint.bindPosition,shift);
    joint.inverseBind = bb::translate(mul(joint.bindPosition,-1));
  }
  if (fitBodyRig) {
    if (outfit.utilityCut) {
      dressed.torso.upperArmPadding -= .012;
      dressed.torso.forearmPadding += .36;
    }
    // Skin and clothes contribute to the torso, but sleeves must not widen it.
    std::vector<bool> trunk;
    trunk.reserve(dressed.joints.size());
    for (const auto& joint : dressed.joints)
      trunk.push_back(joint.name=="root" || joint.name.rfind("spine",0)==0 ||
                      joint.name.rfind("pelvis",0)==0);
    for (const auto& vertex : dressed.vertices) {
      const auto& point = vertex.position;
      const auto level = (point[1]-TorsoProfile::low)/TorsoProfile::step;
      if (level < -.75 || level > dressed.torso.sections.size()-.25) continue;
      double weight = 0;
      for (std::size_t i=0; i<vertex.joints.size(); ++i)
        if (trunk[vertex.joints[i]]) weight += vertex.weights[i];
      if (weight<.64) continue;
      for (int i=int(std::floor(level)); i<=int(std::ceil(level)); ++i) {
        if (i<0 || i>=int(dressed.torso.sections.size())) continue;
        auto& section = dressed.torso.sections[i];
        section.width = std::max(section.width,std::abs(point[0]));
        section.back = std::min(section.back,point[2]);
        section.front = std::max(section.front,point[2]);
      }
    }
  }
  return dressed;
}
} // namespace ftebroom::character
