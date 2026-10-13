#include "WolfosGeometry.h"
#include "wolfos_bind_fixture.h"
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using namespace ARPG::WolfosGeometry;
void Check(bool ok,const char* label) { if(!ok) { throw std::runtime_error(label); } }
Point World(const SkinVertex& v) {
    const auto& a=WolfosBindWorld[v.joint];auto p=Mul(v.surface.p,.01f);
    return {a[0]*p.x+a[1]*p.y+a[2]*p.z+a[3],a[4]*p.x+a[5]*p.y+a[6]*p.z+a[7],a[8]*p.x+a[9]*p.y+a[10]*p.z+a[11]};
}
int main() { try {
    size_t total=0,bridges=0,furTriangles=0,mappedTriangles=0;std::array<size_t,3> counts{};
    for(bool white:{false,true}) { for(int rank=0;rank<3;++rank) {
        size_t actorCount=0;std::set<int> connected;
        for(int limb:Limbs) {
            int variants=limb==18?3:1;
            for(int blink=0;blink<variants;++blink) {
                auto mesh=Wolfos(limb,rank,white,blink);
                if(limb==15 || limb==21) {
                    float palmY=0,backY=0,clawZ=0;int pads=0,backs=0,claws=0;
                    for(const auto& v:mesh) {
                        if(v.material==3){palmY+=World(v).y;++pads;}
                        if(v.material==0 && v.joint==limb){backY+=World(v).y;++backs;}
                        if(v.material==4){clawZ+=v.surface.p.z;++claws;}
                    }
                    Check(pads && backs && palmY/pads<backY/backs,"Both palms sit underneath the furred backs of the hands in the native waiting pose");
                    Check(claws && (limb==15?clawZ<0:clawZ>0),"Left and right thumbs have opposite handedness");
                }
                Check(mesh.size()>36 && mesh.size()%3==0 && mesh.size()<33000,"Bounded mesh for every visible limb and blink state");
                for(auto& v:mesh) {
                    auto& s=v.surface;auto n=s.normal;
                    Check(MatrixIndex(v.joint)>=0 && MatrixIndex(v.joint)<19,"Every skin vertex refers to a native palette matrix");
                    Check(std::isfinite(s.p.x) && std::isfinite(s.p.y) && std::isfinite(s.p.z),"Finite vertex positions");
                    Check(std::abs(s.p.x)<5000 && std::abs(s.p.y)<3600 && std::abs(s.p.z)<2400,"Vertices fit the native mesh envelope");
                    Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001f,"Unit skin normals");
                    s.p={std::round(s.p.x),std::round(s.p.y),std::round(s.p.z)};
                }
                for(size_t i=0;i<mesh.size();i+=3) {
                    if(mesh[i].material<3) {
                        ++furTriangles;
                        const auto& u=mesh[i];const auto& v=mesh[i+1];const auto& w=mesh[i+2];
                        Check(u.material==v.material && u.material==w.material,"Material boundaries remain whole triangles");
                        Check(std::abs(u.s-v.s)<=1024 && std::abs(u.s-w.s)<=1024,"Cylindrical seam does not cross the entire texture");
                        if((v.s-u.s)*(w.t-u.t)-(w.s-u.s)*(v.t-u.t)!=0)++mappedTriangles;
                    }
                    auto a=World(mesh[i]),b=World(mesh[i+1]),c=World(mesh[i+2]);
                    auto n=Cross(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));
                    if(n.x*n.x+n.y*n.y+n.z*n.z<1e-10f) {
                        throw std::runtime_error("Collapsed integer triangle: limb "+std::to_string(limb)+" triangle "+std::to_string(i/3));
                    }
                    if(mesh[i].joint!=mesh[i+1].joint || mesh[i].joint!=mesh[i+2].joint) { ++bridges;connected.insert(limb); }
                    ++total;if(blink==0) { ++actorCount; }
                }
                if(limb==18) {
                    float ymin=10000,ymax=-10000;
                    for(const auto& v:mesh) { float y=v.surface.p.y-.22f*v.surface.p.x;ymin=std::min(ymin,y);ymax=std::max(ymax,y); }
                    Check(blink!=2 || ymax-ymin<45,"Narrow native blink visibly closes the eyes");
                }
            }
        }
        Check(connected.size()==17,"All flex skin connections span both native matrices");
        std::cout<<"Rank "<<rank<<" white "<<white<<": "<<actorCount<<" triangles\n";
        Check(actorCount<40000,"Wolfos stays within its per-actor triangle budget");
        if(!white) { counts[rank]=actorCount; }
        Check(Wolfos(0,rank,white).empty() && Wolfos(1,rank,white).empty() && Wolfos(16,rank,white).empty(),"Empty rig nodes remain empty");
    } }
    Check(counts[0]<counts[1] && counts[1]<counts[2],"Rank variants have distinct fur and accessory geometry");
    Check(furTriangles>0 && mappedTriangles>furTriangles*.9,"Authored UVs cover the coat rather than collapsing to a flat texel");
    std::cout<<"Validated "<<total<<" triangles, "<<bridges<<" mixed-matrix skin triangles, 126 limb variants; per actor "
             <<counts[0]<<", "<<counts[1]<<", "<<counts[2]<<".\n";
} catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
