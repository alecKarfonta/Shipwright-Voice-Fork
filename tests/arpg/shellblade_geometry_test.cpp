#include "ShellbladeGeometry.h"
#include <iostream>
#include <stdexcept>
#include <set>
using namespace ARPG::ShellbladeGeometry;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 for(int variant=0;variant<1;++variant){size_t triangles=0;std::set<int> joined;
  for(int limb:Limbs){auto mesh=Shellblade(limb,variant);Check(mesh.size()>30&&mesh.size()%3==0,"All native visible limbs retain geometry");triangles+=mesh.size()/3;
   for(auto v:mesh){Check(MatrixIndex(v.joint)>=0,"Every vertex references a native limb matrix");auto p=v.surface.p,n=v.surface.normal;
    Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<11000&&std::abs(p.y)<2400&&std::abs(p.z)<4500,"Finite geometry remains in the native local envelope");
    Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001f,"Normals remain unit length");if(v.joint!=limb)joined.insert(limb);}
  }
  Check(joined==std::set<int>({7}),"Adductor follows the native lower-to-upper muscle connection");Check(triangles>5000&&triangles<15000,"Per-actor triangle budget");std::cout<<"Geometry "<<variant<<": "<<triangles<<" triangles; one articulated connection.\n";
 }
 for(int limb:{1,2,3,5,9})Check(Shellblade(limb,0).empty(),"Hidden native joints remain empty");
 Check(Shellblade(3,-1).empty()&&Shellblade(4,1).empty(),"Unknown habitats rejected");Check(MatrixIndex(4)==0&&MatrixIndex(8)==3,"Native draw traversal palette order");
 for(int limb:{4,6})for(auto v:Shellblade(limb))Check(v.joint==limb,"Detached valves are rigid and independent of another skeleton matrix");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
