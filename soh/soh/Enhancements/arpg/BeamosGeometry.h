#pragma once
#include "GearGeometry.h"
#include "BeamosBlenderData.h"
namespace ARPG::BeamosGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,4> Limbs={5,8,9,10};
inline constexpr Color Palette[]={{117,119,107},{166,165,139},{31,35,32},{145,120,72},{83,94,95},{245,72,32},{59,66,58}};
// [TIME] | [INFO] | Beamos.Materials | Explicit semantic slots keep source and frozen-runtime material APIs compatible without inferring roles from color.
enum class Surface { Stone, Metal, Glow };
inline constexpr std::array<Surface,7> Surfaces={Surface::Stone,Surface::Stone,Surface::Metal,Surface::Metal,Surface::Metal,Surface::Glow,Surface::Stone};
inline bool Visible(int limb){return std::find(Limbs.begin(),Limbs.end(),limb)!=Limbs.end();}
struct Vertex { GearGeometry::Vertex surface; int joint; };
inline std::vector<Vertex> Beamos(int limb) {
 std::vector<Vertex> mesh;
 for(const auto& part:BeamosBlenderData::parts)if(part.limb==limb)for(uint32_t i=part.first;i<part.first+part.count;++i){
  const auto& v=BeamosBlenderData::vertices[BeamosBlenderData::indices[i]];
  mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
 return mesh;
}
}
