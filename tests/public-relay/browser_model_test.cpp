#include "../../soh/soh/Network/Anchor/ServerBrowserModel.h"
#include <cassert>
#include <iostream>
int main() {
  BrowserRoom room;
  room.roomId = "Forest Watch";
  room.players = 3;
  room.teams = {"Kokiri"};
  room.storyStage = 3;
  room.storyTitle = "Awaken the Forest Sage";
  room.questTitle = "The Heart Beneath the Bark";
  BrowserFilters f;
  assert(BrowserMatches(room, f, "FOREST"));
  assert(BrowserMatches(room, f, "kokiri"));
  assert(BrowserMatches(room, f, "bark"));
  assert(!BrowserMatches(room, f, "Zora"));
  f.stage = 5;
  assert(!BrowserMatches(room, f, ""));
  f.stage = 3;
  assert(BrowserMatches(room, f, ""));
  f.rules = 2;
  assert(!BrowserMatches(room, f, ""));
  room.pvpMode = 1;
  assert(BrowserMatches(room, f, ""));
  room.players = 8;
  assert(!BrowserMatches(room, f, ""));
  f.availability = 2;
  assert(BrowserMatches(room, f, ""));
  f.availability = 0;
  assert(BrowserMatches(room, f, ""));
  BrowserRoom other = room;
  other.roomId = "River Watch";
  other.players = 1;
  assert(BrowserOrder(other, room));
  other.players = 8;
  assert(BrowserOrder(room, other));
  room.players = 4;
  other.players = 2;
  assert(BrowserOrder(room, other));
  room.storyStage = -1;
  assert(!BrowserMatches(room, f, ""));
  f.stage = -1;
  assert(BrowserMatches(room, f, ""));
  std::cout << "Browser search, combined filters, legacy status and ordering "
               "passed\n";
}
