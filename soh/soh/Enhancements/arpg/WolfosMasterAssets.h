#pragma once
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

namespace ARPG::WolfosMasterAssets {
inline bool Available(bool white) {
    auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
    if(!manager->IsAltAssetsEnabled())return false;
    // [TIME] | [INFO] | WolfosMasterAssets.Available | The validated exporter writes every limb together; its private vertex path identifies that species' master pack.
    return manager->GetArchiveManager()->HasFile(white?"arpg/wolfos/master/white/limb-02.vtx":"arpg/wolfos/master/normal/limb-02.vtx");
}
}
