#pragma once
#include "GearGeometry.h"
#include "DekuBabaBlenderData.h"

namespace ARPG::DekuBabaGeometry {
// Slots follow the semantic material names in the Blender exporter.
inline constexpr std::array<GearGeometry::Surface,11> Surfaces={GearGeometry::Surface::Skin,GearGeometry::Surface::Skin,GearGeometry::Surface::Flat,GearGeometry::Surface::Skin,GearGeometry::Surface::Flat,GearGeometry::Surface::Skin,GearGeometry::Surface::Bone,GearGeometry::Surface::Plant,GearGeometry::Surface::Plant,GearGeometry::Surface::Plant,GearGeometry::Surface::Wood};
using namespace GearGeometry;
inline constexpr std::array<int,7> Limbs={4,7,8,9,10,11,12};
inline constexpr std::array<std::array<Color,11>,2> Palettes={{
    {{{54,87,148},{85,131,175},{220,216,170},{191,168,93},{92,31,55},{172,72,91},{242,226,166},{68,108,51},{54,130,67},{147,178,69},{115,77,44}}},
    {{{58,58,112},{112,105,156},{230,188,119},{186,140,69},{77,25,42},{155,57,75},{242,217,144},{54,85,44},{44,102,55},{149,152,66},{99,62,38}}}
}};
inline const std::array<Color,11>& Colors(bool big){return Palettes[big];}
inline Mesh DekuBaba(int limb,bool big) {
    Mesh mesh;
    for(const auto& part:DekuBabaBlenderData::parts) {
        if(part.variant!=int(big) || part.limb!=limb)continue;
        for(uint32_t i=part.first;i<part.first+part.count;++i) {
            const auto& v=DekuBabaBlenderData::vertices[DekuBabaBlenderData::indices[i]];
            mesh.push_back({{float(v.x),float(v.y),float(v.z)},Paint(Colors(big)[part.material],Surfaces[part.material]),Unit({float(v.nx),float(v.ny),float(v.nz)})});
        }
    }
    return mesh;
}
}
