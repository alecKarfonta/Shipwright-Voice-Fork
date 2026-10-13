#pragma once
#include "GearGeometry.h"
#include "BubbleBlenderData.h"

namespace ARPG::BubbleGeometry {
// Slots follow the semantic material names in the Blender exporter.
inline constexpr std::array<GearGeometry::Surface,8> Surfaces={GearGeometry::Surface::Bone,GearGeometry::Surface::Bone,GearGeometry::Surface::Flat,GearGeometry::Surface::Wing,GearGeometry::Surface::Wing,GearGeometry::Surface::Glow,GearGeometry::Surface::Bone,GearGeometry::Surface::Bone};
using namespace GearGeometry;
inline constexpr std::array<int,6> Limbs={4,8,9,13,14,15};
inline constexpr std::array<std::array<Color,8>,4> Palettes={{{{{219,215,191},{247,235,199},{27,31,39},{74,88,133},{153,170,203},{139,215,255},{235,224,183},{119,118,101}}},
    {{{214,193,158},{243,224,179},{36,23,22},{119,48,44},{185,103,72},{255,146,60},{237,213,168},{114,85,66}}},
    {{{229,226,213},{251,244,222},{34,32,44},{127,114,141},{191,177,201},{239,196,255},{241,231,200},{126,121,132}}},
    {{{208,213,177},{237,235,191},{27,35,25},{72,101,59},{143,171,103},{168,255,107},{229,226,176},{104,119,85}}}}};
inline const auto& Colors(int variant){return Palettes[std::clamp(variant,0,3)];}
inline Mesh Bubble(int limb,int variant) {
    Mesh mesh;
    for(const auto& part:BubbleBlenderData::parts) {
        if(part.limb!=limb)continue;
        for(uint32_t i=part.first;i<part.first+part.count;++i) {
            const auto& v=BubbleBlenderData::vertices[BubbleBlenderData::indices[i]];
            mesh.push_back({{float(v.x),float(v.y),float(v.z)},Paint(Colors(variant)[part.material],Surfaces[part.material]),Unit({float(v.nx),float(v.ny),float(v.nz)})});
        }
    }
    return mesh;
}
}
