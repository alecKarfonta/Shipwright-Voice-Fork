#pragma once
#include "GearGeometry.h"
#include "IronKnuckleBlenderData.h"
namespace ARPG::IronKnuckleGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,24> Limbs={2,3,4,5,6,7,8,12,13,14,15,17,18,19,20,21,22,23,24,25,26,27,28,29};
inline constexpr Color Palette[3][8]={
 {{183,179,143},{236,233,208},{160,115,43},{64,64,57},{64,39,26},{110,36,30},{20,23,22},{104,152,143}},
 {{60,70,65},{134,145,130},{191,145,65},{49,55,50},{59,36,25},{87,39,35},{16,20,18},{99,151,129}},
 {{207,217,211},{240,244,232},{193,159,87},{68,77,71},{62,43,29},{73,49,83},{21,27,25},{143,171,185}}
};
inline int MatrixIndex(int limb){for(size_t i=0;i<Limbs.size();++i)if(Limbs[i]==limb)return int(i);return -1;}
struct Vertex { GearGeometry::Vertex surface; int joint; };
using Mesh=std::vector<Vertex>;
inline Mesh Knight(int limb,int variant) {
 Mesh mesh;if(variant<0||variant>2)return mesh;
 for(const auto& part:IronKnuckleBlenderData::parts)if(part.limb==limb)for(uint32_t i=part.first;i<part.first+part.count;++i){
  const auto& v=IronKnuckleBlenderData::vertices[IronKnuckleBlenderData::indices[i]];
  mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[variant][part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
 return mesh;
}
}
