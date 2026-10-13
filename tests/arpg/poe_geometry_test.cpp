#include "PoeGeometry.h"
#include <iostream>
#include <stdexcept>
using namespace ARPG::PoeGeometry;
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);}
int main(){try {
 size_t count=0;
 for(int limb:Limbs){auto mesh=Poe(limb);Check(!mesh.empty()&&mesh.size()%3==0,"Complete native rigid joint mesh");count+=mesh.size()/3;
  for(auto& v:mesh){auto p=v.surface.p,n=v.surface.normal;Check(v.joint==limb,"Every vertex follows its own original rigid joint");Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<9000&&std::abs(p.y)<9000&&std::abs(p.z)<9000,"Finite anatomy fits native local envelopes");Check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<.001,"Unit lighting normals");}
  for(size_t i=0;i<mesh.size();i+=3){auto a=mesh[i].surface.p,b=mesh[i+1].surface.p,c=mesh[i+2].surface.p;auto n=Cross(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));Check(n.x*n.x+n.y*n.y+n.z*n.z>0,"Quantized topology remains nondegenerate");}}
 Check(count>5000&&count<9500,"Bounded per-actor geometry");for(int l:{-1,0,1,2,3,6,7,8,12,13,14,15,17,21,255})Check(Poe(l).empty()&&!Visible(l),"Hidden native pivots stay hidden");
 for(auto& p:ARPG::PoeBlenderData::parts){Check(p.material<9&&Visible(p.limb),"Valid explicit material slots");}
 for(int i=0;i<9;++i){auto a=Tint(i,255,255,255),b=Tint(i,0,0,0);Check(a.r==Palette[i].r&&a.g==Palette[i].g&&a.b==Palette[i].b&&b.r==0&&b.g==0&&b.b==0,"Native per-channel tint preserves endpoints");}
 std::cout<<count<<" triangles; original nine joints, topology, lighting and explicit material roles passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
