#pragma once
#include "GearGeometry.h"
#include "OctorokBlenderData.h"

namespace ARPG::OctorokGeometry {
// Slots follow the semantic material names in the Blender exporter.
inline constexpr std::array<GearGeometry::Surface,10> Surfaces={GearGeometry::Surface::Skin,GearGeometry::Surface::Skin,GearGeometry::Surface::Skin,GearGeometry::Surface::Skin,GearGeometry::Surface::Skin,GearGeometry::Surface::Flat,GearGeometry::Surface::Flat,GearGeometry::Surface::Flat,GearGeometry::Surface::Flat,GearGeometry::Surface::Flat};
using namespace GearGeometry;
inline constexpr std::array<int,15> Limbs={5,8,13,14,15,20,21,22,27,28,29,34,35,36,37};
inline constexpr std::array<Color,10> Palette={{{177,190,170},{212,218,188},{87,115,99},{138,66,58},{213,162,129},{49,29,36},{238,209,126},{192,97,37},{27,24,25},{255,244,207}}};
inline Mesh Octorok(int limb) {
    Mesh mesh;
    for(const auto& part:OctorokBlenderData::parts) {
        if(part.limb!=limb)continue;
        for(uint32_t i=part.first;i<part.first+part.count;++i) {
            const auto& v=OctorokBlenderData::vertices[OctorokBlenderData::indices[i]];
            mesh.push_back({{float(v.x),float(v.y),float(v.z)},Paint(Palette[part.material],Surfaces[part.material]),Unit({float(v.nx),float(v.ny),float(v.nz)})});
        }
    }
    return mesh;
}
}
