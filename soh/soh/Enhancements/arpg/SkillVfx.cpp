#include "SkillVfx.h"
#include "NativeEffects.h"

namespace ARPG::SkillVfx {
using namespace NativeEffects;
void Draw(Actor* actor,PlayState* play,int id,int rune,int age,int duration,const Vec3f& origin,
          int16_t yaw,const SkillPatterns::Frame& shapes,const Vec3f* chain,int chainCount) {
    if(!play || age<0 || age>=duration)return;
    Mesh fx(play,yaw);
    float fade=std::min(1.f,(duration-age)/6.f),radius=SkillRules::Definition(id).radius;
    auto cloud=[&](Point center,float r,int t,Color color,Texture texture=Texture::Dust) {
        if(t<0 || t>19)return;
        for(int i=0;i<7;++i) {
            float a=i*2.39996f,travel=r*(.25f+t*.035f);
            Point p=Add(center,{std::sin(a)*travel,6.f+t*1.6f,std::cos(a)*travel});
            float size=7+t*.8f;
            fx.Sprite(texture,p,size,texture==Texture::Fire?size*1.8f:size,Fade(color,(1-t/20.f)*fade));
        }
    };
    switch(id) {
        case ARPG_SKILL_SWORD_CLEAVE:
        case ARPG_SKILL_SWORD_CROSSCUT: {
            int t=id==ARPG_SKILL_SWORD_CROSSCUT?age%16:age;
            if(t>10)break;
            float sweep=rune==1 && id==ARPG_SKILL_SWORD_CLEAVE?2*pi:2.65f;
            float tilt=id==ARPG_SKILL_SWORD_CROSSCUT?(age/16%2?-32.f:32.f):8.f;
            float z=rune==2 && id==ARPG_SKILL_SWORD_CROSSCUT?t*12.f:0;
            fx.Arc(radius,13,35,-sweep/2+(t-4)*.11f,sweep,Fade(steel,std::sin((t+1)*pi/12)),z,tilt);
            break;
        }
        case ARPG_SKILL_SWORD_CYCLONE:
            radius*=rune==1?1.4f:1;
            fx.Arc(radius,14,32,age*.31f,4.9f,Fade(steel,fade),0,8);
            fx.Arc(radius*.88f,6,21,age*.31f+pi,3.5f,Fade(steel,fade*.5f));
            break;
        case ARPG_SKILL_SWORD_LUNGE: {
            float end=rune==2?300.f:150.f;
            for(int i:{-1,1})fx.Line({i*12.f,30,-45},{i*8.f,32,end},3,Fade(steel,fade*.8f));
            cloud({0,0,-20},22,age,dust);break;
        }
        case ARPG_SKILL_SWORD_BLADESTORM:
            for(int i=0;i<shapes.count;++i) {
                auto s=shapes.shapes[i];float a=age*.24f+i*2.094f;
                fx.Arc(radius*(rune==1?1.45f:1),10,36,a-1.4f,1.4f,Fade(steel,fade),0,6);
                fx.Sprite(Texture::Glow,{s.x,36,s.z},5,5,Fade(steel,fade));
            }break;
        case ARPG_SKILL_SWORD_EXECUTE: {
            if(age<8)fx.Line({0,130-age*12.f,radius*.7f},{0,25,radius*.7f},5,Fade(gold,.75f));
            if(age>=6) {
                int t=age-6;
                if(rune==1) {
                    float r=shapes.count?shapes.shapes[0].radius:radius;
                    fx.Arc(r,8,3,0,2*pi,Fade(dust,fade));cloud({0,0,0},r*.65f,t,dust);
                } else {
                    fx.Arc(radius,8,3,-1.2f,2.4f,Fade(dust,fade));cloud({0,0,radius*.65f},radius*.35f,t,dust);
                }
            }
            break;
        }
        case ARPG_SKILL_SWORD_SHOCKWAVE:
            for(int i=0;i<shapes.count;++i) {
                auto s=shapes.shapes[i];int t=rune==1?age%28:age-i*3;
                fx.Arc(s.radius,7,3,0,2*pi,Fade(dust,fade),s.z,0,s.x);
                cloud({s.x,0,s.z},s.radius,t,dust);
            }break;
        case ARPG_SKILL_SWORD_BEAM:
            for(int i=0;i<shapes.count;++i) {
                auto s=shapes.shapes[i];
                fx.Line({s.x,8,s.z-5},{s.x,85,s.z+8},7,Fade(ice,fade));
                fx.Sprite(Texture::Glow,{s.x,42,s.z},12,27,Fade(ice,fade*.55f));
            }break;
        case ARPG_SKILL_MAGIC_FLAME_BURST: {
            int t=rune==1?age%22:age;float r=radius*std::min(1.f,(t+1)/6.f);
            for(int i=0;i<10;++i) {
                float a=i*2*pi/10;float h=19+std::sin(i*3+age*.5f)*5;
                fx.Sprite(Texture::Fire,{std::sin(a)*r*.83f,h,std::cos(a)*r*.83f},13,h,Fade(fire,fade));
            }
            break;
        }
        case ARPG_SKILL_MAGIC_FROST_NOVA: {
            float r=radius*(rune==2?.8f:1)*std::min(1.f,(age+1)/6.f);
            fx.Arc(r,9,4,0,2*pi,Fade(ice,fade));cloud({0,2,0},r,age,ice);
            for(int i=0;i<6;++i) {float a=i*pi/3;fx.Sprite(Texture::Glow,{std::sin(a)*r*.8f,12,std::cos(a)*r*.8f},4,9,Fade(ice,fade));}
            break;
        }
        case ARPG_SKILL_MAGIC_CHAIN: {
            Point previous{0,35,0};float sn=Math_SinS(yaw),cs=Math_CosS(yaw);
            for(int i=0;i<chainCount;++i) {
                float x=chain[i].x-origin.x,z=chain[i].z-origin.z;
                Point target{cs*x-sn*z,chain[i].y-origin.y+35,sn*x+cs*z};int t=age-i*4;
                if(t>=0 && t<12) {
                    fx.Line(previous,target,7,Fade(ice,1-t/12.f),Texture::Lightning);
                    fx.Sprite(Texture::Glow,target,10,10,Fade(ice,1-t/12.f));
                }
                previous=target;
            }break;
        }
        case ARPG_SKILL_MAGIC_METEOR:
            for(int i=0;i<(rune==1?3:1);++i) {
                int t=age-i*14;if(t<0 || t>47)continue;float z=i*130.f,r=radius*(rune==2?1.5f:1);
                if(t<28) {
                    // Keep the gameplay footprint visible, with the same ground-plane reach.
                    fx.Arc(r,4,3,0,2*pi,Fade(fire,.65f),z);
                    float h=(28-t)*19.f;Point p{h*.36f,h,z};
                    fx.Sprite(Texture::Fire,p,22,32,fire);
                    fx.Line(p,{p.x+40,p.y+110,z},12,Fade(fire,.6f));
                } else {
                    int impact=t-28;float a=1-impact/20.f;
                    fx.Arc(r*std::min(1.f,(impact+1)/6.f),10,4,0,2*pi,Fade(fire,a),z);
                    cloud({0,0,z},r,impact,fire,Texture::Fire);
                    cloud({0,0,z},r,impact,dust);
                }
            }break;
        default:
            fx.Arc(radius,8,8,age*.12f,2*pi,Fade(gold,fade));break;
    }
    fx.Draw(play,origin,yaw,actor);
}
}
