#include "QuestParty.h"
#include "QuestPartyRules.h"
#include "QuestSystem.h"
#include "ARPGSave.h"
#include "ARPGTheme.h"
#include "soh/Network/Anchor/Anchor.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Notification/Notification.h"
#include <imgui.h>
#include <map>
#include <set>
extern "C" {
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
}
namespace ARPG::Quests::Party {
namespace {
using Json=nlohmann::json;
std::map<int,Json> quests;
std::set<int> dismissed,pending,announceJoin;
std::map<int,uint64_t> outstanding;
bool ready=false,connected=false,showCompleted=false;
uint32_t scope=0,client=0;
uint64_t sequence=0;
int clock=0;
std::string message;
const char* Team() {return CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"),"default");}
uint32_t Seed() {return IS_RANDO?Rando::Context::GetInstance()->GetSeed():0;}
const char* Room() {return CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"),"");}
uint32_t Scope() {
    // Reconnect to the same server/room/team automatically. Joining another party
    // keeps personal progress and rewards without transferring old membership.
    std::string key=std::string(CVarGetString(CVAR_REMOTE_ANCHOR("Host"),""))+":"+
        std::to_string(CVarGetInteger(CVAR_REMOTE_ANCHOR("Port"),43383))+"/"+Room()+"/"+Team()+"/"+std::to_string(Seed());
    uint32_t hash=2166136261u;for(unsigned char c:key)hash=(hash^c)*16777619u;return hash?hash:1;
}
bool AvailableConnection() {
    auto* a=Anchor::Instance;return a&&a->isEnabled&&a->isConnected&&a->IsSaveLoaded()&&Enabled();
}
void Send(Json p) {
    if(!AvailableConnection())return;
    if(p.contains("seq"))outstanding[p["id"].get<int>()]=p["seq"].get<uint64_t>();
    p["type"]="ARPG_QUEST";p["version"]=1;p["quiet"]=true;
    Anchor::Instance->SendJsonToRemote(p);
}
bool Member(int id) {
    auto it=quests.find(id);return it!=quests.end()&&it->second.contains("members")&&
        it->second["members"].contains(std::to_string(client));
}
void Resume(int id,bool joining=false,bool automatic=false) {
    const auto& p=gARPGSave.campaign.quests[id-1];pending.insert(id);
    Send({{"op",joining?"join":"share"},{"id",id},{"state",PartyRules::Encode(p)},{"rewarded",p.state==uint8_t(State::Rewarded)},{"notify",!automatic}});
}
void Join(int id) {
    auto& c=gARPGSave.campaign;
    if(!ready||PartyRules::JoinBlock(c,id,LINK_IS_ADULT))return;
    if(Status(c,id)==State::Unaccepted&&!Accept(c,id,LINK_IS_ADULT))return;
    c.quests[id-1].party=scope;announceJoin.insert(id);Resume(id,quests.count(id)!=0);dismissed.erase(id);
    c.tracked=id;message="Joining the party quest. Your rewards remain personal.";
}
void Leave(int id) {
    Send({{"op","leave"},{"id",id}});gARPGSave.campaign.quests[id-1].party=0;
    pending.erase(id);outstanding.erase(id);dismissed.insert(id);message="Quest kept in your journal. You are now completing it solo.";
}
void Store(const Json& q,bool notify) {
    if(!q.is_object()||!q.contains("id")||!q["id"].is_number_integer()||
       !q.contains("revision")||!q["revision"].is_number_unsigned()||
       !q.contains("members")||!q["members"].is_object())return;
    int id=q["id"].get<int>();if(!Find(id)||q["members"].size()>128)return;
    auto previous=quests.find(id);
    if(previous!=quests.end()&&q["revision"].get<uint64_t>()<=previous->second["revision"].get<uint64_t>())return;
    bool fresh=previous==quests.end();quests[id]=q;
    auto& campaign=gARPGSave.campaign;
    if(campaign.quests[id-1].party==scope&&Member(id)) {
        bool joining=pending.erase(id)!=0;auto before=Status(campaign,id);
        PartyRules::Merge(campaign,id,q);
        if(joining) {
            // The player can move, gather or place sites while the initial
            // invitation is in flight. Reconcile that short local interval once
            // after joining; max/union snapshot merge cannot double-count it.
            auto local=PartyRules::Encode(campaign.quests[id-1]);bool catchup=false;
            for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
                if(q.contains("progress")&&q["progress"].is_array()&&q["progress"].size()==3&&local["progress"][i]>q["progress"][i])catchup=true;
                for(int n=0;n<ARPG_QUEST_NODES;++n)if(!local["sites"][i][n].is_null()&&q["sites"][i][n].is_null())catchup=true;
            }
            if(catchup)Send({{"op","share"},{"id",id},{"seq",++sequence},{"state",local},
                {"rewarded",Status(campaign,id)==State::Rewarded},{"notify",false}});
        }
        if(joining&&announceJoin.erase(id)) {message=std::string("Party joined: ")+Find(id)->title;Notification::Emit({.message="Party quest joined.\n"+std::string(Find(id)->title)});}
        else if(!joining&&before==State::Active&&Status(campaign,id)==State::Ready)Notification::Emit({.message="Party quest ready!\nReturn for your reward."});
    } else if(fresh&&notify&&!dismissed.count(id)&&Status(campaign,id)!=State::Rewarded) {
        Notification::Emit({.message="Party shared a quest.\nJ / Quests > Party"});
    }
}
const char* StateLabel(int id) {
    if(pending.count(id))return "Joining party...";
    if(Online(id))return Status(gARPGSave.campaign,id)==State::Rewarded?"Party / reward claimed":Status(gARPGSave.campaign,id)==State::Ready?"Party / ready to turn in":"Party quest";
    if(Shared(id))return "Party saved / playing solo";
    return "Personal quest";
}
}
void Share(int id) {Join(id);}
float PackHealthRatio(int id) {
    if(!Find(id))return 1.f;
    float desired=1.f+.5f*(PartySize(gARPGSave.campaign.quests[id-1])-1);
    auto* a=Anchor::Instance;
    float room=a&&a->isConnected&&a->roomState.difficultyMode?EnemyDifficulty::HpMult():1.f;
    return std::max(1.f,desired/std::max(1.f,room));
}
void Reset() {quests.clear();dismissed.clear();pending.clear();announceJoin.clear();outstanding.clear();ready=false;connected=false;scope=client=0;sequence=0;clock=0;message.clear();}
bool Shared(int id) {return Find(id)&&gARPGSave.campaign.quests[id-1].party!=0;}
bool Online(int id) {return AvailableConnection()&&ready&&Find(id)&&gARPGSave.campaign.quests[id-1].party==scope&&Member(id);}
bool CanClaim(int id) {return Find(id) && PartyRules::CanClaimInParty(gARPGSave.campaign.quests[id-1], Scope(), AvailableConnection(), Online(id), outstanding.count(id));}
bool PlaceSites(int id) {
    if(!Online(id))return true;
    auto* a=Anchor::Instance;
    for(const auto& entry:quests[id]["members"].items()) {
        uint32_t peer=0;try {peer=std::stoul(entry.key());}catch(...){continue;}
        auto it=a->clients.find(peer);
        if(peer<client&&it!=a->clients.end()&&it->second.online&&it->second.isSaveLoaded&&
           it->second.sceneNum==gPlayState->sceneNum&&it->second.curRoomNum==gPlayState->roomCtx.curRoom.num)return false;
    }
    return true;
}
void Tick() {
    bool live=AvailableConnection();uint32_t next=Scope();auto* a=Anchor::Instance;
    if(!live) {if(connected){ready=false;connected=false;quests.clear();pending.clear();}return;}
    if(!connected||scope!=next||client!=a->ownClientId) {
        Reset();connected=true;scope=next;client=a->ownClientId;
        PartyRules::MoveToParty(gARPGSave.campaign, scope);
    }
    if(!client)return;
    if(!ready&&clock++%100==0)Send({{"op","hello"}});
}
void Receive(const Json& packet) {
    if(!AvailableConnection()||!connected||packet.contains("clientId")||packet.value("version",0)!=1||
       packet.value("seed",uint32_t(0))!=Seed()||packet.value("room",std::string{})!=Room()||packet.value("team",std::string{})!=Team())return;
    // Only server-authored replies are accepted. Legacy relays forward a clientId
    // and never acknowledge this protocol, so sharing stays disabled there.
    if(packet.value("ackClient",uint32_t(0))==client&&packet.contains("ack")&&packet["ack"].is_number_unsigned()) {
        uint64_t ack=packet["ack"].get<uint64_t>();
        for(auto it=outstanding.begin();it!=outstanding.end();)if(it->second<=ack)it=outstanding.erase(it);else ++it;
    }
    std::string op=packet.value("op",std::string{});
    if(op=="hello") {
        if(!packet.contains("quests")||!packet["quests"].is_array()||packet["quests"].size()>ARPG_QUEST_COUNT)return;
        bool first=!ready;ready=true;
        for(const auto& q:packet["quests"])Store(q,false);
        if(first)for(const auto& d:Catalog)if(gARPGSave.campaign.quests[d.id-1].party==scope)Resume(d.id,false,true);
    } else if(op=="state"&&ready&&packet.contains("quest"))Store(packet["quest"],packet.value("invite",false));
}
void Publish(const ARPGCampaignData& before) {
    if(!ready||!AvailableConnection())return;
    for(const auto& d:Catalog) {
        // The share/join is already ahead of these events in Anchor's ordered
        // TCP queue. Publish while its acknowledgement is in flight as well,
        // so simultaneous kills are summed instead of reduced to a max snapshot.
        if(gARPGSave.campaign.quests[d.id-1].party!=scope||(!Member(d.id)&&!pending.count(d.id)))continue;
        const auto& old=before.quests[d.id-1];const auto& p=gARPGSave.campaign.quests[d.id-1];
        for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
            const auto& o=d.objectives[i];
            if(PartyRules::NodeObjective(o)) {
                for(int n=0;n<o.count;++n) {
                    const auto& point=p.sites[i][n];
                    if(point.placed&&!old.sites[i][n].placed)Send({{"op","site"},{"id",d.id},{"seq",++sequence},{"objective",i},{"node",n},
                        {"point",{{"x",point.x},{"y",point.y},{"z",point.z},{"room",point.room},{"adult",point.adult}}}});
                    if((p.nodes[i]&~old.nodes[i])&(1<<n))Send({{"op","advance"},{"id",d.id},{"seq",++sequence},{"objective",i},{"node",n}});
                }
            } else if(p.progress[i]>old.progress[i]&&(o.kind!=Kind::Vigil||p.progress[i]==o.count)) {
                int amount=o.kind==Kind::Vigil?o.count:p.progress[i]-old.progress[i];
                Send({{"op","advance"},{"id",d.id},{"seq",++sequence},{"objective",i},{"amount",amount}});
            }
        }
        if(p.state==uint8_t(State::Rewarded)&&old.state!=p.state)Send({{"op","reward"},{"id",d.id}});
    }
}
int Invitations() {
    int count=0;for(const auto& [id,q]:quests)if(!Member(id)&&!dismissed.count(id)&&Status(gARPGSave.campaign,id)!=State::Rewarded)++count;return count;
}
void DrawOverview() {
    if(!AvailableConnection())Theme::Muted("Party quests: connect through Multiplayer to share an adventure.");
    else if(!ready)Theme::Muted(clock<200?"Party quests: connecting...":"Party quests unavailable. The host needs the updated Hyrule Reborn relay.");
    else Theme::Muted("Party / %s   |   %d invitation%s",Team(),Invitations(),Invitations()==1?"":"s");
}
void DrawDetail(int id) {
    Theme::Colored({.6f,.85f,1,1},"%s",StateLabel(id));
    if(Shared(id)&&Bounty(*Find(id)))Theme::Wrapped("Bounty scaled for %d players. Targets and rewards stay raised if someone leaves.",PartySize(gARPGSave.campaign.quests[id-1]));
    if(Shared(id)) {
      if(Theme::Section("Party sharing options")) {
        Theme::Wrapped("Shared objectives catch up when you reconnect to this party. Return to the contact yourself to choose your reward.");
        if(Theme::Button("Keep quest / Leave party sharing",{-1,34}))Leave(id);
      }
    } else if(Status(gARPGSave.campaign,id)==State::Active||Status(gARPGSave.campaign,id)==State::Ready) {
        ImGui::BeginDisabled(!ready);if(Theme::Button("Share quest with party",{-1,36}))Join(id);ImGui::EndDisabled();
        if(!ready)DrawOverview();
    }
}
void DrawBoard() {
    bool compact=ImGui::GetContentRegionAvail().y<280;
    if(!compact)Theme::Label("QUEST TOGETHER",22,IM_COL32(158,217,255,255));
    DrawOverview();
    if(!compact) {
        Theme::Wrapped("Join a shared quest to catch up and contribute from anywhere in the party. Story prerequisites still apply. Everyone claims their own reward.");
        if(!message.empty())Theme::Wrapped("%s",message.c_str());
    }
    Theme::Rule();
    int count=0,completed=0;
    for(const auto& [id,q]:quests)if(Status(gARPGSave.campaign,id)==State::Rewarded)++completed;
    std::vector<int> order;for(const auto& [id,q]:quests)order.push_back(id);
    auto rank=[](int id) {auto state=Status(gARPGSave.campaign,id);return !Member(id)&&state!=State::Rewarded?0:state==State::Active?1:state==State::Ready?2:3;};
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){return rank(a)<rank(b);});
    for(int id:order) {
        const auto& q=quests.at(id);
        if(!showCompleted&&Status(gARPGSave.campaign,id)==State::Rewarded)continue;
        if(dismissed.count(id)&&!Member(id))continue;
        ++count;ImGui::PushID(id);const auto& d=*Find(id);
        Theme::Colored({1,.91f,.68f,1},"%s",d.title);Theme::Muted("%s / Level %u",Chapters[d.chapter],d.level);
        if(Online(id)) {
            if(Theme::Button(Status(gARPGSave.campaign,id)==State::Rewarded?"View completed quest":"Track / Open quest",{-1,36})) {if(Status(gARPGSave.campaign,id)!=State::Rewarded)gARPGSave.campaign.tracked=id;RequestJournal(id);}
            Theme::Muted("%s",StateLabel(id));
        } else {
            const char* blocked=PartyRules::JoinBlock(gARPGSave.campaign,id,LINK_IS_ADULT);
            ImGui::BeginDisabled(blocked||pending.count(id));
            if(Theme::Button(pending.count(id)?"Joining...":"Join quest / Catch up",{-1,36}))Join(id);
            ImGui::EndDisabled();
            if(blocked) {Theme::Wrapped("%s",blocked);if(d.previous&&!Rewarded(gARPGSave.campaign,d.previous))Theme::Wrapped("Requires: %s",Find(d.previous)->title);}
        }
        for(const auto& entry:q["members"].items()) {
            uint32_t peer=0;try{peer=std::stoul(entry.key());}catch(...){continue;}
            auto* a=Anchor::Instance;auto it=a->clients.find(peer);
            if(it!=a->clients.end())Theme::Wrapped("%s%s",it->second.name.c_str(),entry.value()==true?" / reward claimed":" / joined");
        }
        ARPGQuestProgress preview{};preview.partySize=q.value("size",1);
        Theme::Muted("%zu players joined / shared progress",q["members"].size());
        int done=0,total=0;
        if(q.contains("progress")&&q["progress"].is_array()&&q["progress"].size()==3)for(int i=0;i<3;++i) {
            if(q["progress"][i].is_number_integer())done+=q["progress"][i].get<int>();
            total+=Target(preview,d.objectives[i]);
        }
        if(total)ImGui::ProgressBar(std::clamp(done/float(total),0.f,1.f),{-1,6},"");
        if(Bounty(d))Theme::Muted("Bounty scaled for %d players",PartySize(preview));
        Theme::Wrapped("Each player: %u XP / %u rupees / choice of gear",RewardXp(preview,d),RewardRupees(preview,d));
        for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)if(d.objectives[i].count&&q.contains("progress")&&q["progress"].is_array()&&q["progress"].size()==3&&q["progress"][i].is_number_integer())
            Theme::Wrapped("%d/%u  %s",q["progress"][i].get<int>(),Target(preview,d.objectives[i]),d.objectives[i].text);
        if(!Online(id)&&Theme::Button("Dismiss invitation",{-1,30}))dismissed.insert(id);
        Theme::Rule();ImGui::PopID();
    }
    if(completed&&Theme::Button(showCompleted?"Hide completed quests":("Show completed quests ("+std::to_string(completed)+")").c_str(),{-1,32}))showCompleted=!showCompleted;
    if(!count)Theme::Wrapped("No shared quests yet. Open a quest in your journal and choose Share quest with party.");
    if(!dismissed.empty()&&Theme::Button("Show dismissed invitations",{-1,34}))dismissed.clear();
}
void DrawTracker(int id) {
    if(Invitations())Theme::Wrapped("%d party invitations / J > Party",Invitations());
    if(Shared(id))Theme::Wrapped("%s",StateLabel(id));
}
}
