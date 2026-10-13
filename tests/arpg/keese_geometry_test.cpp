#include "KeeseGeometry.h"
#include <iostream>
#include <stdexcept>
using namespace ARPG::KeeseGeometry;
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);}
int main(){try{
    size_t total=0;
    for(int variant=0;variant<3;++variant){size_t count=0;
        for(int limb:Limbs){
            auto mesh=Keese(limb,variant);
            Check(mesh.size()>30 && mesh.size()%3==0,"Every visible native limb is modeled");
            for(auto v:mesh){
                Check(std::isfinite(v.p.x)&&std::isfinite(v.p.y)&&std::isfinite(v.p.z),"Finite vertices");
                Check(std::abs(v.p.x)<2750 && std::abs(v.p.y)<2100 && std::abs(v.p.z)<1050,"Model remains within native Keese proportions");
                Check(std::abs(v.normal.x*v.normal.x+v.normal.y*v.normal.y+v.normal.z*v.normal.z-1)<.001f,"Unit surface normals");
            }
            for(size_t i=0;i<mesh.size();i+=3){
                auto a=mesh[i].p,b=mesh[i+1].p,c=mesh[i+2].p;auto n=Cross(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));
                Check(n.x*n.x+n.y*n.y+n.z*n.z>0,"No collapsed native integer triangles");
            }
            if(limb==15 || limb==16 || limb==17 || limb==22 || limb==23 || limb==24){
                int positive=0,negative=0;
                for(auto v:mesh){positive+=v.normal.z>.8;negative+=v.normal.z<-.8;}
                Check(positive>12 && negative>12,"Membranes have correctly lit surfaces on both sides");
            }
            count+=mesh.size()/3;
        }
        Check(count>=1500 && count<2500,"Small flying enemies retain a bounded render cost");
        Check(Keese(0,variant).empty() && Keese(14,variant).empty() && Keese(28,variant).empty(),"Invisible rig nodes stay empty");
        std::cout<<"Variant "<<variant<<": "<<count<<" triangles\n";total+=count;
    }
    Check(Variant(0,true)==1 && Variant(1,true)==1 && Variant(2,false)==0 && Variant(3,false)==0 && Variant(4,false)==2,"Native elemental and perch types map correctly");
    Check(Keese(27,-1).empty() && Keese(27,3).empty(),"Invalid variants produce no geometry");
    std::cout<<"Validated "<<total<<" native integer triangles across all 30 limbs.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
