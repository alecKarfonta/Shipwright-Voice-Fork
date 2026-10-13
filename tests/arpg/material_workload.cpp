#include "GearGeometry.h"
#include "RelicGeometry.h"
#include "OctorokGeometry.h"
#include "SkulltulaGeometry.h"
#include "BubbleGeometry.h"
#include <iostream>
#include <map>
#include <set>
#include <bit>
using namespace ARPG;using namespace GearGeometry;
struct Stats {uint64_t hash=1469598103934665603ull,vertices=0,batches=0,maxBatches=0,models=0;};
std::map<std::string,Stats> stats;
void add(const char* family,const Mesh& m) {
 auto& s=stats[family];std::set<uint32_t> batches;
 auto hash=[&](uint32_t bits){for(int n=0;n<4;++n){s.hash^=(bits>>(n*8))&255;s.hash*=1099511628211ull;}};
 for(auto v:m) {
  for(float f:{v.p.x,v.p.y,v.p.z,v.normal.x,v.normal.y,v.normal.z})hash(std::bit_cast<uint32_t>(f));
  uint32_t rgb=uint32_t(v.color.r)<<16|uint32_t(v.color.g)<<8|v.color.b;hash(rgb);
#ifdef LEGACY_RGB_BATCHES
  batches.insert(rgb);
#else
  batches.insert(MaterialKey(v.color));
#endif
 }
 s.models++;s.vertices+=m.size();s.batches+=batches.size();s.maxBatches=std::max(s.maxBatches,uint64_t(batches.size()));
}
int main() {
 for(int v=0;v<4;++v){add("equipment",Shield(v));add("equipment",Tunic(false,v));add("equipment",Tunic(true,v));add("equipment",Boot(false,v));add("equipment",Boot(true,v));}
 for(int b=1;b<=4;++b)for(int e=-1;e<3;++e)for(int mount=0;mount<4;++mount)add("equipment",Sword(e,mount==1,mount>=2,mount==3,b));
 for(int id=0;id<20;++id)for(int p=0;p<=6;++p)add("relics",RelicGeometry::Build(static_cast<ItemAppearance::Kind>(32+id),static_cast<RelicGeometry::Pose>(p)));
 for(int limb:OctorokGeometry::Limbs)add("octorok",OctorokGeometry::Octorok(limb));
 for(int limb:SkulltulaGeometry::Limbs)add("skulltula",SkulltulaGeometry::Skulltula(limb));
 for(int variant=0;variant<4;++variant)for(int limb:BubbleGeometry::Limbs)add("bubble",BubbleGeometry::Bubble(limb,variant));
 std::cout<<"{";bool first=true;
 for(auto [name,s]:stats){if(!first)std::cout<<",";first=false;std::cout<<"\""<<name<<"\":{\"geometry_rgb_hash\":\""<<s.hash<<"\",\"models\":"<<s.models<<",\"vertices\":"<<s.vertices<<",\"material_batches\":"<<s.batches<<",\"maximum_batches_per_model\":"<<s.maxBatches<<"}";}
 std::cout<<"}\n";
}
