#pragma once
#include <functional>
#include <string>
namespace ARPG::AdventureJournal {
inline constexpr const char* NativeItemPayload = "ARPG_NATIVE_ITEM";
enum class Page { Items, BaseGear, Quest, Map, Save };
void Draw(Page page);
void StopSong();
void CancelSave();
void Reset();
int DrawItemGrid(int selectedSlot, const std::function<void(int)>& dropTarget = {});
void DrawItemDetails(int slot, bool header = true, const std::function<void(int)>& assign = {},
                     bool showAssignments = true);
void DrawAssignmentButtons(int slot, const std::function<void(int)>& assign = {});
void DrawCSlotButtons(int slot, float width, bool active = true,
                      const std::function<void(int)>& assign = {}, const std::string& restriction = {});
int DrawBottleSelector(int slot, float width);
bool DrawNativeItemCard(int slot, float width, float height, bool selected = false,
                        bool controllerNavigation = false);
bool CanAssignItem(int slot);
void DrawEquipmentAssignmentButtons(int type, int value, const std::function<void(int)>& assign);
bool AssignItem(int slot, int button);
bool AssignEquipment(int type, int value, int button);
bool EquipBase(int type, int value);
bool Save();
}
