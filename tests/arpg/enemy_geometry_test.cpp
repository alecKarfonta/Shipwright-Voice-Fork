#include "EnemyGeometry.h"
#include <iostream>
#include <stdexcept>
using namespace ARPG::EnemyGeometry;
void Check(bool value,const char* message) { if(!value) { throw std::runtime_error(message); } }
int main() { try {
    size_t total=0;std::array<size_t,3> counts{};
    for(int rank=0;rank<3;++rank) {
        for(int limb:StalfosLimbs) {
            const auto m=Stalfos(limb,rank);
            Check(m.size()>=36 && m.size()%3==0 && m.size()<24000,"Every visible native limb has a bounded replacement");
            for(const auto& v:m) {
                Check(std::isfinite(v.p.x) && std::isfinite(v.p.y) && std::isfinite(v.p.z),"Finite geometry");
                Check(std::abs(v.p.x)<4000 && std::abs(v.p.y)<2000 && std::abs(v.p.z)<1600,"Mesh fits native limb envelope and signed vertices");
                const auto n=v.normal;Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001f,"Unit surface normals");
            }
            for(size_t i=0;i<m.size();i+=3) {
                auto quantize=[](Point p) { return Point{std::round(p.x),std::round(p.y),std::round(p.z)}; };
                auto a=quantize(m[i].p),b=quantize(m[i+1].p),c=quantize(m[i+2].p);
                auto n=Cross(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));
                Check(n.x*n.x+n.y*n.y+n.z*n.z>0,"No collapsed triangles after native integer conversion");
                ++counts[rank];++total;
            }
            if(limb==34) {
                float tip=-10000;for(const auto& v:m) { tip=std::max(tip,v.p.x); }
                Check(std::abs(tip-3400)<1,"Blade ends at native sword-trail tip");
            }
        }
        Check(Stalfos(0,rank).empty() && Stalfos(61,rank).empty() && Stalfos(1,rank).empty(),"Root and unsupported limbs stay untouched");
        Check(counts[rank]<40000,"Per-actor triangle budget");
    }
    Check(counts[0]<counts[1] && counts[1]<counts[2],"Champion and elite have distinct geometry");
    std::cout<<"Validated "<<total<<" integer-safe triangles across 72 animated limb meshes; per rank "<<counts[0]<<", "<<counts[1]<<", "<<counts[2]<<".\n";
} catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
