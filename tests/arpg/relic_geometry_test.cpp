#include "RelicGeometry.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
using namespace ARPG;
int main() {
    using Kind=ItemAppearance::Kind;using Pose=RelicGeometry::Pose;
    size_t triangles=0,variants=0;std::set<std::string> icons;
    for(int id=0;id<ItemAppearance::RelicCount;++id) {
        const auto style=ItemAppearance::relics[id];
        assert(ItemAppearance::RelicIndex(style.kind)==id);assert(icons.insert(style.artwork).second);
        int slot=ItemAppearance::RelicSlot(style.kind);
        ARPGItemInstance item{};item.slot=slot;item.baseEquip=2;
        if(id<8) { Loot::IdentifyUnique(item,id+1); } else { Loot::IdentifySet(item,(id-8)/4+1); }
        for(int base=1;base<=3;++base) for(int gem=0;gem<=Loot::RuneLast;++gem) {
            item.baseEquip=base;item.socketCount=1;item.sockets[0]=gem;
            auto before=item;
            assert(ItemAppearance::Resolve(item).kind==style.kind);
            assert(std::string(ItemAppearance::Resolve(item).artwork)==style.artwork);
            assert(std::memcmp(&item,&before,sizeof(item))==0);
        }
        assert(ItemAppearance::IsSword(style.kind)==(slot==0));
        assert(ItemAppearance::IsShield(style.kind)==(slot==1));
        assert(ItemAppearance::IsTunic(style.kind)==(slot==2));
        assert(ItemAppearance::IsBoots(style.kind)==(slot==3));
        std::vector<Pose> poses=slot==0?std::vector<Pose>{Pose::Pickup,Pose::Wielded,Pose::Sheathed,Pose::Hilt,Pose::Scabbard}:
             slot==2?std::vector<Pose>{Pose::Pickup,Pose::Chest}:slot==3?std::vector<Pose>{Pose::Pickup,Pose::Foot}:std::vector<Pose>{Pose::Pickup};
        for(int base=1;base<=(slot==0?4:1);++base) for(auto pose:poses) {
            auto m=RelicGeometry::Build(style.kind,pose,base);
            assert(!m.empty() && m.size()%3==0 && m.size()<18000);++variants;
            for(const auto& v:m) {
                auto n=v.normal;float length=n.x*n.x+n.y*n.y+n.z*n.z;
                assert(std::isfinite(length) && std::abs(length-1)<.001f);
                for(float p:{v.p.x,v.p.y,v.p.z}) { assert(std::isfinite(p) && std::abs(p)<100); }
            }
            for(size_t k=0;k<m.size();k+=3) {
                auto a=m[k].p,b=m[k+1].p,c=m[k+2].p;
                auto cross=GearGeometry::Cross(GearGeometry::Add(b,GearGeometry::Mul(a,-1)),GearGeometry::Add(c,GearGeometry::Mul(a,-1)));
                assert(cross.x*cross.x+cross.y*cross.y+cross.z*cross.z>1e-10f);++triangles;
            }
        }
        if(slot==0) {
            auto pickup=RelicGeometry::Build(style.kind,Pose::Pickup);
            auto hand=RelicGeometry::Build(style.kind,Pose::Wielded);
            assert(pickup.size()==hand.size());
            for(size_t j=0;j<hand.size();++j) { assert(pickup[j].p.x==hand[j].p.x && pickup[j].p.y==hand[j].p.y && pickup[j].p.z==hand[j].p.z); }
            auto height=[](const auto& m){float lo=100,hi=-100;for(auto v:m){lo=std::min(lo,v.p.y);hi=std::max(hi,v.p.y);}return hi-lo;};
            for(auto pose:{Pose::Pickup,Pose::Sheathed}) {
                float regular=height(RelicGeometry::Build(style.kind,pose,2));
                assert(std::abs(height(RelicGeometry::Build(style.kind,pose,1))/regular-.74f)<.0001f);
                assert(std::abs(height(RelicGeometry::Build(style.kind,pose,3))/regular-1.48f)<.0001f);
            }
        }
    }
    ARPGItemInstance invalid{};invalid.slot=0;invalid.baseEquip=2;invalid.uniqueId=255;invalid.rarity=ARPG_RARITY_UNIQUE;
    assert(ItemAppearance::RelicIndex(ItemAppearance::Resolve(invalid).kind)<0);
    invalid.uniqueId=1;invalid.setId=1;assert(ItemAppearance::RelicIndex(ItemAppearance::Resolve(invalid).kind)<0);
    invalid.uniqueId=0;invalid.setId=1;invalid.slot=255;assert(ItemAppearance::Resolve(invalid).kind==Kind::Native);
    assert(RelicGeometry::Build(Kind::Native,Pose::Pickup).empty());
    std::cout<<"Validated 20 persistent identities, "<<variants<<" mounting/base variants and "<<triangles<<" nondegenerate triangles.\n";
}
