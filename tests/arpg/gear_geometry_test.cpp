#include "GearGeometry.h"
#include "ItemAppearance.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

using namespace ARPG;
int main() {
    size_t triangles=0;
    std::vector<GearGeometry::Mesh> meshes;
    for(int element=-1;element<3;++element) { for(int base=1;base<=3;++base) {
        meshes.push_back(GearGeometry::Sword(element,false,false,false,base));
        meshes.push_back(GearGeometry::Sword(element,true,true,false,base));
        meshes.push_back(GearGeometry::Sword(element,true,true,true,base));
        meshes.push_back(GearGeometry::Sword(element,true,false,false,base));
    } }
    for(int variant=0;variant<4;++variant) {
        meshes.push_back(GearGeometry::Shield(variant));
        meshes.push_back(GearGeometry::Tunic(false,variant));
        meshes.push_back(GearGeometry::Tunic(true,variant));
        meshes.push_back(GearGeometry::Boots(false,variant));
        meshes.push_back(GearGeometry::Boots(true,variant));
        meshes.push_back(GearGeometry::Boot(false,variant));
    }
    for(const auto& mesh:meshes) {
        assert(!mesh.empty() && mesh.size()%3==0 && mesh.size()<30000);
        for(const auto& vertex:mesh) {
            auto n=vertex.normal;
            float length=n.x*n.x+n.y*n.y+n.z*n.z;
            assert(std::isfinite(length) && std::abs(length-1)<.001f);
        }
        for(size_t i=0;i<mesh.size();i+=3) {
            auto a=mesh[i].p,b=mesh[i+1].p,c=mesh[i+2].p;
            for(auto p:{a,b,c}) {
                assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
                assert(std::abs(p.x)<100 && std::abs(p.y)<100 && std::abs(p.z)<100);
            }
            auto cross=GearGeometry::Cross(GearGeometry::Add(b,GearGeometry::Mul(a,-1)),
                                          GearGeometry::Add(c,GearGeometry::Mul(a,-1)));
            assert(cross.x*cross.x+cross.y*cross.y+cross.z*cross.z>1e-8f);
            ++triangles;
        }
    }
    const int mods[]={ARPG_MOD_FLAME,ARPG_MOD_AEGIS,ARPG_MOD_MAGICAFF,ARPG_MOD_SWIFT};
    const ItemAppearance::Kind kinds[]={ItemAppearance::Kind::Ember,ItemAppearance::Kind::Root,
        ItemAppearance::Kind::Star,ItemAppearance::Kind::Wind};
    for(int slot=0;slot<4;++slot) {
        ARPGItemInstance item{}; item.slot=slot; item.baseEquip=1; item.modCount=1;
        item.mods[0]={uint8_t(mods[slot]),10};
        auto before=item;
        for(int i=0;i<100;++i) { assert(ItemAppearance::Get(item).kind==kinds[slot]); }
        assert(item.mods[0].power==before.mods[0].power && item.baseEquip==before.baseEquip);
        item.modCount=0; assert(ItemAppearance::Get(item).kind==ItemAppearance::Kind::Native);
    }
    for(int slot=0;slot<4;++slot) { for(int base=1;base<=3;++base) {
        ARPGItemInstance item{};item.slot=slot;item.baseEquip=base;
        assert(ItemAppearance::WorldKind(item)!=ItemAppearance::Kind::Native);
    } }
    ARPGItemInstance missing{}; assert(ItemAppearance::WorldKind(missing)==ItemAppearance::Kind::Native);
    missing.baseEquip=4; assert(ItemAppearance::WorldKind(missing)==ItemAppearance::Kind::Native);
    // [TIME] | [INFO] | GearIdentity.Test | Painted base identities remain distinct; sockets resolve through the same appearance catalog as rolled powers.
    for(int slot=0;slot<4;++slot) for(int base=1;base<=3;++base) {
        ARPGItemInstance item{}; item.slot=slot; item.baseEquip=base;
        auto style=ItemAppearance::Resolve(item);
        assert(style.artwork && style.name && style.kind==ItemAppearance::WorldKind(item));
        for(int other=base+1;other<=3;++other) {
            item.baseEquip=other;
            auto next=ItemAppearance::Resolve(item);
            assert(std::string(style.artwork)!=next.artwork && style.kind!=next.kind);
        }
    }
    ARPGItemInstance socketed{};socketed.slot=ARPG_SLOT_SWORD;socketed.baseEquip=2;
    socketed.socketCount=1;socketed.sockets[0]=2;
    assert(ItemAppearance::Resolve(socketed).kind==ItemAppearance::Kind::Frost);
    socketed.sockets[0]=0;
    assert(ItemAppearance::Resolve(socketed).kind==ItemAppearance::Kind::KnightBlade);
    auto height=[](const GearGeometry::Mesh& mesh) {
        float low=100,high=-100;
        for(auto v:mesh) { low=std::min(low,v.p.y); high=std::max(high,v.p.y); }
        return high-low;
    };
    auto standard=GearGeometry::Sword(-1,false,false,false,2);
    auto shortBlade=GearGeometry::Sword(-1,false,false,false,1);
    auto greatBlade=GearGeometry::Sword(-1,false,false,false,3);
    assert(std::abs(height(shortBlade)/height(standard)-.74f)<.0001f);
    assert(std::abs(height(greatBlade)/height(standard)-1.48f)<.0001f);
    // Base lengths are part of the shared mesh, including holstered variants.
    assert(std::abs(height(GearGeometry::Sword(0,true,true,false,3))/height(GearGeometry::Sword(0,true,true,false,2))-1.48f)<.0001f);
    std::cout<<"Validated "<<triangles<<" nondegenerate triangles with unit normals across "<<meshes.size()<<" meshes and stable appearance selection.\n";
}
