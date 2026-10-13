#pragma once
#include "GearGeometry.h"
#include "PoeBlenderData.h"
namespace ARPG::PoeGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,9> Limbs={4,5,9,10,11,16,18,19,20};
inline constexpr Color Palette[]={{151,162,129},{196,203,168},{31,32,42},{92,110,81},{126,143,97},{176,186,129},{103,71,32},{183,141,65},{255,235,161}};
// [TIME] | [INFO] | Poe.Materials | Explicit semantic slots keep source and frozen-runtime material APIs compatible without inferring roles from color.
enum class Surface { Cloth, Skin, Metal, Glow };
inline constexpr std::array<Surface,9> Surfaces={Surface::Cloth,Surface::Cloth,Surface::Cloth,Surface::Cloth,Surface::Skin,Surface::Skin,Surface::Metal,Surface::Metal,Surface::Glow};
inline Color Tint(int material,uint8_t r,uint8_t g,uint8_t b){auto c=Palette[material];return {uint8_t(c.r*unsigned(r)/255),uint8_t(c.g*unsigned(g)/255),uint8_t(c.b*unsigned(b)/255)};}
inline bool Visible(int limb){return std::find(Limbs.begin(),Limbs.end(),limb)!=Limbs.end();}
struct Vertex { GearGeometry::Vertex surface; int joint; };
inline std::vector<Vertex> Poe(int limb) {
 std::vector<Vertex> mesh;
 for(const auto& part:PoeBlenderData::parts)if(part.limb==limb)for(uint32_t i=part.first;i<part.first+part.count;++i){
  const auto& v=PoeBlenderData::vertices[PoeBlenderData::indices[i]];
  mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
 return mesh;
}
}
