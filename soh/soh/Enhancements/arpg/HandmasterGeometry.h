#pragma once
#include "GearGeometry.h"
#include "HandmasterBlenderData.h"
namespace ARPG::HandmasterGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,14> Limbs={2,4,6,7,9,11,12,14,16,17,19,21,23,24};
inline constexpr Color Palette[2][8]={
 {{118,107,135},{129,117,144},{133,122,148},{61,49,72},{177,159,115},{109,91,72},{222,204,164},{96,61,79}},
 {{111,124,93},{123,134,102},{129,141,108},{52,63,43},{179,165,113},{102,94,65},{222,206,165},{99,73,60}}
};
inline int MatrixIndex(int limb,int variant){if(variant<0||variant>1)return -1;for(size_t i=0;i<Limbs.size();++i)if(Limbs[i]==limb)return int(i);return -1;}
struct Vertex { GearGeometry::Vertex surface; int joint; };
using Mesh=std::vector<Vertex>;
inline Mesh Handmaster(int limb,int variant) {
    Mesh mesh;if(variant<0 || variant>1)return mesh;
    for(const auto& part:HandmasterBlenderData::parts)if(part.limb==limb && part.variant==variant) {
        for(uint32_t i=part.first;i<part.first+part.count;++i){const auto& v=HandmasterBlenderData::vertices[HandmasterBlenderData::indices[i]];
            mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Palette[variant][part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint});}
    }
    return mesh;
}
}
