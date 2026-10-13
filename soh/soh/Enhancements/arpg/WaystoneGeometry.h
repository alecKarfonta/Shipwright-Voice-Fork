#pragma once
#include "GearGeometry.h"
#include "WaystoneBlenderData.h"
#include "WaystoneState.h"

namespace ARPG::Waystone {
// Slots follow the semantic material names in the Blender exporter.
inline constexpr std::array<GearGeometry::Surface,11> Surfaces={GearGeometry::Surface::Stone,GearGeometry::Surface::Stone,GearGeometry::Surface::Metal,GearGeometry::Surface::Metal,GearGeometry::Surface::Wood,GearGeometry::Surface::Wood,GearGeometry::Surface::Metal,GearGeometry::Surface::Cloth,GearGeometry::Surface::Gem,GearGeometry::Surface::Gem,GearGeometry::Surface::Stone};
inline constexpr GearGeometry::Color Tones[]={{193,204,215},{126,216,145},{249,129,60},{142,220,250},{192,151,248},{245,202,105},{45,60,67}};
// [TIME] | [INFO] | Waystone.Geometry | Preserve the Blender model's cut surfaces and normals while choosing the active oath's inlay color.
inline GearGeometry::Mesh Build(int part,int tone=0) {
    using namespace GearGeometry;
    if(part<0 || part>=PartCount) { return {}; }
    Color oath=Tones[std::clamp(tone,0,6)];
    Color colors[]={{57,76,81},{110,135,133},{133,96,53},{221,178,92},{69,43,34},{109,67,43},
                    {51,61,70},{52,31,58},oath,tone==6?oath:Shade(oath,1.12f),{28,36,41}};
    Mesh mesh;
    for(const auto& group:WaystoneBlenderData::parts) {
        if(group.part!=part) { continue; }
        for(uint32_t i=group.first;i<group.first+group.count;++i) {
            const auto& v=WaystoneBlenderData::vertices[WaystoneBlenderData::indices[i]];
            mesh.push_back({{v.x*.01f,v.y*.01f,v.z*.01f},Paint(colors[group.material],Surfaces[group.material]),Unit({float(v.nx),float(v.ny),float(v.nz)})});
        }
    }
    return mesh;
}
}
