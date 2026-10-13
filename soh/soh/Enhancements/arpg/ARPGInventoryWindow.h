#pragma once
#include <ship/window/gui/GuiWindow.h>
#include "ARPGTypes.h"
#include <string>

class ARPGInventoryWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void Draw() override;
  protected:
    void InitElement() override {}
    void UpdateElement() override {}
    void DrawElement() override;
  private:
    int selectedIndex = 0;
    bool selectedEquipment = false;
    int filter = -1;
    char search[96] = {};
    void Card(const ARPGItemInstance& item, int index, bool equipment, ImVec2 size);
    void Inspector();
    void CSlots();
    void SocketArtwork(const ARPGItemInstance& item, float size, bool interactive);
    void SocketWorkbench();
    bool InsertStone(bool equipped, int index, int material, int socket = -1);
    bool socketing = false, focusSocketStone = false;
    int selectedSocket = -1;
    std::string socketNotice;
    void MaterialInspector();
    bool MoveItem(bool sourceEquipment, int sourceIndex, bool targetEquipment, int targetIndex);
    void Forge();
    void Character();
    void More();
    int detailTab = 0, utilityPage = 0;
    bool campaignPage = true;
    bool showFilters = false;
    int selectedNativeSlot = -1;
    bool inspectNative = false;
    bool resetDetailsScroll = false;
    bool compactInspector = false;
    bool inspectItem = false;
    bool inspectMaterial = false;
    bool compactLoadout = false;
    int stashTab = 0;
    void SelectPage(int page);
    int CurrentPage() const;
    int recipeIndex = 1;
    bool forgeMode = false;
    int forgeTab = 0, gemRank = 0, selectedMaterial = 1;
    int salvageArmed = -1;
    int forgeOperation = 0;
    bool forgeChooseItem = false;
    bool forgeChooseOperation = false;
    bool forgeChooseRecipe = false;
    std::string forgeNotice;
};
