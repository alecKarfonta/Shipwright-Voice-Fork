#pragma once
#include "GearGeometry.h"
#include "StalchildBlenderData.h"
namespace ARPG::StalchildGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,17> Limbs={2,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
inline constexpr Color Palette[]={{172,152,112},{209,191,153},{33,28,25},{223,205,163},{255,78,22},{79,66,48}};
inline bool Visible(int limb){return std::find(Limbs.begin(),Limbs.end(),limb)!=Limbs.end();}
struct Vertex { GearGeometry::Vertex surface; int joint; };
inline std::vector<Vertex> Stalchild(int limb) {
 std::vector<Vertex> mesh;
 for(const auto& part:StalchildBlenderData::parts)if(part.limb==limb)for(uint32_t i=part.first;i<part.first+part.count;++i){
  const auto& v=StalchildBlenderData::vertices[StalchildBlenderData::indices[i]];
  mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
 return mesh;
}
}
