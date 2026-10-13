#pragma once

// [TIME] | [INFO] | ARPGWindow.h | ImGui debug/config window for the ARPG layer

#include <ship/window/gui/GuiWindow.h>
#include "soh/Enhancements/local-npc/NpcDialogue.h"

class ARPGWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    ~ARPGWindow() { LocalNpc::Shutdown(); }
    void Draw() override;

  protected:
    void InitElement() override{};
    void DrawElement() override;
    void UpdateElement() override{};
};
