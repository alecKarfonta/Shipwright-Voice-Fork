#pragma once
#include <string_view>

namespace CharacterTravel {
inline bool IsLegacySavePacket(std::string_view type) {
    return type == "UPDATE_TEAM_STATE" || type == "REQUEST_TEAM_STATE" || type == "GIVE_ITEM" || type == "SET_FLAG" ||
           type == "UNSET_FLAG" || type == "UPDATE_BEANS_COUNT" || type == "UPDATE_DUNGEON_ITEMS" ||
           type == "SET_CHECK_STATUS" || type == "ENTRANCE_DISCOVERED" || type == "CONSUME_ADULT_TRADE_ITEM";
}
} // namespace CharacterTravel
