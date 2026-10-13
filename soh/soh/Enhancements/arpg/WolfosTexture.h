#pragma once
#include "WolfosFurData.h"
#include "NativeMaterials.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/type/Texture.h>

namespace ARPG::WolfosTexture {
alignas(2) inline constexpr char Path[]="__OTR__arpg/wolfos/painted-fur-v2";
inline void EnsureLoaded() {
    auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
    if(manager->GetCachedResource(Path+7,true))return;
    auto init=std::make_shared<Ship::ResourceInitData>();init->Path=Path+7;
    auto texture=std::make_shared<Fast::Texture>(init);
    texture->Type=Fast::TextureType::RGBA32bpp;
    texture->Width=WolfosFurData::Width;texture->Height=WolfosFurData::Height;
    texture->Flags=TEX_FLAG_LOAD_AS_IMG;
    texture->HByteScale=32;texture->VPixelScale=16;
    texture->ImageDataSize=WolfosFurData::Width*WolfosFurData::Height*4;
    texture->mImageBuffer=std::make_shared<std::vector<char>>(texture->ImageDataSize);
    texture->ImageData=reinterpret_cast<uint8_t*>(texture->mImageBuffer->data());
    for(size_t i=0;i<sizeof(WolfosFurData::Pixels);++i) {
        for(int c=0;c<3;++c)texture->ImageData[i*4+c]=WolfosFurData::Pixels[i];
        texture->ImageData[i*4+3]=255;
    }
    // [TIME] | [INFO] | WolfosTexture.Load | Keep the authored map in the resource cache; no external texture pack is required.
    manager->CacheExternalResource(Path+7,texture);
}
inline Gfx* Bind(Gfx* p,GearGeometry::Color c) {
    EnsureLoaded();
    p=NativeMaterials::Bind(p,NativeMaterials::Surface::Fur,c);
    gDPLoadTextureBlock(p++,Path,G_IM_FMT_RGBA,G_IM_SIZ_16b,32,32,0,G_TX_WRAP,G_TX_WRAP,5,5,0,0);
    // [TIME] | [INFO] | WolfosTexture.Shade | Keep the painted detail and species colors readable under the original game's dark actor lighting.
    gDPSetCombineLERP(p++,TEXEL0,0,PRIM_LOD_FRAC,PRIMITIVE,0,0,0,1,COMBINED,0,SHADE,0,0,0,0,1);
    gDPSetPrimColor(p++,0,160,int(c.r)*3/5,int(c.g)*3/5,int(c.b)*3/5,255);
    return p;
}
}
