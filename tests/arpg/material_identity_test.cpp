#include "GearGeometry.h"
#include "WeaponGeometry.h"
#include "RelicGeometry.h"
#include "OctorokGeometry.h"
#include "SkulltulaGeometry.h"
#include "BubbleGeometry.h"
#include "EnemyGeometry.h"
#include "WolfosGeometry.h"
#include "LizalfosGeometry.h"
#include "DekuBabaGeometry.h"
#include "TektiteGeometry.h"
#include "KeeseGeometry.h"
#include "WaystoneGeometry.h"
#include "RelayGeometry.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace ARPG;
using namespace GearGeometry;
size_t checked=0;
void Check(const Mesh& mesh) {
    assert(mesh.size()%3==0);
    for(size_t i=0;i<mesh.size();i+=3) {
        assert(mesh[i].color.surface!=Surface::Inherit);
        for(int n=0;n<3;++n) {
            const auto c=mesh[i+n].color;
            assert(c.surface==mesh[i].color.surface);
            assert(Shade(c,.45f).surface==c.surface);
            auto recolored=c;recolored.r=255-c.r;recolored.g=255-c.g;recolored.b=255-c.b;
            assert(ResolveSurface(recolored,Surface::Stone)==c.surface);
            ++checked;
        }
    }
}
int main() {
    assert(Shade(bronze,.6f).surface==Surface::Metal);
    Color red{235,67,57};Mesh jewel;Gem(jewel,{0,0,0},2,3,1,red);
    for(auto v:jewel)assert(v.color.surface==Surface::Gem);
    Check(jewel);
    // Gold embroidery and a gold buckle share a color but have different surfaces.
    std::set<Surface> goldTunicSurfaces;
    for(auto v:Tunic(false,2))if(v.color.r==gold.r && v.color.g==gold.g && v.color.b==gold.b)goldTunicSurfaces.insert(v.color.surface);
    assert(goldTunicSurfaces.contains(Surface::Cloth) && goldTunicSurfaces.contains(Surface::Metal));
    // Equal RGB must not merge differently textured surfaces in a display-list batch.
    assert(MaterialKey(Paint(red,Surface::Gem))!=MaterialKey(Paint(red,Surface::Leather)));
    for(int v=0;v<4;++v) {Check(Shield(v));Check(Tunic(false,v));Check(Tunic(true,v));Check(Boot(false,v));Check(Boot(true,v));}
    for(int b=1;b<=4;++b)for(int e=-1;e<3;++e)for(int mount=0;mount<4;++mount)Check(Sword(e,mount==1,mount>=2,mount==3,b));
    Check(WeaponGeometry::Bow());Check(WeaponGeometry::Hammer());
    for(int id=0;id<20;++id)for(int p=0;p<=int(RelicGeometry::Pose::Foot);++p)
        Check(RelicGeometry::Build(static_cast<ItemAppearance::Kind>(int(ItemAppearance::Kind::Cinderwake)+id),static_cast<RelicGeometry::Pose>(p)));
    for(int part=0;part<Waystone::PartCount;++part)for(int tone=0;tone<7;++tone)Check(Waystone::Build(part,tone));
    for(int part=0;part<3;++part)for(int tone=0;tone<3;++tone)Check(RelayGeometry::Build(part,tone));
    for(int limb:OctorokGeometry::Limbs)Check(OctorokGeometry::Octorok(limb));
    for(int limb:SkulltulaGeometry::Limbs)Check(SkulltulaGeometry::Skulltula(limb));
    for(int v=0;v<4;++v)for(int limb:BubbleGeometry::Limbs)Check(BubbleGeometry::Bubble(limb,v));
    for(int limb:EnemyGeometry::StalfosLimbs)for(int rank=0;rank<3;++rank)Check(EnemyGeometry::Stalfos(limb,rank));
    for(int limb=1;limb<22;++limb)for(int rank=0;rank<3;++rank)for(bool white:{false,true}) {
        Mesh mesh;for(auto v:WolfosGeometry::Wolfos(limb,rank,white))mesh.push_back(v.surface);Check(mesh);
    }
    for(int limb:LizalfosGeometry::Limbs)for(int rank=0;rank<3;++rank)for(bool dino:{false,true})Check(LizalfosGeometry::Lizalfos(limb,rank,dino));
    for(int limb:DekuBabaGeometry::Limbs)for(bool big:{false,true})Check(DekuBabaGeometry::DekuBaba(limb,big));
    for(int limb:TektiteGeometry::Limbs)for(bool blue:{false,true})Check(TektiteGeometry::Tektite(limb,blue));
    for(int limb:KeeseGeometry::Limbs)for(int v=0;v<3;++v)Check(KeeseGeometry::Keese(limb,v));
    assert(OctorokGeometry::Surfaces[5]==Surface::Flat);
    assert(BubbleGeometry::Surfaces[5]==Surface::Glow);
    assert(SkulltulaGeometry::Surfaces[0]==Surface::Bone);
    std::cout<<"Material identity preserved across "<<checked<<" authored vertices and recolors\n";
}
