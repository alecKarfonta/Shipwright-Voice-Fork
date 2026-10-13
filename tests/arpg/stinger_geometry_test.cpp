#include "StingerGeometry.h"
#include <iostream>
#include <stdexcept>
#include <set>
using namespace ARPG::StingerGeometry;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 for(int variant=0;variant<2;++variant){size_t triangles=0;std::set<int> joined;
  for(int limb:Limbs){auto mesh=Stinger(limb,variant);Check(mesh.size()>30&&mesh.size()%3==0,"All native visible limbs retain geometry");triangles+=mesh.size()/3;
   for(auto v:mesh){Check(MatrixIndex(v.joint)>=0,"Every vertex references a native limb matrix");auto p=v.surface.p,n=v.surface.normal;
    Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<6500&&std::abs(p.y)<1800&&std::abs(p.z)<6500,"Finite geometry remains in the native local envelope");
    Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001f,"Normals remain unit length");if(v.joint!=limb)joined.insert(limb);}
  }
  Check(joined==std::set<int>({4,5,6,12,14,15,17,18}),"Head, tail and both fin chains have connected joint surfaces");Check(triangles>5000&&triangles<12000,"Per-actor triangle budget");std::cout<<"Habitat "<<variant<<": "<<triangles<<" triangles; eight articulated connections.\n";
 }
 for(int limb:{1,2,7,8,11,13,16,19})Check(Stinger(limb,0).empty(),"Hidden native joints remain empty");
 Check(Stinger(3,-1).empty()&&Stinger(3,2).empty(),"Unknown habitats rejected");Check(MatrixIndex(3)==0&&MatrixIndex(18)==10,"Native draw traversal palette order");
 Check(Palette[0][0].b!=Palette[1][0].b,"Land and water have distinct palettes");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
