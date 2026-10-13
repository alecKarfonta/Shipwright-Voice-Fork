#pragma once
#include "GearGeometry.h"
#include "StalfosBlenderData.h"

namespace ARPG::EnemyGeometry {
// Slots follow the semantic material names in the Blender exporter.
inline constexpr std::array<GearGeometry::Surface,11> Surfaces={GearGeometry::Surface::Bone,GearGeometry::Surface::Bone,GearGeometry::Surface::Metal,GearGeometry::Surface::Metal,GearGeometry::Surface::Cloth,GearGeometry::Surface::Glow,GearGeometry::Surface::Metal,GearGeometry::Surface::Metal,GearGeometry::Surface::Flat,GearGeometry::Surface::Leather,GearGeometry::Surface::Metal};
using namespace GearGeometry;
// [TIME] | [INFO] | EnemyGeometry.Rig | Native Stalfos limb indices and local axes preserve all animation, collider and break-apart transforms.
inline constexpr std::array<int,24> StalfosLimbs={10,11,12,13,14,15,18,21,26,27,28,29,34,35,36,37,38,48,49,50,57,58,59,60};
struct Palette { Color bone, shadow, armor, trim, cloth, eye; };
inline Palette Colors(int rank) {
    if(rank>=2) { return {{225,218,179},{105,108,97},{44,74,73},{204,158,76},{57,47,78},{110,234,207}}; }
    if(rank==1) { return {{216,211,182},{106,104,91},{84,53,63},{188,153,107},{65,37,44},{249,164,79}}; }
    return {{214,207,174},{102,104,89},{57,84,74},{147,125,82},{53,64,54},{225,100,68}};
}
// [TIME] | [INFO] | EnemyGeometry.Blender | Authored bone, armor and weapon surfaces retain the native rigid limb matrices and immutable BodyBreak display lists.
inline Mesh Stalfos(int limb,int rank=0) {
    Mesh mesh;const auto p=Colors(rank);
    const std::array<Color,11> materials={p.bone,p.shadow,p.armor,p.trim,p.cloth,p.eye,
        Color{99,122,122},Color{206,219,206},Color{29,35,31},Color{63,45,31},Color{124,151,137}};
    for(const auto& part:StalfosBlenderData::parts) {
        if(part.limb!=limb || part.rank!=rank) { continue; }
        for(uint32_t i=part.first;i<part.first+part.count;++i) {
            const auto& v=StalfosBlenderData::vertices[StalfosBlenderData::indices[i]];
            mesh.push_back({{float(v.x),float(v.y),float(v.z)},Paint(materials[part.material],Surfaces[part.material]),Unit({float(v.nx),float(v.ny),float(v.nz)})});
        }
    }
    return mesh;
}
}
