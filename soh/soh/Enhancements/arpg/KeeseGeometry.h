#pragma once
#include "GearGeometry.h"
#include "KeeseBlenderData.h"

namespace ARPG::KeeseGeometry {
// Slots follow the semantic material names in the Blender exporter.
inline constexpr std::array<GearGeometry::Surface,11> Surfaces={GearGeometry::Surface::Fur,GearGeometry::Surface::Fur,GearGeometry::Surface::Wing,GearGeometry::Surface::Wing,GearGeometry::Surface::Bone,GearGeometry::Surface::Skin,GearGeometry::Surface::Bone,GearGeometry::Surface::Glow,GearGeometry::Surface::Flat,GearGeometry::Surface::Flat,GearGeometry::Surface::Gem};
using namespace GearGeometry;
inline constexpr std::array<int,10> Limbs={6,9,10,15,16,17,22,23,24,27};
inline constexpr std::array<std::array<Color,11>,3> Palettes={{
    {{{84,74,119},{123,109,154},{89,70,111},{119,87,131},{73,59,91},{157,111,143},{222,218,187},{255,221,111},{25,17,27},{45,24,42},{180,142,91}}},
    {{{72,53,59},{116,68,56},{132,51,34},{190,77,36},{49,35,39},{214,92,42},{238,199,140},{255,233,117},{43,20,21},{42,17,22},{244,151,42}}},
    {{{107,150,171},{164,196,203},{73,126,168},{112,177,201},{42,77,116},{151,210,218},{229,242,222},{207,255,238},{17,44,69},{26,50,79},{172,224,237}}}
}};
// Species follows the native live state, including ignition and extinguishing.
inline int Variant(int params,bool onFire) { return onFire?1:params==4?2:0; }
inline Mesh Keese(int limb,int variant) {
    Mesh mesh;
    if(variant<0 || variant>=int(Palettes.size()))return mesh;
    for(const auto& part:KeeseBlenderData::parts) {
        if(part.variant!=variant || part.limb!=limb)continue;
        for(uint32_t i=part.first;i<part.first+part.count;++i) {
            const auto& v=KeeseBlenderData::vertices[KeeseBlenderData::indices[i]];
            mesh.push_back({{float(v.x),float(v.y),float(v.z)},Paint(Palettes[variant][part.material],Surfaces[part.material]),Unit({float(v.nx),float(v.ny),float(v.nz)})});
        }
    }
    return mesh;
}
}
