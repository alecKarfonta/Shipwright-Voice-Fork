#pragma once
#include "ARPGTypes.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace ARPG::AuraGeometry {
struct Point { float x,y,z; };
struct Ink { uint8_t r,g,b,a; };
struct Vertex { Point p; Ink ink; };
constexpr float pi=3.14159265359f;
constexpr Ink jade{99,231,161,210}, ivory{244,238,169,235}, gold{246,188,64,205},
              blue{100,194,255,220}, ember{255,107,40,225}, violet{154,110,243,190};
inline Ink Fade(Ink c,float f) { c.a=uint8_t(c.a*std::clamp(f,0.0f,1.0f));return c; }
inline Point Polar(float a,float r,float y=0) { return {std::sin(a)*r,y,std::cos(a)*r}; }
// [TIME] | [INFO] | AuraGeometry.Mesh | Original sigils and feather forms use a bounded mesh and no gameplay randomness.
struct Mesh {
    std::array<Vertex,1536> vertices{};
    int count=0;
    void Tri(Point a,Point b,Point c,Ink ink) {
        if(count+3>int(vertices.size())) { return; }
        vertices[count++]={a,ink}; vertices[count++]={b,ink}; vertices[count++]={c,ink};
    }
    void Quad(Point a,Point b,Point c,Point d,Ink ink) { Tri(a,b,c,ink);Tri(a,c,d,ink); }
    void Line(Point a,Point b,float width,Ink ink) {
        float dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z;
        float length=std::sqrt(dx*dx+dz*dz);
        Point side=length>.01f?Point{-dz/length*width,0,dx/length*width}:Point{width,0,0};
        Quad({a.x-side.x,a.y,a.z-side.z},{b.x-side.x,b.y,b.z-side.z},
             {b.x+side.x,b.y,b.z+side.z},{a.x+side.x,a.y,a.z+side.z},ink);
        if(std::abs(dy)<.01f) {
            Quad({a.x,a.y-width,a.z},{b.x,b.y-width,b.z},{b.x,b.y+width,b.z},{a.x,a.y+width,a.z},ink);
        }
    }
    void Ring(float radius,float y,float turn,Ink ink,int segments=32) {
        for(int i=0;i<segments;++i) { Line(Polar(turn+i*2*pi/segments,radius,y),Polar(turn+(i+1)*2*pi/segments,radius,y),1.1f,ink); }
    }
    void Gem(Point p,float width,float height,Ink ink) {
        Point l{p.x-width,p.y,p.z},r{p.x+width,p.y,p.z},t{p.x,p.y+height,p.z},b{p.x,p.y-height,p.z};
        Tri(l,t,{p.x,p.y,p.z+width*.5f},ink);Tri(t,r,{p.x,p.y,p.z+width*.5f},Fade(ivory,ink.a/255.f));
        Tri(r,b,{p.x,p.y,p.z+width*.5f},ink);Tri(b,l,{p.x,p.y,p.z+width*.5f},Fade(ink,.5f));
    }
    void Shield(Point p,float turn,float scale,Ink ink) {
        auto at=[&](float x,float y,float z=0) { return Point{p.x+std::cos(turn)*x*scale+std::sin(turn)*z*scale,p.y+y*scale,p.z-std::sin(turn)*x*scale+std::cos(turn)*z*scale}; };
        constexpr std::array<Point,6> rim={Point{-22,25,0},Point{22,25,0},Point{26,10,0},Point{18,-19,0},Point{0,-32,0},Point{-18,-19,0}};
        for(int i=0;i<6;++i) {
            auto a=rim[i],b=rim[(i+1)%6];
            Tri(at(a.x,a.y),at(b.x,b.y),at(0,0,5),Fade(ink,.24f));
            Line(at(a.x,a.y),at(b.x,b.y),1.4f,ink);
        }
        for(float sign:{-1.f,1.f}) {
            Line(at(sign*12,-17,2),at(-sign*3,-3,6),1.3f,Fade(ivory,ink.a/255.f));
            Line(at(-sign*3,-3,6),at(sign*9,15,3),1.3f,Fade(ivory,ink.a/255.f));
            Line(at(sign*4,6,5),at(sign*17,12,2),1,ink);
        }
        Tri(at(0,10,7),at(-5,0,7),at(0,-7,7),ink);
        Tri(at(0,10,7),at(0,-7,7),at(5,0,7),Fade(ivory,ink.a/255.f));
    }
    void Feather(Point p,float turn,float scale,Ink ink) {
        auto at=[&](float x,float y) { return Point{p.x+std::cos(turn)*x*scale,p.y+y*scale,p.z-std::sin(turn)*x*scale}; };
        Line(at(0,-12),at(0,16),.65f,Fade(ivory,ink.a/255.f));
        for(int i=0;i<5;++i) {
            float y=-8.f+i*4.f,w=7-i*.8f;
            Tri(at(0,y-2),at(-w,y+6),at(0,y+4),ink);
            Tri(at(0,y-2),at(0,y+4),at(w,y+6),Fade(ink,.65f));
        }
    }
};
inline bool Supported(int id) {
    return id==ARPG_SKILL_MAGIC_INFUSE || id==ARPG_SKILL_MAGIC_HOLY_AURA || id==ARPG_SKILL_MAGIC_RECALL ||
           id==ARPG_SKILL_GUARD_RIPOSTE || id==ARPG_SKILL_GUARD_WARCRY || id==ARPG_SKILL_GUARD_DODGE || id==ARPG_SKILL_GUARD_SECOND_WIND;
}
inline Mesh Build(int id,int rune,int age,int remaining,int element=0) {
    Mesh mesh;
    if(!Supported(id) || remaining<=0 || age<0) { return mesh; }
    float alpha=std::min({1.f,(age+1)/5.f,remaining/10.f});
    float turn=age*.055f;
    Ink tint=Fade(element==1?blue:element==2?gold:ember,alpha);
    switch(id) {
        case ARPG_SKILL_MAGIC_INFUSE:
            mesh.Ring(13+2*std::sin(turn),0,turn,tint,24);
            mesh.Ring(9,7,-turn,Fade(ivory,alpha*.7f),16);
            for(int i=0;i<3;++i) { mesh.Gem(Polar(turn+i*2*pi/3,18,4+std::sin(turn+i)*5),2.5f,rune==2?12.f:7.f,tint); }
            break;
        case ARPG_SKILL_GUARD_RIPOSTE:
            mesh.Shield({0,39,34},0,1.15f,Fade(rune==2?ember:jade,alpha*(.8f+.2f*std::sin(turn))));
            mesh.Ring(41,3,0,Fade(gold,alpha*.45f));
            break;
        case ARPG_SKILL_MAGIC_HOLY_AURA:
            mesh.Ring(56,4,turn,Fade(gold,alpha*.6f));
            for(int i=0;i<3;++i) { float a=turn*.3f+i*2*pi/3;mesh.Shield(Polar(a,51,38),a,.55f,Fade(rune==2?violet:blue,alpha*.7f)); }
            break;
        case ARPG_SKILL_GUARD_WARCRY:
            mesh.Ring(60,3,turn,Fade(gold,alpha*.65f));
            mesh.Ring(48,4,-turn,Fade(jade,alpha*.4f),16);
            for(int i=0;i<6;++i) {
                float a=i*pi/3+turn*.2f;
                mesh.Gem(Polar(a,55,10+5*std::sin(turn*2+i)),4,8,Fade(rune==1?ember:jade,alpha));
                mesh.Line(Polar(a,42,4),Polar(a+.3f,58,4),1.2f,Fade(ivory,alpha*.7f));
            }
            if(age<20) { mesh.Ring(20.f+age*6.f,5,0,Fade(gold,(1-age/20.f)*alpha)); }
            break;
        case ARPG_SKILL_GUARD_DODGE:
            for(int i=0;i<7;++i) {
                auto p=Polar(i*2.4f,20+age*1.5f,10+(age+i)*1.3f); p.z+=age*2;
                mesh.Feather(p,turn+i, .5f+(i%3)*.15f,Fade(rune==2?violet:jade,alpha));
            }
            break;
        case ARPG_SKILL_MAGIC_RECALL:
            for(int i=0;i<3;++i) { mesh.Ring(32.f+i*12.f,4+i*18+age*1.5f,(i%2?1:-1)*turn,Fade(jade,alpha*.7f),24); }
            for(int i=0;i<5;++i) { mesh.Feather(Polar(turn+i*2*pi/5,45,10.f+age*2.f),turn+i,.7f,Fade(ivory,alpha)); }
            break;
        case ARPG_SKILL_GUARD_SECOND_WIND:
            for(int i=0;i<4;++i) {
                float a=turn+i*pi/2;
                mesh.Feather(Polar(a,26,10.f+static_cast<float>((age*2+i*12)%70)),a,.65f,Fade(jade,alpha));
            }
            break;
    }
    return mesh;
}
}
