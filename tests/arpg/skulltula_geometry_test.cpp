#include "SkulltulaGeometry.h"
#include <iostream>
#include <stdexcept>
using namespace ARPG::SkulltulaGeometry;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    size_t count=0;
    for(int limb:Limbs){auto mesh=Skulltula(limb);Check(mesh.size()>30 && mesh.size()%3==0,"Each visible joint has triangle geometry");
        for(auto v:mesh){Check(std::isfinite(v.p.x)&&std::isfinite(v.p.y)&&std::isfinite(v.p.z),"Finite coordinates");
            Check(std::abs(v.p.x)<2500 && std::abs(v.p.y)<4000 && std::abs(v.p.z)<2500,"Geometry fits original local proportions");
            Check(std::abs(v.normal.x*v.normal.x+v.normal.y*v.normal.y+v.normal.z*v.normal.z-1)<.001f,"Normalized normals");}
        for(size_t i=0;i<mesh.size();i+=3){auto a=mesh[i].p,b=mesh[i+1].p,c=mesh[i+2].p;auto n=Cross(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));Check(n.x*n.x+n.y*n.y+n.z*n.z>0,"Integer triangles retain area");}
        count+=mesh.size()/3;
    }
    Check(count>3500 && count<5000,"Bounded rendering cost");
    Check(Skulltula(0).empty()&&Skulltula(7).empty()&&Skulltula(30).empty(),"Hidden native joints stay empty");
    std::cout<<"All 10 native limbs: "<<count<<" nondegenerate triangles.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
