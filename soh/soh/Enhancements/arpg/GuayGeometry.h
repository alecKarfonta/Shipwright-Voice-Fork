#pragma once
#include "GearGeometry.h"
#include "GuayBlenderData.h"
namespace ARPG::GuayGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,7> Limbs={2,3,4,5,6,7,8};
inline constexpr Color Palette[]={{35,28,47},{59,38,78},{44,30,61},{85,60,110},{136,94,45},{195,149,68},{239,98,21},{14,8,15}};
enum class Surface { Feather, Bone, Eye };
inline constexpr std::array<Surface,8> Surfaces={Surface::Feather,Surface::Feather,Surface::Feather,Surface::Feather,Surface::Bone,Surface::Bone,Surface::Eye,Surface::Eye};
inline bool Visible(int limb){return std::find(Limbs.begin(),Limbs.end(),limb)!=Limbs.end();}
inline int MatrixIndex(int limb){return Visible(limb)?limb-2:-1;}
struct Vertex { GearGeometry::Vertex surface;int joint; };
inline std::vector<Vertex> Guay(int limb) {
 std::vector<Vertex> result;
 for(const auto& part:GuayBlenderData::parts)if(part.limb==limb)for(uint32_t i=part.first;i<part.first+part.count;++i){const auto& v=GuayBlenderData::vertices[GuayBlenderData::indices[i]];
  result.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
 return result;
}
}
