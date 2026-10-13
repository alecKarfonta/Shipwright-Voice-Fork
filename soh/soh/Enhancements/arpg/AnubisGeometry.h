#pragma once
#include "GearGeometry.h"
#include "AnubisBlenderData.h"
namespace ARPG::AnubisGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,8> Limbs={3,5,6,8,9,11,13,15};
inline constexpr Color Palette[]={{177,153,109},{215,192,143},{68,57,40},{80,59,39},{132,103,62},{184,139,58},{46,72,106},{61,189,207}};
// [TIME] | [INFO] | Anubis.Materials | Explicit semantic slots keep source and frozen-runtime material APIs compatible without inferring roles from color.
enum class Surface { Cloth, Wood, Metal, Gem };
inline constexpr std::array<Surface,8> Surfaces={Surface::Cloth,Surface::Cloth,Surface::Cloth,Surface::Wood,Surface::Wood,Surface::Metal,Surface::Metal,Surface::Gem};
inline bool Visible(int limb){return std::find(Limbs.begin(),Limbs.end(),limb)!=Limbs.end();}
struct Vertex { GearGeometry::Vertex surface; int joint; };
inline std::vector<Vertex> Anubis(int limb) {
 std::vector<Vertex> mesh;
 for(const auto& part:AnubisBlenderData::parts)if(part.limb==limb)for(uint32_t i=part.first;i<part.first+part.count;++i){
  const auto& v=AnubisBlenderData::vertices[AnubisBlenderData::indices[i]];
  mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
 return mesh;
}
}
