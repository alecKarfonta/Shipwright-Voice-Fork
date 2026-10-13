#pragma once
#include "QuestRules.h"
#include <nlohmann/json.hpp>

namespace ARPG::Quests::PartyRules {
using Json=nlohmann::json;
inline bool NodeObjective(const Objective& o) { return Physical(o)||o.kind==Kind::Pack; }
inline Json Encode(const ARPGQuestProgress& p) {
    Json sites=Json::array();
    for(const auto& row:p.sites) {
        Json nodes=Json::array();
        for(const auto& s:row) {
            if(!s.placed)nodes.push_back(nullptr);
            else nodes.push_back({{"x",s.x},{"y",s.y},{"z",s.z},{"room",s.room},{"adult",s.adult}});
        }
        sites.push_back(nodes);
    }
    return {{"size",PartySize(p)},{"progress",p.progress},{"nodes",p.nodes},{"sites",sites}};
}
inline bool Site(const Json& j,ARPGQuestSite& s) {
    if(!j.is_object())return false;
    for(const char* k:{"x","y","z"})
        if(!j.contains(k)||!j[k].is_number()||!std::isfinite(j[k].get<double>())||std::abs(j[k].get<double>())>32760)return false;
    if(!j.contains("room")||!j["room"].is_number_integer()||j["room"]<0||j["room"]>126||
       !j.contains("adult")||!j["adult"].is_number_integer()||j["adult"]<0||j["adult"]>1)return false;
    s={j["x"].get<float>(),j["y"].get<float>(),j["z"].get<float>(),j["room"].get<int8_t>(),1,j["adult"].get<uint8_t>()};
    return true;
}
// A party snapshot can advance accepted objectives, never accept quests, change
// prerequisites, claim rewards, or erase personal progress. Server revisions are
// checked by the adapter before applying the immutable shared site coordinates.
inline bool Merge(ARPGCampaignData& campaign,int id,const Json& j) {
    const auto* d=Find(id);if(!d||!j.is_object()||Status(campaign,id)==State::Unaccepted)return false;
    if(!j.contains("progress")||!j["progress"].is_array()||j["progress"].size()!=ARPG_QUEST_OBJECTIVES||
       !j.contains("nodes")||!j["nodes"].is_array()||j["nodes"].size()!=ARPG_QUEST_OBJECTIVES||
       !j.contains("sites")||!j["sites"].is_array()||j["sites"].size()!=ARPG_QUEST_OBJECTIVES)return false;
    auto candidate=campaign.quests[id-1];
    if(!j.contains("size")||!j["size"].is_number_integer()||j["size"]<1||j["size"]>8)return false;
    candidate.partySize=std::max<int>(candidate.partySize,j["size"].get<int>());
    for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
        if(!j["progress"][i].is_number_integer()||j["progress"][i]<0||j["progress"][i]>Target(candidate,d->objectives[i])||
           !j["nodes"][i].is_number_integer()||j["nodes"][i]<0||j["nodes"][i]>7||
           !j["sites"][i].is_array()||j["sites"][i].size()!=ARPG_QUEST_NODES)return false;
    }
    auto& p=campaign.quests[id-1];p.partySize=candidate.partySize;
    for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
        const auto& o=d->objectives[i];if(!PriorComplete(p,*d,i))continue;
        if(NodeObjective(o)) {
            p.nodes[i]|=j["nodes"][i].get<uint8_t>()&((1<<o.count)-1);
            p.progress[i]=0;for(int n=0;n<o.count;++n)p.progress[i]+=(p.nodes[i]>>n)&1;
            for(int n=0;n<o.count;++n)Site(j["sites"][i][n],p.sites[i][n]);
        } else {
            int incoming=j["progress"][i].get<int>();
            // A watch is one continuous performance, never the sum of partial watches.
            if(o.kind!=Kind::Vigil||incoming==o.count)p.progress[i]=std::max<int>(p.progress[i],incoming);
        }
    }
    Refresh(campaign,id);return true;
}
inline void MoveToParty(ARPGCampaignData& campaign, uint32_t nextScope) {
    for (auto& quest : campaign.quests)
        if (quest.party && quest.party != nextScope) quest.party = 0;
}
inline bool CanClaimInParty(const ARPGQuestProgress& quest, uint32_t currentScope,
                            bool connected, bool online, bool outstanding) {
    return !quest.party || quest.party != currentScope || !connected || (online && !outstanding);
}
inline const char* JoinBlock(const ARPGCampaignData& c,int id,bool adult) {
    auto* d=Find(id);if(!d)return "Unknown quest.";
    if(Status(c,id)==State::Rewarded)return "You already completed this quest.";
    if(Status(c,id)!=State::Unaccepted)return nullptr;
    if(!AgeMatches(d->age,adult))return d->age==Age::Adult?"Return as adult Link to join.":"Return as child Link to join.";
    if(d->previous&&!Rewarded(c,d->previous))return "Complete the preceding quest first.";
    if((c.milestones&d->gate)!=d->gate)return "Continue the main story to unlock this quest.";
    return nullptr;
}
}
