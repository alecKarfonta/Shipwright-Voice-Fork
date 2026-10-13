#pragma once
#include "GearGeometry.h"
#include "RelicBlenderData.h"
#include "ItemAppearance.h"

namespace ARPG::RelicGeometry {
inline GearGeometry::Surface Material(int material,int slot,int part) {
    using S=GearGeometry::Surface;
    constexpr S surfaces[]={S::Metal,S::Metal,S::Metal,S::Metal,S::Gem,S::Leather,S::Leather,S::Gem,S::Gem,S::Bone,S::Wood,S::Gem,S::Leather,S::Leather,S::Gem,S::Leather};
    // The iron-colored Cinderbound tunic is cloth; its separate fitted ornaments remain metal.
    if(slot==ARPG_SLOT_TUNIC && part!=3 && (material==0 || material==5 || material==6 || material==10))return S::Cloth;
    if(slot==ARPG_SLOT_BOOTS && (material==0 || material==5 || material==6 || material==10))return S::Leather;
    if(slot==ARPG_SLOT_SWORD && part!=0 && (material==5 || material==6 || material==10))return S::Leather;
    if(slot==ARPG_SLOT_SHIELD && material==6)return S::Metal;
    return surfaces[material];
}
enum class Pose { Pickup, Wielded, Sheathed, Hilt, Scabbard, Chest, Foot };
// [TIME] | [INFO] | RelicGeometry.Build | One Blender identity supplies both the collectible and animated equipment parts.
inline GearGeometry::Mesh Build(ItemAppearance::Kind kind,Pose pose,int base=2) {
    using namespace GearGeometry;
    const int identity=ItemAppearance::RelicIndex(kind),slot=ItemAppearance::RelicSlot(kind);
    Mesh mesh;
    if(identity<0) { return mesh; }
    auto include=[&](int part) {
        if(slot==ARPG_SLOT_SWORD) {
            if(pose==Pose::Sheathed) { return part==1 || part==2; }
            if(pose==Pose::Hilt) { return part==1; }
            if(pose==Pose::Scabbard) { return part==2; }
            return part==0 || part==1;
        }
        if(slot==ARPG_SLOT_TUNIC && pose==Pose::Chest) { return part==3; }
        return part!=2;
    };
    float length=slot==ARPG_SLOT_SWORD ? (base==4?.48f:base==3?1.48f:base==1?.74f:1.f) : 1.f;
    int copies=slot==ARPG_SLOT_BOOTS && pose!=Pose::Foot ? 2 : 1;
    for(int copy=0;copy<copies;++copy) {
        for(const auto& part:RelicBlenderData::parts) {
            if(part.identity!=identity || !include(part.part)) { continue; }
            auto c=RelicBlenderData::colors[part.material];
            for(uint32_t j=0;j<part.count;++j) {
                const auto& v=RelicBlenderData::vertices[RelicBlenderData::indices[part.first+j]];
                Point p{v.x*.01f,v.y*.01f*length,v.z*.01f};
                if(copies==2) { p.x+=copy?4.3f:-4.3f; }
                if(slot==ARPG_SLOT_TUNIC && part.part==3 && pose!=Pose::Chest) { p.z+=3.8f; }
                auto n=Unit({float(v.nx),v.ny/length,float(v.nz)});
                mesh.push_back({p,{c[0],c[1],c[2],Material(part.material,slot,part.part)},n});
            }
        }
    }
    return mesh;
}
inline GearGeometry::Color Cloth(ItemAppearance::Kind kind) {
    using Kind=ItemAppearance::Kind;
    if(kind==Kind::AshenPilgrim) { return {121,35,34}; }
    if(kind==Kind::BriarwatchTunic) { return {33,87,53}; }
    if(kind==Kind::CinderboundTunic) { return {61,68,79}; }
    return {30,53,91};
}
}
