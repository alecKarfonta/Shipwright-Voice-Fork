#include "../../soh/soh/Network/Anchor/CharacterPolicy.h"
#include "ARPGSaveCodec.h"
#include "QuestPartyRules.h"
#include <cassert>
#include <iostream>
using namespace ARPG;
using namespace ARPG::Quests;
int main() {
  int checks = 0;
  for (const auto &d : Catalog) {
    ARPGSaveData save{};
    save.seed = 987;
    save.rngState = 412;
    save.xp = Rules::XpThreshold(12);
    save.level = 13;
    SkillRules::Initialize(save);
    save.materials[1] = 17;
    save.materials[19] = 3;
    save.backpack[2].baseEquip = 1;
    save.backpack[2].slot = 0;
    save.backpack[2].itemLevel = 12;
    save.campaign.milestones = 0x7fff;
    auto &q = save.campaign.quests[d.id - 1];
    q.state = uint8_t(State::Active);
    q.party = 111;
    q.partySize = 3;
    for (int i = 0; i < 3; ++i) {
      q.progress[i] = Target(q, d.objectives[i]);
      if (PartyRules::NodeObjective(d.objectives[i]))
        for (int n = 0; n < d.objectives[i].count; ++n) {
          q.nodes[i] |= 1 << n;
          q.sites[i][n] = {float(n * 80), 0, 45, 0, 1, 0};
        }
    }
    Refresh(save.campaign, d.id);
    assert(Status(save.campaign, d.id) == State::Ready);
    assert(PartyRules::CanClaimInParty(q, 222, true, false, false));
    assert(!PartyRules::CanClaimInParty(q, 111, true, false, false));
    assert(!PartyRules::CanClaimInParty(q, 111, true, true, true));
    assert(PartyRules::CanClaimInParty(q, 111, true, true, false));
    assert(PartyRules::CanClaimInParty(q, 111, false, false, true));
    checks += 6;
    auto before = SaveCodec::Encode(save);
    PartyRules::MoveToParty(save.campaign, 111);
    assert(SaveCodec::Encode(save) == before);
    ++checks;
    PartyRules::MoveToParty(save.campaign, 222);
    assert(!q.party && q.partySize == 3);
    before["campaign"]["quests"][d.id - 1]["party"] = 0;
    assert(SaveCodec::Encode(save) == before);
    checks += 2;
    // An actual disk-format round trip retains objectives, physical sites,
    // bounty difficulty, skills, loot, and reward eligibility.
    save = SaveCodec::Decode(SaveCodec::Encode(save), 1);
    assert(Status(save.campaign, d.id) == State::Ready);
    assert(save.xp == Rules::XpThreshold(12) && save.materials[1] == 17 &&
           save.backpack[2].itemLevel == 12);
    assert(Claim(save, d.id, 0, false));
    auto earned = SaveCodec::Encode(save);
    PartyRules::MoveToParty(save.campaign, 333);
    save = SaveCodec::Decode(SaveCodec::Encode(save), 1);
    assert(!Claim(save, d.id, 0, false));
    assert(Status(save.campaign, d.id) == State::Rewarded);
    assert(PartyRules::JoinBlock(save.campaign, d.id, false));
    assert(!Accept(save.campaign, d.id, false));
    checks += 7;
  }
  // Portable clients reject save-import traffic but retain the live co-op
  // protocol.
  for (auto type : {"UPDATE_TEAM_STATE", "REQUEST_TEAM_STATE", "GIVE_ITEM",
                    "SET_FLAG", "UNSET_FLAG", "UPDATE_BEANS_COUNT",
                    "UPDATE_DUNGEON_ITEMS", "SET_CHECK_STATUS",
                    "ENTRANCE_DISCOVERED", "CONSUME_ADULT_TRADE_ITEM"}) {
    assert(CharacterTravel::IsLegacySavePacket(type));
    ++checks;
  }
  for (auto type :
       {"ARPG_QUEST", "ARPG_ENEMY_KILL", "PLAYER_UPDATE", "ALL_CLIENT_STATE",
        "UPDATE_ROOM_STATE", "UPDATE_CLIENT_STATE", "HANDSHAKE",
        "GAME_COMPLETE", "TELEPORT_TO"}) {
    assert(!CharacterTravel::IsLegacySavePacket(type));
    ++checks;
  }
  std::cout << checks << " character travel checks passed across "
            << ARPG_QUEST_COUNT << " quests\n";
}
