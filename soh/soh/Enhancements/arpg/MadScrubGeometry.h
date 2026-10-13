#pragma once
#include "GearGeometry.h"
#include "MadScrubBlenderData.h"
namespace ARPG::MadScrubGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,10> Limbs={0,4,7,10,13,14,18,19,23,24};
inline constexpr Color Palette[]={{100,112,57},{149,148,80},{39,48,23},{169,74,24},{229,149,43},{114,45,21},{255,143,21}};
// [TIME] | [INFO] | MadScrub.Materials | Explicit semantic slots keep source and frozen-runtime material APIs compatible without inferring roles from color.
enum class Surface { Wood, Plant, Glow };
inline constexpr std::array<Surface,7> Surfaces={Surface::Wood,Surface::Wood,Surface::Wood,Surface::Plant,Surface::Plant,Surface::Plant,Surface::Glow};
inline bool Visible(int limb){return std::find(Limbs.begin(),Limbs.end(),limb)!=Limbs.end();}
struct Vertex { GearGeometry::Vertex surface; int joint; };
inline std::vector<Vertex> MadScrub(int limb) {
 std::vector<Vertex> mesh;
 for(const auto& part:MadScrubBlenderData::parts)if(part.limb==limb)for(uint32_t i=part.first;i<part.first+part.count;++i){
  const auto& v=MadScrubBlenderData::vertices[MadScrubBlenderData::indices[i]];
  mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
 return mesh;
}
}
