#include "QuestRules.h"
#include "QuestDialogue.h"
#include "ARPGSaveCodec.h"
#include <iostream>
#include <stdexcept>
#include <set>
using namespace ARPG;
using namespace ARPG::Quests;
int checks=0;
void Check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);++checks;}
void Finish(ARPGCampaignData& c,int id) {
    const auto& d=*Find(id);
    for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
        const auto& o=d.objectives[i];if(!o.count)continue;
        switch(o.kind) {
            case Kind::Hunt:case Kind::Recover:for(int n=0;n<o.count;++n)RecordKill(c,o.scene,o.foe);break;
            case Kind::Gather:case Kind::Work:case Kind::Survey:case Kind::Pack:for(int n=0;n<o.count;++n)Check(UseNode(c,id,i,n),"Distinct world node advances");break;
            case Kind::Visit:VisitScene(c,o.scene);break;
            case Kind::Milestone:ObserveStory(c,o.milestone);break;
            case Kind::Song:RecordSong(c,o.scene,o.detail);break;
            case Kind::Delivery:Check(Deliver(c,id,i,o.scene,Contact(o.detail)),"Correct recipient records delivery");break;
            case Kind::Vigil:{WatchClock clock{};for(int n=0;n<o.count*20;++n)AdvanceWatches(c,clock,o.scene,o.detail==uint8_t(Period::Night),true);break;}
            default:break;
        }
    }
}
int main() {
    try {
        ARPGSaveData save{};save.level=1;SkillRules::Initialize(save);
        auto& c=save.campaign;
        Check(!Accept(c,0,false) && !Accept(c,ARPG_QUEST_COUNT+1,false),"Reject invalid IDs");
        Check(Available(c,1,false) && !Available(c,1,true),"Child offer has an age gate");
        Check(!Available(c,2,false),"Quest chain prerequisite enforced");
        RecordKill(c,0x55,Foe::Plant);Check(!c.quests[0].progress[0],"Unaccepted quests never accumulate kills");
        Check(Accept(c,1,false),"Accept the opening quest");
        RecordKill(c,0x51,Foe::Plant);RecordKill(c,0x55,Foe::Wolf);
        Check(!c.quests[0].progress[0],"Wrong scene and wrong target cannot count");
        for(int i=0;i<100;++i)RecordKill(c,0x55,Foe::Plant);
        Check(c.quests[0].progress[0]==4 && Status(c,1)==State::Ready,"Counters saturate and transition to ready");
        Check(!Available(c,2,false),"Ready is not rewarded for chain gating");
        for(auto& item:save.backpack)item=RewardItem(Catalog[0],0,false);
        auto before=SaveCodec::Encode(save);
        Check(!Claim(save,1,0,false) && before==SaveCodec::Encode(save),"Full backpack claim is an atomic no-op");
        save.backpack[0]={};Check(Claim(save,1,1,false),"Choose alternate reward after freeing space");
        auto claimed=SaveCodec::Encode(save);
        Check(!Claim(save,1,0,false) && claimed==SaveCodec::Encode(save),"Rewards cannot be claimed twice");
        Check(Available(c,2,false) && Accept(c,2,false),"Reward unlocks next NPC offer");
        Check(UseNode(c,2,0,0) && !UseNode(c,2,0,0),"Gather nodes are not repeatable");
        Check(!UseNode(c,2,-1,0) && !UseNode(c,2,0,3),"Malformed node access rejected");
        c.quests[1].sites[0][0]={120,15,-220,0,1,0};
        auto loaded=SaveCodec::Decode(SaveCodec::Encode(save),123);
        Check(loaded.campaign.quests[1].progress[0]==1 && loaded.campaign.quests[1].nodes[0]==1 &&
            loaded.campaign.quests[1].sites[0][0].x==120,"Partial progress and world coordinates survive reload");
        Check(!UseNode(loaded.campaign,2,0,0),"Reload cannot duplicate a gathered item");
        Check(UseNode(c,2,0,1) && UseNode(c,2,0,2),"All distinct gather nodes complete");
        save.backpack[0]={};Check(Claim(save,2,0,false),"Gather reward claim");
        ObserveStory(c,Emerald);Check(Accept(c,3,false) && Status(c,3)==State::Ready,"Previously defeated story bosses count on acceptance");
        Check(Available(c,21,true),"Adult story arc becomes available independently of childhood side quests");
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<'\n';return 1;
    }
    try {
        ARPGSaveData save{};save.level=1;SkillRules::Initialize(save);auto& c=save.campaign;
        Check(!Available(c,21,true),"Adult forest arc requires the native Emerald");
        ObserveStory(c,Emerald);Check(Available(c,21,true),"Adult arcs do not force missed childhood side quests");
        Check(Accept(c,5,true),"Accept a named pack quest");
        RecordKill(c,0x51,Foe::Wolf);Check(!c.quests[4].progress[0],"Unrelated regional monsters never satisfy a named pack hunt");
        Check(UseNode(c,5,0,1) && !UseNode(c,5,0,1),"A named pack member grants credit only once");
        auto packSave=SaveCodec::Decode(SaveCodec::Encode(save),42);
        Check(packSave.campaign.quests[4].nodes[0]==2 && packSave.campaign.quests[4].progress[0]==1,"Interrupted pack hunts preserve defeated member identities");
        c.quests[5].state=uint8_t(State::Rewarded);Check(Accept(c,7,true),"Accept multi-step construction");
        Check(!UseNode(c,7,1,0),"Cannot build before collecting supplies");
        for(int n=0;n<3;++n)UseNode(c,7,0,n);
        Check(UseNode(c,7,1,0),"Gathered supplies unlock building");
        auto mid=SaveCodec::Decode(SaveCodec::Encode(save),42);
        Check(mid.campaign.quests[6].nodes[1]==1,"Restored worksite persists before turn-in");
        auto malformed=SaveCodec::Encode(save);
        auto& q=malformed["campaign"]["quests"][6];q["progress"]={999,-20,"oops"};q["nodes"]={999,999,999};
        q["sites"][1][0]["x"]="bad";q["sites"][1][1]["x"]=1e100;
        malformed["campaign"]["tracked"]=999;
        auto repaired=SaveCodec::Decode(malformed,1);
        Check(repaired.campaign.quests[6].progress[0]==3 && repaired.campaign.quests[6].progress[1]==3,"Physical counters derive from bounded node masks");
        Check(!repaired.campaign.quests[6].sites[1][0].placed && !repaired.campaign.quests[6].sites[1][1].placed,"Invalid coordinates are discarded");
        Check(repaired.campaign.tracked==0,"Bad tracking IDs discarded");
        auto old=SaveCodec::Encode(save);old.erase("campaign");auto migrated=SaveCodec::Decode(old,10);
        Check(migrated.campaign.tracked==0 && migrated.campaign.milestones==0 && !migrated.campaign.quests[6].state,"Old saves start a fresh optional campaign");
        Check(migrated.seed==save.seed && migrated.level==save.level,"Old-save character progress preserved");
        std::set<std::string> titles;std::set<int> chapters;int work=0,collection=0,story=0,claims=0;
        save=SaveCodec::Decode(nlohmann::json::object(),42);
        for(const auto& d:Catalog) {
            Check(d.id==int(&d-Catalog)+1,"Stable contiguous catalog identity");
            Check(titles.insert(d.title).second && d.offer[0] && d.thanks[0],"Distinct titles with authored offer and completion dialogue");
            Check(!d.previous || d.previous<d.id,"Campaign graph is acyclic");chapters.insert(d.chapter);
            for(const auto& o:d.objectives) {
                if(o.kind==Kind::Work)++work;if(o.kind==Kind::Gather || o.kind==Kind::Recover)++collection;if(o.kind==Kind::Milestone)++story;
                if(Physical(o))Check(o.count>0 && o.count<=ARPG_QUEST_NODES && o.scene>=0,"World objectives fit persistent node capacity");
            }
            ObserveStory(save.campaign,d.gate);
            Check(Accept(save.campaign,d.id,d.age==Age::Adult),"Each catalog quest is reachable in campaign order");
            Finish(save.campaign,d.id);Check(Status(save.campaign,d.id)==State::Ready,"Every authored objective can reach ready");
            for(int choice=0;choice<2;++choice)for(bool adult:{false,true})Check(InventoryRules::Valid(RewardItem(d,choice,adult)),"Every age/reward choice creates valid gear");
            if(InventoryRules::Count(save)==ARPG_BACKPACK_CAPACITY)for(auto& item:save.backpack)item={};
            Check(Claim(save,d.id,d.id%2,d.age==Age::Adult),"Every authored reward is claimable");++claims;
            Check(!Claim(save,d.id,0,true),"Every reward remains single-use");
            auto roundTrip=SaveCodec::Decode(SaveCodec::Encode(save),0);
            Check(SaveCodec::Encode(save)==SaveCodec::Encode(roundTrip),"Campaign prefixes survive complete save round trips");
        }
        Check(chapters.size()==11 && work>=10 && collection>=10 && story>=8,"Campaign has substantial coverage across eleven arcs");
        Check(claims==ARPG_QUEST_COUNT,"Exactly one reward claim per completed quest");
        // All new branches are authored, linked, and specific; no online dialogue service is needed.
        std::set<std::string> backgrounds,advice,ready;
        for(const auto& d:Catalog) {
            const auto& line=Dialogue::For(d.id);
            Check(backgrounds.insert(line.why).second && advice.insert(line.hint).second && ready.insert(line.ready).second,
                "Every quest has distinct background, advice and ready dialogue");
            Check(std::string(line.why).size()>30 && std::string(line.hint).size()>30 && std::string(line.ready).size()>30,
                "Conversation topics contain substantive authored responses");
            for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)if(d.objectives[i].kind==Kind::Delivery)
                Check(std::string(Dialogue::DeliveryReply(d.id,i))!="I have your message. Thank you for bringing it yourself.","Every delivery has a specific recipient response");
        }
        Check(std::string(Dialogue::Greeting(Dialogue::Voice::Mido,true,0)).find("Saria")!=std::string::npos &&
              std::string(Dialogue::Greeting(Dialogue::Voice::Mido,true,0))!=Dialogue::Greeting(Dialogue::Voice::Mido,false,0),"Mido's voice respects Link's age");
        Check(std::string(Dialogue::Greeting(Dialogue::Voice::Goron,true,0))!=Dialogue::Greeting(Dialogue::Voice::Goron,true,Fire),"The Goron greeting responds to the Fire Temple's liberation");
        ARPGSaveData expansion{};auto& e=expansion.campaign;
        e.quests[40].state=uint8_t(State::Rewarded);Check(Accept(e,42,false),"Accept leaf delivery after its local chain");
        Check(!Deliver(e,42,1,0x62,Contact::Goron),"Recipient cannot accept supplies before they are gathered");
        for(int n=0;n<3;++n)UseNode(e,42,0,n);
        VisitScene(e,0x62);Check(!e.quests[41].progress[1],"Entering the destination is not a conversation");
        Check(!Deliver(e,42,1,0x55,Contact::Goron) && !Deliver(e,42,1,0x62,Contact::Kokiri),"Wrong scene or recipient cannot receive a parcel");
        Check(Deliver(e,42,1,0x62,Contact::Goron) && !Deliver(e,42,1,0x62,Contact::Goron),"Delivery records once and cannot be repeated");
        Check(!Deliver(e,42,-1,0x62,Contact::Goron) && !Deliver(e,42,3,0x62,Contact::Goron),"Malformed delivery objectives rejected");
        Check(Accept(e,50,false),"Scientist's introductory route is independently available");
        Check(!Deliver(e,50,1,0x58,Contact::Zora),"Multi-recipient errands enforce their order");
        Check(Deliver(e,50,0,0x63,Contact::Ranch) && Deliver(e,50,1,0x58,Contact::Zora),"Ordered two-recipient route completes");
        e.quests[35].state=uint8_t(State::Rewarded);ObserveStory(e,Spirit);Check(Accept(e,55,true),"Accept desert water errand");
        Check(!UseNode(e,55,1,0),"Cannot gather the scientist's plants before requesting his advice");
        Check(Deliver(e,55,0,0x38,Contact::Scientist) && UseNode(e,55,1,0),"Advice unlocks the specified gathering sites");
        e.quests[4].state=uint8_t(State::Rewarded);Check(Accept(e,43,false),"Accept Malon's musical watch");
        WatchClock clock{};for(int i=0;i<600;++i)AdvanceWatches(e,clock,0x63,true,true);
        Check(!e.quests[42].progress[1],"Watch cannot start before the lullaby");
        RecordSong(e,0x51,int(Tune::Epona));RecordSong(e,0x63,int(Tune::Saria));
        Check(!e.quests[42].progress[0],"Wrong place and wrong melody do not count");
        RecordSong(e,0x63,int(Tune::Epona));Check(e.quests[42].progress[0]==1,"Correct learned song event records performance");
        for(int i=0;i<80;++i)AdvanceWatches(e,clock,0x63,false,true);
        Check(!e.quests[42].progress[1],"Night watch cannot accumulate in daylight");
        for(int i=0;i<59;++i)AdvanceWatches(e,clock,0x63,true,true);
        Check(e.quests[42].progress[1]==2,"Only full gameplay seconds advance a watch");
        AdvanceWatches(e,clock,0x63,true,false);Check(!e.quests[42].progress[1],"One movement or damage frame resets the continuous watch");
        for(int i=0;i<100;++i)AdvanceWatches(e,clock,0x63,true,true);
        auto partial=SaveCodec::Decode(SaveCodec::Encode(expansion),0);
        Check(partial.campaign.quests[42].progress[0]==1 && !partial.campaign.quests[42].progress[1],"Reload retains song but restarts unfinished continuous watch");
        AdvanceWatches(e,clock,0x51,true,true);Check(!e.quests[42].progress[1],"Leaving the target region resets a watch");
        for(int i=0;i<400;++i)AdvanceWatches(e,clock,0x63,true,true);
        Check(Status(e,43)==State::Ready && e.quests[42].progress[1]==20,"Full uninterrupted night watch finishes");
        ResetWatches(e);Check(e.quests[42].progress[1]==20,"Finished watches survive transitions");
        const auto& day=Find(49)->objectives[0];const auto& night=Find(49)->objectives[1];
        Check(PeriodMatches(day,false) && !PeriodMatches(day,true) && PeriodMatches(night,true) && !PeriodMatches(night,false),"Day and night observations have distinct conditions");
        auto oldCampaign=SaveCodec::Encode(expansion);oldCampaign["campaign"]["quests"].erase(oldCampaign["campaign"]["quests"].begin()+40,oldCampaign["campaign"]["quests"].end());
        auto fromForty=SaveCodec::Decode(oldCampaign,17);
        Check(fromForty.campaign.quests[4].state==uint8_t(State::Rewarded) && !fromForty.campaign.quests[40].state && !fromForty.campaign.quests[57].state,"Original forty-quest save migrates without reinterpreting IDs");
        std::cout<<checks<<" quest checks passed: all 58 quests, 11 arcs, objective gates, world nodes, migration, rewards and save recovery.\n";
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
