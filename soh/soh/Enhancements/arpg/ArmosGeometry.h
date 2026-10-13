#pragma once
#include "GearGeometry.h"
#include "ArmosBlenderData.h"
namespace ARPG::ArmosGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,4> Limbs={4,7,10,13};
inline constexpr Color Palette[2][7]={
 {{113,116,102},{173,170,141},{38,43,39},{87,96,78},{95,93,88},{89,84,67},{52,59,51}},
 {{127,104,104},{188,165,137},{37,26,37},{104,74,85},{117,103,96},{236,105,56},{64,42,54}}
};
inline Color Blend(int material,uint8_t awake) {
 if(material<0||material>=7)return {0,0,0};
 const auto a=Palette[0][material],b=Palette[1][material];
 auto mix=[&](int x,int y){return uint8_t((x*(255-awake)+y*awake+127)/255);};
 return {mix(a.r,b.r),mix(a.g,b.g),mix(a.b,b.b)};
}
inline int MatrixIndex(int limb){for(size_t i=0;i<Limbs.size();++i)if(Limbs[i]==limb)return int(i);return -1;}
struct Vertex { GearGeometry::Vertex surface; int joint; };
using Mesh=std::vector<Vertex>;
inline Mesh Armos(int limb,int variant=0) {
 Mesh mesh;if(variant<0||variant>1)return mesh;
 for(const auto& part:ArmosBlenderData::parts)if(part.limb==limb)for(uint32_t i=part.first;i<part.first+part.count;++i){
  const auto& v=ArmosBlenderData::vertices[ArmosBlenderData::indices[i]];
  mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[variant][part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
 return mesh;
}
}
