#pragma once
#include "QuestCatalog.h"
#include "InventoryRules.h"

namespace ARPG::Quests {
inline State Status(const ARPGCampaignData& s,int id) {
    return Find(id)?static_cast<State>(s.quests[id-1].state):State::Unaccepted;
}
inline bool Rewarded(const ARPGCampaignData& s,int id) { return Status(s,id)==State::Rewarded; }
inline bool AgeMatches(Age age,bool adult) { return age==Age::Either || (age==Age::Adult)==adult; }
inline bool Available(const ARPGCampaignData& s,int id,bool adult) {
    const auto* d=Find(id);
    return d && Status(s,id)==State::Unaccepted && AgeMatches(d->age,adult) &&
        (!d->previous || Rewarded(s,d->previous)) && (s.milestones&d->gate)==d->gate;
}
inline int PartySize(const ARPGQuestProgress& p) {return std::clamp<int>(p.partySize,1,8);}
inline int Target(const ARPGQuestProgress& p,const Objective& o) {
    return o.count+((o.kind==Kind::Hunt||o.kind==Kind::Recover)?(PartySize(p)-1)*((o.count+1)/2):0);
}
inline bool Bounty(const Definition& d) {
    for(const auto& o:d.objectives)if(o.kind==Kind::Hunt||o.kind==Kind::Recover||o.kind==Kind::Pack)return true;
    return false;
}
inline uint32_t RewardXp(const ARPGQuestProgress& p,const Definition& d) {return d.xp+(Bounty(d)?d.xp*(PartySize(p)-1)/4:0);}
inline uint32_t RewardRupees(const ARPGQuestProgress& p,const Definition& d) {return d.rupees+(Bounty(d)?d.rupees*(PartySize(p)-1)/4:0);}
inline bool Complete(const ARPGQuestProgress& p,const Definition& d) {
    for(size_t i=0;i<d.objectives.size();++i)if(p.progress[i]<Target(p,d.objectives[i]))return false;
    return true;
}
inline void Refresh(ARPGCampaignData& s,int id) {
    const auto* d=Find(id);if(!d)return;
    auto& p=s.quests[id-1];
    if(p.state!=uint8_t(State::Active) && p.state!=uint8_t(State::Ready))return;
    for(size_t i=0;i<d->objectives.size();++i) {
        const auto& o=d->objectives[i];
        if(o.kind==Kind::Milestone && (s.milestones&o.milestone)==o.milestone)p.progress[i]=o.count;
    }
    p.state=uint8_t(Complete(p,*d)?State::Ready:State::Active);
}
inline bool Accept(ARPGCampaignData& s,int id,bool adult) {
    if(!Available(s,id,adult))return false;
    s.quests[id-1]={};s.quests[id-1].state=uint8_t(State::Active);s.tracked=id;Refresh(s,id);return true;
}
inline bool PriorComplete(const ARPGQuestProgress& p,const Definition& d,int objective) {
    // Conversation, performance and observation steps are ordered. Original hunt/recovery objectives can overlap.
    if(!Ordered(d.objectives[objective]))return true;
    for(int i=0;i<objective;++i)if(p.progress[i]<Target(p,d.objectives[i]))return false;
    return true;
}
inline bool Add(ARPGCampaignData& s,int id,int objective,int amount=1) {
    const auto* d=Find(id);if(!d || objective<0 || objective>=ARPG_QUEST_OBJECTIVES || amount<=0)return false;
    auto& p=s.quests[id-1];const auto& o=d->objectives[objective];
    if(Status(s,id)!=State::Active || !PriorComplete(p,*d,objective) || p.progress[objective]>=Target(p,o))return false;
    p.progress[objective]=std::min<int>(Target(p,o),p.progress[objective]+std::min(amount,255));Refresh(s,id);return true;
}
inline bool UseNode(ARPGCampaignData& s,int id,int objective,int node) {
    const auto* d=Find(id);if(!d || objective<0 || objective>=ARPG_QUEST_OBJECTIVES || node<0 || node>=ARPG_QUEST_NODES)return false;
    auto& p=s.quests[id-1];const auto& o=d->objectives[objective];
    if((!Physical(o) && o.kind!=Kind::Pack) || node>=o.count || (p.nodes[objective]&(1<<node)))return false;
    if(!Add(s,id,objective))return false;
    p.nodes[objective]|=1<<node;return true;
}
inline void RecordKill(ARPGCampaignData& s,int scene,Foe foe) {
    for(const auto& d:Catalog)for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
        const auto& o=d.objectives[i];
        if((o.kind==Kind::Hunt || o.kind==Kind::Recover) && o.scene==scene && (o.foe==Foe::Any || o.foe==foe))Add(s,d.id,i);
    }
}
inline void VisitScene(ARPGCampaignData& s,int scene) {
    for(const auto& d:Catalog)for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)if(d.objectives[i].kind==Kind::Visit && d.objectives[i].scene==scene)Add(s,d.id,i);
}
inline void RecordSong(ARPGCampaignData& s,int scene,int tune) {
    for(const auto& d:Catalog)for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
        const auto& o=d.objectives[i];if(o.kind==Kind::Song && o.scene==scene && o.detail==tune)Add(s,d.id,i);
    }
}
inline bool Deliver(ARPGCampaignData& s,int id,int objective,int scene,Contact recipient) {
    const auto* d=Find(id);if(!d || objective<0 || objective>=ARPG_QUEST_OBJECTIVES)return false;
    const auto& o=d->objectives[objective];
    return o.kind==Kind::Delivery && o.scene==scene && o.detail==uint8_t(recipient) && Add(s,id,objective);
}
// Partial watches must be continuous. Completed watches persist like other objectives.
inline void ResetWatches(ARPGCampaignData& s) {
    for(const auto& d:Catalog)for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)
        if(d.objectives[i].kind==Kind::Vigil && s.quests[d.id-1].progress[i]<d.objectives[i].count)s.quests[d.id-1].progress[i]=0;
}
using WatchClock=std::array<uint8_t,ARPG_QUEST_COUNT*ARPG_QUEST_OBJECTIVES>;
inline void AdvanceWatches(ARPGCampaignData& s,WatchClock& clock,int scene,bool night,bool quiet) {
    for(const auto& d:Catalog)for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
        const auto& o=d.objectives[i];auto& p=s.quests[d.id-1];auto& ticks=clock[(d.id-1)*ARPG_QUEST_OBJECTIVES+i];
        if(o.kind!=Kind::Vigil || Status(s,d.id)!=State::Active || p.progress[i]>=o.count) {ticks=0;continue;}
        if(o.scene!=scene || !quiet || !PeriodMatches(o,night) || !PriorComplete(p,d,i)) {p.progress[i]=0;ticks=0;}
        else if(++ticks>=20) {ticks=0;Add(s,d.id,i);}
    }
}
inline void ObserveStory(ARPGCampaignData& s,uint32_t flags) {
    s.milestones|=flags;
    for(const auto& d:Catalog)Refresh(s,d.id);
}
inline ARPGItemInstance RewardItem(const Definition& d,int choice,bool adult) {
    ARPGItemInstance item{};item.slot=choice==0?d.rewardSlot:(d.rewardSlot+1)%4;
    item.baseEquip=(adult && item.slot<2)?2:1;
    item.rarity=ARPG_RARITY_RARE;item.itemLevel=d.level;item.modCount=2;
    item.mods[0]={uint8_t(item.slot==0?ARPG_MOD_BRUTAL:item.slot==1?ARPG_MOD_AEGIS:item.slot==2?ARPG_MOD_MAGICAFF:ARPG_MOD_SWIFT),
        int16_t(item.slot==1?std::min(4,1+d.level/14):std::min(25,5+d.level/3))};
    item.mods[1]={ARPG_MOD_LUCKY,int16_t(3+d.level/5)};
    // Regional rebuilding finales and the final preparations give recognizable build-defining gear.
    if(d.id==23)Loot::IdentifySet(item,1);
    if(d.id==26)Loot::IdentifySet(item,2);
    if(d.id==29)Loot::IdentifySet(item,3);
    if(d.id==36)Loot::IdentifyUnique(item,item.slot==0?2:4);
    if(d.id==39)Loot::IdentifyUnique(item,item.slot==0?1:3);
    if(d.id==58)Loot::IdentifySet(item,3);
    if(d.id==40)Loot::IdentifyUnique(item,item.slot==2?6:8);
    return item;
}
// Full backpacks fail before any XP, currency, RNG, or state is touched. The native adapter applies XP and rupees
// synchronously after this succeeds; saves snapshot the complete transaction on the game thread.
inline bool Claim(ARPGSaveData& save,int id,int choice,bool adult) {
    const auto* d=Find(id);if(!d || choice<0 || choice>1 || Status(save.campaign,id)!=State::Ready)return false;
    if(!InventoryRules::Collect(save,RewardItem(*d,choice,adult)))return false;
    save.campaign.quests[id-1].state=uint8_t(State::Rewarded);
    if(save.campaign.tracked==id)save.campaign.tracked=0;
    return true;
}
inline void Sanitize(ARPGCampaignData& s) {
    s.milestones&=(1<<11)-1;
    ResetWatches(s);
    for(const auto& d:Catalog) {
        auto& p=s.quests[d.id-1];
        if(p.state>uint8_t(State::Rewarded) || p.state==0) {p={};continue;}
        for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
            const auto& o=d.objectives[i];p.progress[i]=std::min<int>(p.progress[i],Target(p,o));
            if(!Physical(o) && o.kind!=Kind::Pack) {p.nodes[i]=0;for(auto& site:p.sites[i])site={};continue;}
            p.nodes[i]&=(1<<o.count)-1;
            p.progress[i]=0;for(int n=0;n<o.count;++n)p.progress[i]+=(p.nodes[i]>>n)&1;
            for(auto& site:p.sites[i])if(!site.placed || site.placed!=1 || site.adult>1 || site.room<0 ||
                !std::isfinite(site.x) || !std::isfinite(site.y) || !std::isfinite(site.z) ||
                std::abs(site.x)>32760 || std::abs(site.y)>32760 || std::abs(site.z)>32760)site={};
        }
        Refresh(s,d.id);
    }
    if(!Find(s.tracked) || Status(s,s.tracked)==State::Unaccepted || Status(s,s.tracked)==State::Rewarded)s.tracked=0;
}
}
