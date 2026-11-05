// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include "al_model.h"
namespace al {
struct Face {std::array<Vec3,3> points;std::string material="fb_grey";Vec3 u{1,0,0},v{0,1,0};double offsetU=0,offsetV=0,scaleU=1,scaleV=1,rotation=0;std::string attributes;Vec3 normal()const{return unit(cross(points[2]-points[0],points[1]-points[0]));}double distance()const{return dot(normal(),points[0]);}};
struct Brush {std::vector<Face> faces;};
struct PointEntity {std::map<std::string,std::string> fields;};
struct Geometry {std::vector<Brush> brushes;std::vector<PointEntity> entities;};
struct Polyhedron {std::vector<Vec3> vertices;std::vector<std::vector<Vec3>> polygons;Vec3 min,max;double volume=0;};
Geometry parseMap(const std::string&);std::string writeMap(const Geometry&);std::string writeBrush(const Brush&);std::string writeEntity(const PointEntity&);
Polyhedron polyhedron(const Brush&);void validateBrush(const Brush&);Brush box(Vec3 lo,Vec3 hi,const std::string& material);
Geometry transformGeometry(const Geometry&,Vec3 cell,const std::vector<BlockAction>&,Vec3 translation={});
Geometry translateGeometry(const Geometry&,Vec3 translation);Geometry rescaleGeometry(const Geometry&,Vec3 factors);
bool clipBrush(const Brush&,Vec3 normal,double distance,Brush&);Geometry clipGeometry(const Geometry&,Vec3 lo,Vec3 hi);
uint8_t generateFill(const Geometry&,Vec3 cell);bool contains(const Brush&,Vec3,double epsilon=.01);
}
