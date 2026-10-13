#include "TektiteGeometry.h"
#include <iostream>
#include <stdexcept>
using namespace ARPG::TektiteGeometry;
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);}
int main(){try{
    size_t total=0;
    for(int variant=0;variant<2;++variant){size_t count=0;
        for(int limb:Limbs){
            auto mesh=Tektite(limb,variant);
            Check(mesh.size()>30 && mesh.size()%3==0,"Every visible native limb is modeled");
            for(auto v:mesh){
                Check(std::isfinite(v.p.x)&&std::isfinite(v.p.y)&&std::isfinite(v.p.z),"Finite vertices");
                Check(std::abs(v.p.x)<3700 && std::abs(v.p.y)<1200 && std::abs(v.p.z)<1600,"Model remains within native Tektite proportions");
                Check(std::abs(v.normal.x*v.normal.x+v.normal.y*v.normal.y+v.normal.z*v.normal.z-1)<.001f,"Unit surface normals");
            }
            for(size_t i=0;i<mesh.size();i+=3){
                auto a=mesh[i].p,b=mesh[i+1].p,c=mesh[i+2].p;auto n=Cross(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));
                Check(n.x*n.x+n.y*n.y+n.z*n.z>0,"No collapsed native integer triangles");
            }
            count+=mesh.size()/3;
        }
        Check(count>=5000 && count<6500,"Articulated Tektites retain a bounded render cost");
        Check(Tektite(0,variant).empty() && Tektite(12,variant).empty() && Tektite(25,variant).empty(),"Invisible rig nodes stay empty");
        std::cout<<"Variant "<<variant<<": "<<count<<" triangles\n";total+=count;
    }
    std::cout<<"Validated "<<total<<" native integer triangles across all 18 limbs.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
