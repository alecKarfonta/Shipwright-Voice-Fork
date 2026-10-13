#include "LizalfosGeometry.h"
#include <iostream>
#include <string>
#include <stdexcept>
using namespace ARPG::LizalfosGeometry;
void Check(bool ok,const char* label) { if(!ok) { throw std::runtime_error(label); } }
int main() { try {
    size_t total=0,variants=0;std::array<std::array<size_t,3>,2> counts{};
    for(bool dino:{false,true}) { for(int rank=0;rank<3;++rank) {
        for(int limb:Limbs) { for(int sheath=0;sheath<(limb==15||limb==33?2:1);++sheath) {
            auto mesh=Lizalfos(limb,rank,dino,sheath);++variants;
            Check(mesh.size()>36 && mesh.size()%3==0 && mesh.size()<22000,"Every visible limb has a bounded mesh");
            float ymin=10000,xmin=10000;
            for(auto& v:mesh) {
                auto n=v.normal;ymin=std::min(ymin,v.p.y);xmin=std::min(xmin,v.p.x);
                Check(std::isfinite(v.p.x) && std::isfinite(v.p.y) && std::isfinite(v.p.z),"Finite geometry");
                Check(std::abs(v.p.x)<3900 && std::abs(v.p.y)<4200 && std::abs(v.p.z)<1800,"Geometry fits signed native vertices and actor proportions");
                Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001f,"Unit surface normals");
                v.p={std::round(v.p.x),std::round(v.p.y),std::round(v.p.z)};
            }
            for(size_t i=0;i<mesh.size();i+=3) {
                auto a=mesh[i].p,b=mesh[i+1].p,c=mesh[i+2].p;auto n=Cross(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));
                if(n.x*n.x+n.y*n.y+n.z*n.z==0) {
                    throw std::runtime_error("Collapsed integer triangle at limb "+std::to_string(limb)+", triangle "+std::to_string(i/3));
                }
                ++total;if(sheath==0) { ++counts[dino][rank]; }
            }
            if(limb==15) { Check(sheath?ymin>-450:std::abs(ymin+(dino?3600:1900))<1,"Sheathing removes the blade while drawn swords retain species-specific reach"); }
            if(limb==33 && sheath) { Check(xmin<-500,"Sheathed model includes its hilt behind the scabbard mouth"); }
        } }
        Check(Lizalfos(0,rank,dino).empty() && Lizalfos(14,rank,dino).empty() && Lizalfos(49,rank,dino).empty(),"Empty and unsupported rig nodes remain unchanged");
        std::cout<<"Species "<<dino<<", rank "<<rank<<": "<<counts[dino][rank]<<" triangles\n";
        Check(counts[dino][rank]<40000,"Per-actor mesh budget");
    } }
    for(const auto& c:counts) { Check(c[0]<c[1] && c[1]<c[2],"Champion and elite variants have distinct geometry"); }
    Check(counts[0][0]!=counts[1][0],"Lizalfos and Dinolfos have distinct geometry");
    std::cout<<"Validated "<<total<<" integer-safe triangles across "<<variants<<" animated limb and weapon-state variants.\n";
} catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
