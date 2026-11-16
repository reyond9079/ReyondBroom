// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/character/wardrobe.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace ftebroom::character;

static std::string bytes(const std::filesystem::path& path) {
  std::ifstream input(path,std::ios::binary);
  if (!input) throw std::runtime_error("Missing character fixture: "+path.string());
  return {std::istreambuf_iterator<char>(input),{}};
}

using Cell=std::array<int,3>;
using Face=std::array<Vec3,3>;

static std::optional<Vec3> cuts(const Face& subject,const Face& against) {
  const auto a=sub(against[1],against[0]), b=sub(against[2],against[0]);
  for (unsigned edge=0;edge<3;++edge) {
    const auto direction=sub(subject[(edge+1)%3],subject[edge]);
    const auto p=cross(direction,b);
    const auto determinant=dot(a,p);
    if (std::abs(determinant)<1e-9) continue;
    const auto from=sub(subject[edge],against[0]);
    const double u=dot(from,p)/determinant;
    if (u<.015 || u>.985) continue;
    const auto q=cross(from,a);
    const double v=dot(direction,q)/determinant;
    const double t=dot(b,q)/determinant;
    if (v>.015 && u+v<.985 && t>.015 && t<.985)
      return add(subject[edge],mul(direction,t));
  }
  return std::nullopt;
}

static std::pair<Cell,Cell> cells(const Face& face) {
  Cell low{},high{};
  for (unsigned axis=0;axis<3;++axis) {
    const auto [minimum,maximum]=std::minmax({face[0][axis],face[1][axis],face[2][axis]});
    low[axis]=int(std::floor(minimum*2));
    high[axis]=int(std::floor(maximum*2));
  }
  return {low,high};
}

template<class Fn>
static void eachCell(const Face& face,const Fn& callback) {
  const auto [lo,hi]=cells(face);
  for (int x=lo[0];x<=hi[0];++x)
    for (int y=lo[1];y<=hi[1];++y)
      for (int z=lo[2];z<=hi[2];++z)
        callback(Cell{x,y,z});
}

enum class Region { Torso, Arm, Legs, Feet, Head, Hair, Count };
using IntersectionCounts=std::array<unsigned,static_cast<unsigned>(Region::Count)>;
using ContactSample=std::function<void(unsigned,const Vec3&,const Vec3&,unsigned)>;

static Region region(const Humanoid& model,const Triangle& triangle) {
  if (triangle.material==5) return Region::Hair;
  const auto a=model.vertices.at(triangle.indices[0]).position;
  const auto b=model.vertices.at(triangle.indices[1]).position;
  const auto c=model.vertices.at(triangle.indices[2]).position;
  const auto point=mul(add(add(a,b),c),1./3);
  if (triangle.material==6 || point[1]>14.5) return Region::Head;
  if (triangle.material==4 || point[1]<1.4) return Region::Feet;
  if (std::abs(point[0])>1.65 && point[1]>8.6) return Region::Arm;
  return point[1]<8.6 ? Region::Legs : Region::Torso;
}

static IntersectionCounts intersections(const Humanoid& model,const AnimationPose& pose,
                                         const ContactSample& sample) {
  auto skin=skinMatrices(model,pose.rotations,pose.root);
  std::vector<Vec3> points;
  points.reserve(model.vertices.size());
  for (const auto& vertex : model.vertices) {
    const auto point=deform(vertex,vertex.position,skin);
    if (std::any_of(point.begin(),point.end(),[](double v){return !std::isfinite(v) || std::abs(v)>50;}))
      throw std::runtime_error("A garment pose produced invalid skinned coordinates.");
    points.push_back(point);
  }
  const auto corners=[&](const Triangle& face) {
    return Face{points.at(face.indices[0]),points.at(face.indices[1]),points.at(face.indices[2])};
  };
  FittingSurface depth;
  if (sample) {
    depth.faces.reserve(model.triangles.size());
    for (const auto& face : model.triangles) {
      // Cutout layers require their texture alpha. Treat them as opaque here
      // to report only views whose contact is definitely unobstructed.
      const auto [a,b,c]=corners(face);
      depth.addTriangle(a,b,c);
    }
    depth.subdivide(0,unsigned(depth.faces.size()));
  }
  std::vector<Face> body;
  std::vector<Vec3> bodyRest;
  std::map<Cell,std::vector<unsigned>> index;
  for (const auto& face : model.triangles) {
    if (face.material!=0) continue;
    const auto triangle=corners(face);
    eachCell(triangle,[&](const Cell& key){index[key].push_back(unsigned(body.size()));});
    body.push_back(triangle);
    const auto [a,b,c]=face.indices;
    bodyRest.push_back(mul(add(add(model.vertices[a].position,model.vertices[b].position),
                              model.vertices[c].position),1./3));
  }
  IntersectionCounts result{};
  std::vector<unsigned> seen(body.size());
  unsigned token=0;
  for (const auto& face : model.triangles) {
    if (face.material<3 || face.material>6) continue;
    const auto triangle=corners(face);
    ++token;
    bool touched=false;
    eachCell(triangle,[&](const Cell& key) {
      if (touched) return;
      const auto entry=index.find(key);
      if (entry==index.end()) return;
      for (const auto id : entry->second) {
        if (seen[id]==token) continue;
        seen[id]=token;
        auto contact=cuts(triangle,body[id]);
        if (!contact) contact=cuts(body[id],triangle);
        if (contact) {
          if (sample) {
            unsigned visible=0;
            constexpr Vec3 directions[]{{0,0,1},{1,0,0},{0,0,-1},{-1,0,0},{0,1,0}};
            for (unsigned view=0;view<std::size(directions);++view)
              if (depth.farthest(*contact,directions[view],.018,40)==.018) visible|=1u<<view;
            sample(face.material,*contact,bodyRest[id],visible);
          }
          touched=true;
          break;
        }
      }
    });
    if (touched) ++result[static_cast<unsigned>(region(model,face))];
  }
  return result;
}

