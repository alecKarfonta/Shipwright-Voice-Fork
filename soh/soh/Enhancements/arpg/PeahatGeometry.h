#pragma once
#include "GearGeometry.h"
#include "PeahatBlenderData.h"
namespace ARPG::PeahatGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,8> Limbs={3,7,10,13,16,19,22,23};
inline constexpr int WeakPoint=24;
inline constexpr Color Palette[2][7]={
 {{118,77,41},{180,139,79},{78,112,48},{151,170,83},{151,88,50},{223,152,73},{52,43,27}},
 {{137,99,54},{190,158,89},{111,135,64},{179,190,109},{163,112,62},{234,171,86},{63,56,32}}
};
inline int MatrixIndex(int limb){for(size_t i=0;i<Limbs.size();++i)if(Limbs[i]==limb)return int(i);return -1;}
struct Vertex { GearGeometry::Vertex surface; int joint; };
using Mesh=std::vector<Vertex>;
inline Mesh Peahat(int limb,int variant=0) {
 Mesh mesh;if(variant<0||variant>1||(variant==1&&limb==WeakPoint))return mesh;
 for(const auto& part:PeahatBlenderData::parts)if(part.limb==limb)for(uint32_t i=part.first;i<part.first+part.count;++i){
  const auto& v=PeahatBlenderData::vertices[PeahatBlenderData::indices[i]];
  mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[variant][part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
 return mesh;
}
}
