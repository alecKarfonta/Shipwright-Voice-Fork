#pragma once
#include "GearGeometry.h"
#include "LizalfosBlenderData.h"

namespace ARPG::LizalfosGeometry {
// Slots follow the semantic material names in the Blender exporter.
inline constexpr std::array<GearGeometry::Surface,13> Surfaces={GearGeometry::Surface::Scales,GearGeometry::Surface::Scales,GearGeometry::Surface::Scales,GearGeometry::Surface::Metal,GearGeometry::Surface::Metal,GearGeometry::Surface::Leather,GearGeometry::Surface::Bone,GearGeometry::Surface::Glow,GearGeometry::Surface::Metal,GearGeometry::Surface::Metal,GearGeometry::Surface::Flat,GearGeometry::Surface::Flat,GearGeometry::Surface::Scales};
using namespace GearGeometry;
inline constexpr std::array<int,20> Limbs={8,9,10,15,16,17,22,23,24,25,29,30,33,38,39,40,45,46,47,48};
struct Palette { Color skin,belly,scale,armor,trim,leather,claw,eye; };
inline Palette Colors(int rank,bool dino) {
    if(dino) { return {{131,76,46},{184,132,73},{71,45,37},{47,52,61},rank>=2?Color{220,168,80}:Color{186,119,53},
                      {53,33,30},{220,193,142},{247,101,35}}; }
    return {{79,119,75},{180,163,103},{44,76,58},rank==1?Color{49,62,90}:Color{42,65,65},
            rank>=2?Color{207,180,94}:Color{187,149,68},{59,43,33},{217,211,160},rank>=2?Color{109,231,188}:Color{231,170,52}};
}
// [TIME] | [INFO] | LizalfosGeometry.Blender | Edited surfaces and split normals come from Blender; the native rig owns all animation and attachment transforms.
inline Mesh Lizalfos(int limb,int rank,bool dino,bool sheathed=false) {
    Mesh mesh;const auto p=Colors(rank,dino);
    const std::array<Color,13> materials={p.skin,p.belly,p.scale,p.armor,p.trim,p.leather,p.claw,p.eye,
        dino?Color{78,96,102}:Color{79,109,114},dino?Color{201,207,192}:Color{195,214,207},
        dino?Color{76,29,27}:Color{77,34,31},dino?Color{16,12,11}:Color{15,20,14},
        dino?Color{113,65,42}:Color{69,105,69}};
    for(const auto& part:LizalfosBlenderData::parts) {
        if(part.species!=int(dino) || part.limb!=limb || part.rank>rank ||
           (part.state==1 && sheathed) || (part.state==2 && !sheathed)) { continue; }
        for(uint32_t i=part.first;i<part.first+part.count;++i) {
            const auto& v=LizalfosBlenderData::vertices[LizalfosBlenderData::indices[i]];
            mesh.push_back({{float(v.x),float(v.y),float(v.z)},Paint(materials[part.material],Surfaces[part.material]),Unit({float(v.nx),float(v.ny),float(v.nz)})});
        }
    }
    return mesh;
}
}
