#include "MoblinGeometry.h"
#include <iostream>
#include <stdexcept>
#include <set>
using namespace ARPG::MoblinGeometry;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 for(int variant=0;variant<2;++variant){size_t count=0;std::set<int> joints,connections;float weaponMinimum=0;
  for(int limb:Limbs[variant]){auto mesh=Moblin(limb,variant);Check(mesh.size()>30&&mesh.size()%3==0,"Every visible original limb has complete triangles");
   for(auto v:mesh){Check(MatrixIndex(v.joint,variant)>=0,"Every vertex uses an existing native matrix");joints.insert(v.joint);if(v.joint!=limb)connections.insert(limb);auto p=v.surface.p,n=v.surface.normal;
    Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<4500&&std::abs(p.y)<9200&&std::abs(p.z)<4500,"Finite native-local geometry within bounded anatomy and weapon reach");
    Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001f,"Quantized normals normalized for lighting");if(limb==(variant?12:17))weaponMinimum=std::min(weaponMinimum,p.y);}
   count+=mesh.size()/3;
  }
  Check(joints.size()==15&&connections.size()==12,"Fifteen original flex matrices and twelve joint connections");Check(count>10000&&count<14000,"Bounded geometry budget for both native actor types");
  Check(weaponMinimum<(variant?-8400:-7200)&&weaponMinimum>(variant?-9100:-7400),"Rebuilt weapons retain their original native reach");
  for(int limb:{0,1,2,4,8,10,13,15,18,19,23,24,28})Check(Moblin(limb,variant).empty(),"Original hidden joints remain hidden");
  Check(MatrixIndex(variant?5:6,variant)==1&&MatrixIndex(variant?6:5,variant)==-1,"Different native chest joints retain the correct palette index");
  std::cout<<"Variant "<<variant<<": "<<count<<" triangles; 15 native matrices; 12 joint connections.\n";
 }
 Check(Moblin(3,-1).empty()&&Moblin(3,2).empty()&&MatrixIndex(3,2)==-1,"Invalid variants cannot address geometry or matrices");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
