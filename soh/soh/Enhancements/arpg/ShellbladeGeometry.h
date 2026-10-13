#pragma once
#include "GearGeometry.h"
#include "ShellbladeBlenderData.h"
namespace ARPG::ShellbladeGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,4> Limbs={4,6,7,8};
inline constexpr Color Palette[1][7]={
 {{121,119,104},{187,178,136},{207,181,151},{77,75,67},{112,45,70},{230,133,90},{33,17,29}}
};
inline int MatrixIndex(int limb){for(size_t i=0;i<Limbs.size();++i)if(Limbs[i]==limb)return int(i);return -1;}
struct Vertex { GearGeometry::Vertex surface; int joint; };
using Mesh=std::vector<Vertex>;
inline Mesh Shellblade(int limb,int variant=0) {
 Mesh mesh;if(variant<0||variant>0)return mesh;
 for(const auto& part:ShellbladeBlenderData::parts)if(part.limb==limb)for(uint32_t i=part.first;i<part.first+part.count;++i){
  const auto& v=ShellbladeBlenderData::vertices[ShellbladeBlenderData::indices[i]];
  mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[variant][part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
 return mesh;
}
}
