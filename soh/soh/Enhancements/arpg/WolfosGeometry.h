#pragma once
#include "WolfosRig.h"
#include "WolfosBlenderData.h"

namespace ARPG::WolfosGeometry {
// Slots follow the semantic material names in the Blender exporter.
inline constexpr std::array<GearGeometry::Surface,11> Surfaces={GearGeometry::Surface::Fur,GearGeometry::Surface::Fur,GearGeometry::Surface::Fur,GearGeometry::Surface::Flat,GearGeometry::Surface::Bone,GearGeometry::Surface::Metal,GearGeometry::Surface::Glow,GearGeometry::Surface::Flat,GearGeometry::Surface::Flat,GearGeometry::Surface::Flat,GearGeometry::Surface::Metal};
struct SkinVertex { GearGeometry::Vertex surface; int joint; int16_t s=0,t=0; int material=-1; };
using SkinMesh=std::vector<SkinVertex>;
struct Palette { Color fur,mane,muzzle,skin,claw,metal,eye; };
inline Palette Colors(int rank,bool white) {
    if(white) { return {{193,210,217},{133,156,174},{226,227,211},{40,52,65},{215,225,225},
                       rank>=2?Color{193,163,97}:Color{146,168,183},{116,228,243}}; }
    return {{137,122,96},rank>=2?Color{82,93,88}:Color{89,85,69},{186,171,138},{41,40,37},{216,205,168},
            rank>=2?Color{194,154,76}:Color{159,126,69},rank>=2?Color{152,227,135}:Color{245,161,63}};
}
inline void Attach(SkinMesh& result,const Mesh& mesh,int joint) {
    for(const auto& v:mesh) { result.push_back({v,joint}); }
}
// [TIME] | [INFO] | WolfosGeometry.Blender | Authored surfaces carry split normals and per-vertex native joints, including all 17 flexible skin connections.
inline SkinMesh Wolfos(int limb,int rank,bool white,int blink=0) {
    SkinMesh mesh;const auto p=Colors(rank,white);
    const std::array<Color,11> materials={p.fur,p.mane,p.muzzle,p.skin,p.claw,p.metal,p.eye,
        white?Color{111,62,76}:Color{103,48,49},white?Color{19,29,39}:Color{17,20,17},
        white?Color{239,249,250}:Color{237,245,227},white?Color{199,216,224}:Color{109,124,119}};
    for(const auto& part:WolfosBlenderData::parts) {
        if(part.species!=int(white) || part.limb!=limb || part.rank>rank ||
           (part.blink>=0 && part.blink!=blink)) { continue; }
        for(uint32_t i=part.first;i<part.first+part.count;++i) {
            const auto& v=WolfosBlenderData::vertices[WolfosBlenderData::indices[i]];
            mesh.push_back({{{float(v.x),float(v.y),float(v.z)},Paint(materials[part.material],Surfaces[part.material]),
                            Unit({float(v.nx),float(v.ny),float(v.nz)})},v.joint,v.s,v.t,part.material});
        }
    }
    return mesh;
}
}
