#pragma once
#include "ARPGTypes.h"
#include <string>

namespace ARPG::Inventory {
std::string EquipRestriction(const ARPGItemInstance& item);
bool Equip(int backpackIndex);
int GearSlot(int nativeSlot);
int NativeSlot(int gearSlot);
bool Assign(bool equipped, int index, int button, int nativeSlot = -1);
bool Unequip(int slot, int backpackIndex = -1);
void Open();
void Close(bool resume = true);
void ObservePause();
bool IsOpen();
bool CanCraft();
}
