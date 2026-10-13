#pragma once
#include "GearGeometry.h"
#include "DodongoBlenderData.h"

namespace ARPG::DodongoGeometry {
using namespace GearGeometry;
inline constexpr std::array<int,12> Limbs={6,7,13,14,15,16,21,22,23,28,29,30};
inline constexpr std::array<Color,8> Palette={{{107,125,79},{144,151,93},{178,137,71},{62,73,48},{226,207,154},{228,156,45},{25,26,18},{67,37,36}}};
inline Mesh Dodongo(int limb) {
    Mesh mesh;
    for(const auto& part:DodongoBlenderData::parts) {
        if(part.limb!=limb)continue;
        for(uint32_t i=part.first;i<part.first+part.count;++i) {
            const auto& v=DodongoBlenderData::vertices[DodongoBlenderData::indices[i]];
            mesh.push_back({{float(v.x),float(v.y),float(v.z)},Palette[part.material],Unit({float(v.nx),float(v.ny),float(v.nz)})});
        }
    }
    return mesh;
}
}
