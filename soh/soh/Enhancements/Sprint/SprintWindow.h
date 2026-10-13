#pragma once

#include <ship/window/gui/GuiWindow.h>

// [TIME] | [INFO] | SprintWindow | in-game stamina bar overlay shown while the sprint enhancement is on
class SprintWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override;
    void DrawElement() override{};
    void Draw() override;
    void UpdateElement() override{};
};
