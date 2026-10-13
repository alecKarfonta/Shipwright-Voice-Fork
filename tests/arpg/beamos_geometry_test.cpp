#include "BeamosGeometry.h"
#include <iostream>
#include <stdexcept>
using namespace ARPG::BeamosGeometry;
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);}
int main(){try {
 size_t count=0;bool eye=false;
 for(int limb:Limbs){auto mesh=Beamos(limb);Check(!mesh.empty()&&mesh.size()%3==0,"Complete native rigid joint mesh");count+=mesh.size()/3;
  for(auto& v:mesh){auto p=v.surface.p,n=v.surface.normal;Check(v.joint==limb,"Every vertex follows its own original rigid joint");Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<2000&&std::abs(p.y)<5100&&std::abs(p.z)<2400,"Finite anatomy fits native local envelopes");Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001,"Unit lighting normals");}
  for(size_t i=0;i<mesh.size();i+=3){auto a=mesh[i].surface.p,b=mesh[i+1].surface.p,c=mesh[i+2].surface.p;auto n=Cross(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));Check(n.x*n.x+n.y*n.y+n.z*n.z>0,"Quantized topology remains nondegenerate");}}
 Check(count>3000&&count<8000,"Bounded per-actor geometry");for(int l:{-1,0,1,2,3,4,6,7,11,255})Check(Beamos(l).empty()&&!Visible(l),"Hidden native pivots stay hidden");
 for(auto& p:ARPG::BeamosBlenderData::parts){Check(p.material<7&&Visible(p.limb),"Valid explicit material slots");if(Surfaces[p.material]==ARPG::BeamosGeometry::Surface::Glow){Check(p.limb==9,"Only inset lens emits light");eye=true;}}
 Check(eye,"Distinct inset eye material");std::cout<<count<<" triangles; original four joints, topology, lighting and explicit material roles passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
