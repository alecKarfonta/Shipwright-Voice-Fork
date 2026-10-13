#include "RedeadGeometry.h"
#include <iostream>
#include <stdexcept>
#include <set>
using namespace ARPG::RedeadGeometry;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 for(int variant=0;variant<2;++variant){size_t count=0;std::set<int> joints;
  for(int limb:Limbs){auto mesh=Undead(limb,variant);Check(mesh.size()>30 && mesh.size()%3==0,"Visible native limbs have triangle geometry");
   for(auto v:mesh){Check(MatrixIndex(v.joint)>=0,"Every vertex uses an existing flex matrix");joints.insert(v.joint);auto p=v.surface.p,n=v.surface.normal;
    Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"Finite coordinates");Check(std::abs(p.x)<4000&&std::abs(p.y)<4000&&std::abs(p.z)<4000,"Bounded native local coordinates");Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001f,"Unit normals");}
   count+=mesh.size()/3;
  }
  Check(joints.size()==15 && count>4500 && count<16000,"Complete rig with bounded rendering cost");Check(Undead(0,variant).empty()&&Undead(23,variant).empty(),"Dummy joints remain hidden");
  std::cout<<"Variant "<<variant<<": "<<count<<" triangles on "<<joints.size()<<" native matrices.\n";
 }
 Check(Undead(24,-1).empty()&&Undead(24,2).empty(),"Unknown species rejected");Check(MatrixIndex(3)==0&&MatrixIndex(25)==14&&MatrixIndex(23)==-1,"Native depth-first palette order");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
