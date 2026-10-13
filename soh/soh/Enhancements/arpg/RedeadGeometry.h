#pragma once
#include "GearGeometry.h"
#include "RedeadBlenderData.h"
namespace ARPG::RedeadGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,15> Limbs={3,4,6,8,9,11,14,16,17,18,20,21,22,24,25};
inline constexpr Color Palette[2][9]={
    {{109,78,60},{155,111,74},{139,91,50},{27,25,22},{161,133,93},{198,168,118},{216,139,65},{86,67,45},{72,57,43}},
    {{115,109,82},{145,134,98},{156,146,110},{28,31,26},{178,166,121},{211,196,149},{231,189,71},{102,91,59},{107,101,72}}
};
inline int MatrixIndex(int limb){for(size_t i=0;i<Limbs.size();++i)if(Limbs[i]==limb)return int(i);return -1;}
struct Vertex { GearGeometry::Vertex surface; int joint; };
using Mesh=std::vector<Vertex>;
inline Mesh Undead(int limb,int variant) {
    Mesh mesh;if(variant<0 || variant>1)return mesh;
    for(const auto& part:RedeadBlenderData::parts)if(part.limb==limb && part.variant==variant) {
        for(uint32_t i=part.first;i<part.first+part.count;++i){const auto& v=RedeadBlenderData::vertices[RedeadBlenderData::indices[i]];
            mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[variant][part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
    }
    return mesh;
}
}
