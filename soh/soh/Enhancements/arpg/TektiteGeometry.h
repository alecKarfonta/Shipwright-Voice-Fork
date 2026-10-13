#pragma once
#include "GearGeometry.h"
#include "TektiteBlenderData.h"

namespace ARPG::TektiteGeometry {
// Slots follow the semantic material names in the Blender exporter.
inline constexpr std::array<GearGeometry::Surface,10> Surfaces={GearGeometry::Surface::Scales,GearGeometry::Surface::Scales,GearGeometry::Surface::Flat,GearGeometry::Surface::Skin,GearGeometry::Surface::Skin,GearGeometry::Surface::Bone,GearGeometry::Surface::Flat,GearGeometry::Surface::Flat,GearGeometry::Surface::Flat,GearGeometry::Surface::Glow};
using namespace GearGeometry;
inline constexpr std::array<int,9> Limbs={4,8,9,13,14,18,19,23,24};
inline constexpr std::array<std::array<Color,10>,2> Palettes={{
    {{{171,74,39},{223,135,67},{64,35,31},{118,64,49},{67,49,40},{198,166,109},{242,226,174},{216,89,27},{23,17,18},{255,249,222}}},
    {{{52,98,137},{116,164,190},{29,48,73},{60,101,121},{39,63,70},{184,204,183},{242,237,182},{166,115,34},{16,27,40},{255,253,229}}}
}};
inline const std::array<Color,10>& Colors(bool blue){return Palettes[blue];}
inline Mesh Tektite(int limb,bool blue) {
    Mesh mesh;
    for(const auto& part:TektiteBlenderData::parts) {
        if(part.variant!=int(blue) || part.limb!=limb)continue;
        for(uint32_t i=part.first;i<part.first+part.count;++i) {
            const auto& v=TektiteBlenderData::vertices[TektiteBlenderData::indices[i]];
            mesh.push_back({{float(v.x),float(v.y),float(v.z)},Paint(Colors(blue)[part.material],Surfaces[part.material]),Unit({float(v.nx),float(v.ny),float(v.nz)})});
        }
    }
    return mesh;
}
}
