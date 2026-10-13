#pragma once
#include "GearGeometry.h"
#include "MoblinBlenderData.h"
namespace ARPG::MoblinGeometry {
using namespace GearGeometry;
inline constexpr std::array<std::array<int,15>,2> Limbs={{{3,6,7,9,11,12,14,16,17,20,21,22,25,26,27},{3,5,7,9,11,12,14,16,17,20,21,22,25,26,27}}};
inline constexpr Color Palette[2][10]={
    {{145,114,66},{183,143,80},{75,88,82},{153,151,119},{78,47,30},{79,63,42},{29,24,20},{214,198,143},{209,153,46},{109,70,36}},
    {{156,98,65},{194,130,81},{66,73,72},{146,140,115},{66,37,24},{87,46,36},{31,22,18},{224,201,146},{227,162,48},{102,59,30}}
};
inline int MatrixIndex(int limb,int variant){if(variant<0||variant>1)return -1;for(size_t i=0;i<Limbs[variant].size();++i)if(Limbs[variant][i]==limb)return int(i);return -1;}
struct Vertex { GearGeometry::Vertex surface; int joint; };
using Mesh=std::vector<Vertex>;
inline Mesh Moblin(int limb,int variant) {
    Mesh mesh;if(variant<0 || variant>1)return mesh;
    for(const auto& part:MoblinBlenderData::parts)if(part.limb==limb && part.variant==variant) {
        for(uint32_t i=part.first;i<part.first+part.count;++i){const auto& v=MoblinBlenderData::vertices[MoblinBlenderData::indices[i]];
            mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[variant][part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
    }
    return mesh;
}
}
