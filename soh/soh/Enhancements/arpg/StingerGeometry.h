#pragma once
#include "GearGeometry.h"
#include "StingerBlenderData.h"
namespace ARPG::StingerGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,11> Limbs={3,4,5,6,9,10,12,14,15,17,18};
inline constexpr Color Palette[2][7]={
 {{59,104,100},{159,169,128},{181,178,140},{106,56,46},{15,27,29},{213,165,69},{17,23,27}},
 {{52,83,131},{122,157,173},{191,196,177},{95,65,87},{12,20,31},{220,176,71},{16,20,26}}
};
inline int MatrixIndex(int limb){for(size_t i=0;i<Limbs.size();++i)if(Limbs[i]==limb)return int(i);return -1;}
struct Vertex { GearGeometry::Vertex surface; int joint; };
using Mesh=std::vector<Vertex>;
inline Mesh Stinger(int limb,int variant) {
 Mesh mesh;if(variant<0||variant>1)return mesh;
 for(const auto& part:StingerBlenderData::parts)if(part.limb==limb)for(uint32_t i=part.first;i<part.first+part.count;++i){
  const auto& v=StingerBlenderData::vertices[StingerBlenderData::indices[i]];
  mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[variant][part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
 return mesh;
}
}
