// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace al {
struct Vec3 { double x=0,y=0,z=0; double& operator[](int a){return a==0?x:a==1?y:z;} double operator[](int a)const{return a==0?x:a==1?y:z;} };
inline Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator*(Vec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
inline Vec3 operator/(Vec3 a,double s){return a*(1/s);}
inline Vec3 mul(Vec3 a,Vec3 b){return {a.x*b.x,a.y*b.y,a.z*b.z};}
inline Vec3 div(Vec3 a,Vec3 b){return {a.x/b.x,a.y/b.y,a.z/b.z};}
inline double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double length(Vec3 a){return std::sqrt(dot(a,a));}
inline Vec3 unit(Vec3 a){double n=length(a);if(n<1e-12)throw std::runtime_error("degenerate vector");return a/n;}
inline bool near(Vec3 a,Vec3 b,double e=1e-6){return length(a-b)<=e;}
struct Int3 {int x=0,y=0,z=0;int& operator[](int a){return a==0?x:a==1?y:z;}int operator[](int a)const{return a==0?x:a==1?y:z;}};
inline bool operator==(Int3 a,Int3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
inline bool operator!=(Int3 a,Int3 b){return !(a==b);}
inline Int3 operator+(Int3 a,Int3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Int3 operator-(Int3 a,Int3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Int3 operator*(Int3 a,int b){return {a.x*b,a.y*b,a.z*b};}
inline Vec3 vec(Int3 p){return {double(p.x),double(p.y),double(p.z)};}
inline size_t volume(Int3 s){if(s.x<0||s.y<0||s.z<0||s.x>32768||s.y>32768||s.z>32768)throw std::runtime_error("invalid dimensions");return size_t(s.x)*s.y*s.z;}
inline bool inside(Int3 p,Int3 s){return p.x>=0&&p.y>=0&&p.z>=0&&p.x<s.x&&p.y<s.y&&p.z<s.z;}
inline size_t index(Int3 p,Int3 s){return size_t(p.x)+size_t(p.y)*s.x+size_t(p.z)*s.x*s.y;}
inline Int3 position(size_t i,Int3 s){return {int(i%s.x),int(i/s.x%s.y),int(i/(s.x*s.y))};}
struct Bounds3i {Int3 min{},max{1,1,1};Int3 size()const{return max-min;}bool contains(Int3 p)const{return inside(p-min,size());}};
inline constexpr std::array<Int3,6> delta{{{-1,0,0},{0,0,-1},{0,-1,0},{1,0,0},{0,0,1},{0,1,0}}};
inline int opposite(int d){return (d+3)%6;}
inline constexpr int inspectorSides[6]={3,0,4,1,5,2};
inline constexpr const char* sideNames[6]={"Left","Down","Back","Right","Up","Front"};
enum class BlockAction {RotateX,RotateY,RotateZ,MirrorX,MirrorY,MirrorZ,Flip};
inline constexpr const char* actionNames[7]={"RotateX","RotateY","RotateZ","MirrorX","MirrorY","MirrorZ","Flip"};
inline constexpr int faceMap[7][6]={{0,5,1,3,2,4},{4,0,2,1,3,5},{2,1,3,5,4,0},{3,1,2,0,4,5},{0,1,5,3,4,2},{0,4,2,3,1,5},{0,1,2,3,4,5}};
inline Vec3 linear(Vec3 p,BlockAction a){switch(a){case BlockAction::RotateX:return {p.x,-p.z,p.y};case BlockAction::RotateY:return {p.z,p.y,-p.x};case BlockAction::RotateZ:return {-p.y,p.x,p.z};case BlockAction::MirrorX:return {-p.x,p.y,p.z};case BlockAction::MirrorY:return {p.x,-p.y,p.z};case BlockAction::MirrorZ:return {p.x,p.y,-p.z};default:return p;}}
inline int determinant(BlockAction a){return a>=BlockAction::MirrorX&&a<=BlockAction::MirrorZ?-1:1;}
inline int transformFace(int d,const std::vector<BlockAction>& actions){for(auto a:actions)d=faceMap[int(a)][d];return d;}
inline Vec3 transformPoint(Vec3 p,Vec3 cell,const std::vector<BlockAction>& actions){p=div(p,cell)-Vec3{.5,.5,.5};for(auto a:actions)p=linear(p,a);return mul(p+Vec3{.5,.5,.5},cell);}
// Covectors use A^-T. For A = D M D^-1 and orthogonal M this is D^-1 M D.
inline Vec3 transformCovector(Vec3 v,Vec3 cell,const std::vector<BlockAction>& actions){v=mul(v,cell);for(auto a:actions)v=linear(v,a);return div(v,cell);}
}
