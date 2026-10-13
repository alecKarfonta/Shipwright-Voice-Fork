#include "WaystoneGeometry.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace ARPG;
int main() {
    size_t triangles=0;
    std::set<size_t> emblems;
    for(int part=0;part<Waystone::PartCount;++part) {
        auto mesh=Waystone::Build(part,0);
        assert(!mesh.empty() && mesh.size()%3==0 && mesh.size()/3<6000);
        if(part>=Waystone::Iron && part<=Waystone::Storm) { emblems.insert(mesh.size()); }
        triangles+=mesh.size()/3;
        for(size_t i=0;i<mesh.size();i+=3) {
            GearGeometry::Point quantized[3];
            for(int j=0;j<3;++j) {
                auto& v=mesh[i+j];auto p=v.p,n=v.normal;
                assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
                assert(std::abs(p.x)<25 && p.y>-12 && p.y<58 && std::abs(p.z)<36);
                assert(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001f);
                quantized[j]={std::round(p.x*100),std::round(p.y*100),std::round(p.z*100)};
                // [TIME] | [INFO] | WaystoneTest.Hinge | Check closed and open world-space lid envelopes, not only its local mesh.
                if(part==Waystone::Lid) for(float open:{0.f,.25f,.5f,.75f,1.f}) {
                    float a=Waystone::LidAngle(open),y=14+p.y*std::cos(a)-p.z*std::sin(a),z=15+p.y*std::sin(a)+p.z*std::cos(a);
                    assert(y>12 && y<33 && z>12 && z<34);
                }
            }
            auto ab=GearGeometry::Add(quantized[1],GearGeometry::Mul(quantized[0],-1));
            auto ac=GearGeometry::Add(quantized[2],GearGeometry::Mul(quantized[0],-1));
            auto area=GearGeometry::Cross(ab,ac);assert(area.x*area.x+area.y*area.y+area.z*area.z>0);
        }
        for(int tone=1;tone<7;++tone) { auto variant=Waystone::Build(part,tone);assert(variant.size()==mesh.size()); }
    }
    assert(emblems.size()==5 && triangles<18000);
    assert(Waystone::Build(-1).empty() && Waystone::Build(Waystone::PartCount).empty());
    // [TIME] | [INFO] | WaystoneTest.Material | Oaths and completion must be visibly different; dark sockets must not retain an active glow color.
    std::set<uint32_t> tones;
    for(int tone=0;tone<7;++tone) {
        auto c=Waystone::Build(Waystone::FirstLamp,tone).front().color;
        tones.insert((uint32_t(c.r)<<16)|(uint32_t(c.g)<<8)|c.b);
    }
    assert(tones.size()==7);
    std::cout<<"15 Blender components; 5 oath emblems; 7 tones; "<<triangles<<" integer-safe triangles; articulated lid envelopes passed\n";
}
