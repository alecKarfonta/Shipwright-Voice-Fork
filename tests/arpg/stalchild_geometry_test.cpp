#include "StalchildGeometry.h"
#include <iostream>
#include <stdexcept>
using namespace ARPG::StalchildGeometry;
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);}
int main(){try {
 size_t count=0;
 for(int limb:Limbs){auto mesh=Stalchild(limb);Check(!mesh.empty()&&mesh.size()%3==0,"Every native bone has complete triangles");count+=mesh.size()/3;
  for(auto& v:mesh){auto p=v.surface.p,n=v.surface.normal;Check(v.joint==limb,"Rigid meshes stay on the original joint and remain valid as debris");Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<2500&&std::abs(p.y)<2500&&std::abs(p.z)<2500,"Finite bone anatomy within the native joint envelope");Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001,"Unit normals support native lighting and damage filters");}
  for(size_t i=0;i<mesh.size();i+=3){auto a=mesh[i].surface.p,b=mesh[i+1].surface.p,c=mesh[i+2].surface.p;auto n=Cross(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));Check(n.x*n.x+n.y*n.y+n.z*n.z>0,"Integer export keeps nondegenerate triangles");}}
 Check(count>5000&&count<12000,"Bounded per-actor geometry");
 for(int l:{-1,0,1,3,20,255})Check(Stalchild(l).empty()&&!Visible(l),"Hidden and invalid native joints remain hidden");
 bool eyes=false,jaw=false,skull=false;
 for(auto& part:ARPG::StalchildBlenderData::parts){Check(part.material<6&&Visible(part.limb),"Valid material and limb");if(part.material==4){Check(part.limb==11,"Pulsing embers belong only to the breakable head");eyes=true;}jaw|=part.limb==12;skull|=part.limb==11;}
 Check(eyes&&jaw&&skull,"Skull and jaw are separate native breakable parts");std::cout<<count<<" triangles; all seventeen native bones, skull/jaw separation, normals and topology passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
