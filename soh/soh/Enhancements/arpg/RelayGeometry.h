#pragma once
#include "GearGeometry.h"
#include "RelayBlenderData.h"
namespace ARPG::RelayGeometry {
// Slots follow the semantic material names in the Blender exporter.
inline constexpr std::array<GearGeometry::Surface,7> Surfaces={GearGeometry::Surface::Stone,GearGeometry::Surface::Metal,GearGeometry::Surface::Metal,GearGeometry::Surface::Metal,GearGeometry::Surface::Gem,GearGeometry::Surface::Gem,GearGeometry::Surface::Gem};
// [TIME] | [INFO] | Relay.Geometry | Separate beacon and spirit pivots animate without deforming the Blender-authored cage.
inline GearGeometry::Mesh Build(int part,int tone=0) {
    using namespace GearGeometry;
    if(part<0 || part>2 || tone<0 || tone>2) { return {}; }
    Color energy=tone==0?Color{66,81,102}:tone==1?Color{119,222,244}:Color{250,200,82};
    Color colors[]={{42,53,68},{111,138,156},{139,86,48},{222,178,82},{92,57,123},energy,tone==0?energy:Color{230,250,249}};
    Mesh mesh;
    for(const auto& group:RelayBlenderData::parts) {
        if(group.part!=part) { continue; }
        for(uint32_t i=group.first;i<group.first+group.count;++i) {
            const auto& v=RelayBlenderData::vertices[RelayBlenderData::indices[i]];
            mesh.push_back({{v.x*.01f,v.y*.01f,v.z*.01f},Paint(colors[group.material],Surfaces[group.material]),Unit({float(v.nx),float(v.ny),float(v.nz)})});
        }
    }
    return mesh;
}
}
