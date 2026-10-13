#pragma once
#include "GearGeometry.h"
#include "SkulltulaBlenderData.h"

namespace ARPG::SkulltulaGeometry {
// Slots follow the semantic material names in the Blender exporter.
inline constexpr std::array<GearGeometry::Surface,8> Surfaces={GearGeometry::Surface::Bone,GearGeometry::Surface::Bone,GearGeometry::Surface::Flat,GearGeometry::Surface::Skin,GearGeometry::Surface::Skin,GearGeometry::Surface::Skin,GearGeometry::Surface::Bone,GearGeometry::Surface::Bone};
using namespace GearGeometry;
inline constexpr std::array<int,10> Limbs={4,5,8,11,14,17,20,23,26,29};
inline constexpr std::array<Color,8> Palette={{{210,199,165},{240,224,188},{37,29,24},{159,78,43},{98,45,31},{111,75,43},{197,137,69},{244,223,183}}};
inline Mesh Skulltula(int limb) {
    Mesh mesh;
    for(const auto& part:SkulltulaBlenderData::parts) {
        if(part.limb!=limb)continue;
        for(uint32_t i=part.first;i<part.first+part.count;++i) {
            const auto& v=SkulltulaBlenderData::vertices[SkulltulaBlenderData::indices[i]];
            mesh.push_back({{float(v.x),float(v.y),float(v.z)},Paint(Palette[part.material],Surfaces[part.material]),Unit({float(v.nx),float(v.ny),float(v.nz)})});
        }
    }
    return mesh;
}
}