int main(int argc,char** argv) try {
  if (argc!=3 && argc!=4) throw std::runtime_error("Pass the character asset directory, output CSV and optional contact CSV.");
  const std::filesystem::path root=argv[1];
  auto rig=readHumanoid(bytes(root/"humanoid-v1.rbh"));
  readFaceShapes(rig,bytes(root/"adult-face-shapes.rbf"));
  readBodyShapes(rig,bytes(root/"adult-body-shapes.rbs"));
  std::map<std::string,Wardrobe> wardrobe;
  for (const auto& entry : std::filesystem::directory_iterator(root/"wardrobe"))
    if (entry.path().extension()==".rbw")
      wardrobe.emplace(entry.path().stem().string(),readWardrobe(bytes(entry.path()),rig,entry.path().stem().string()));
  const auto lookup=[&](const auto& id)->const Wardrobe& { return wardrobe.at(id); };
  std::ofstream report(argv[2]);
  std::ofstream contacts;
  if (argc==4) {
    contacts.open(argv[3]);
    if (!contacts) throw std::runtime_error("Could not open the contact CSV.");
    contacts << "outfit,body,clip,phase,material,x,y,z,skin_x,skin_y,skin_z,visible_views\n";
  }
  report << "outfit,body,clip,phase,triangles,torso,arms,legs,feet,headwear,hair\n";
  const char* names[]{"neutral","slim","heavy","muscular","small-frame"};
  constexpr std::pair<Motion,double> poses[]{
    {Motion::Neutral,.4},{Motion::Walk,.25},{Motion::Walk,.75},{Motion::Run,.32},
    {Motion::Crouch,.45},{Motion::Wave,.55},{Motion::JumpLoop,.52},{Motion::DeathBack,.78},
  };
  unsigned cases=0, withIntersection=0;
  for (unsigned outfit=0;outfit<10;++outfit) {
    for (unsigned variant=0;variant<std::size(names);++variant) {
      Document doc;
      doc.outfit=outfit;
      doc.hairstyle=(outfit%9)+1;
      doc.bodyProfile=variant%2 ? -.78 : .78;
      doc.values[BodyFat]=variant==1 ? -.65 : variant==2 ? .80 : 0;
      doc.values[Muscle]=variant==3 ? .85 : .05;
      doc.values[ShoulderWidth]=variant==3 ? .72 : variant==4 ? -.56 : .12;
      const auto fitted=assemble(rig,doc,lookup,true);
      Animator animator(fitted,true);
      for (const auto& [motion,phase] : poses) {
        auto clip=*std::find_if(std::begin(clips),std::end(clips),[&](const auto& c){return c.motion==motion;});
        clip=captureTiming(clip,true);
        ContactSample contact;
        if (contacts.is_open()) contact=[&](unsigned material,const Vec3& p,const Vec3& skin,unsigned visible) {
          contacts << outfits[outfit].id << ',' << names[variant] << ',' << clip.id << ',' << phase << ',' << material;
          for (const auto coordinate : p) contacts << ',' << coordinate;
          for (const auto coordinate : skin) contacts << ',' << coordinate;
          contacts << ',' << visible << '\n';
        };
        const auto hit=intersections(fitted,animator.sample(clip,phase*clip.seconds),contact);
        ++cases;
        withIntersection+=std::any_of(hit.begin(),hit.end(),[](unsigned count){return count!=0;});
        report << outfits[outfit].id << ',' << names[variant] << ',' << clip.id << ',' << phase << ','
               << fitted.triangles.size();
        for (const auto count : hit) report << ',' << count;
        report << '\n';
      }
    }
  }
  if (!report || (contacts.is_open() && !contacts) || cases!=400)
    throw std::runtime_error("Could not complete the full garment fit matrix.");
  std::cout << "Ran " << cases << " garment/body/pose fits; " << withIntersection
            << " have mesh intersections requiring inspection.\n";
  return withIntersection==0 ? 0 : 1;
} catch (const std::exception& error) {
  std::cerr << error.what() << '\n';
  return 2;
}
