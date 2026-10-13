#include "QuestSystem.h"
#include "QuestParty.h"
#include "QuestDialogue.h"
#include "QuestWorld.h"
#include "ARPG.h"
#include "ARPGSave.h"
#include "ARPGTheme.h"
#include "EnemySystem.h"
#include "ExpeditionSystem.h"
#include "SkillSystem.h"
#include "InventorySystem.h"
#include "ItemModifierSystem.h"
#include "soh/ShipInit.hpp"
#include "soh/Notification/Notification.h"
#include "soh/Enhancements/nametag.h"
#include "soh/Enhancements/EnemyDifficulty/EnemyDifficulty.h"
#include "soh/ObjectExtension/ActorMaximumHealth.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <imgui.h>
#include <cmath>
#include <set>
#include <vector>
#include <sstream>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;
}
namespace ARPG::Quests {
namespace {
static_assert(int(Tune::Saria)==OCARINA_SONG_SARIAS && int(Tune::Epona)==OCARINA_SONG_EPONAS && int(Tune::Sun)==OCARINA_SONG_SUNS && int(Tune::Storms)==OCARINA_SONG_STORMS);
WatchClock watchClock{};
int dialogueQuest=0,dialogueTopic=0,lastDeliveryQuest=0;
std::string deliveryReply;
struct SiteRef {int quest,objective,node;};
std::vector<SiteRef> visible;
struct PackMember {Actor* actor;int quest,objective,node;bool shared=false;float hpRatio=1.f;};
std::vector<PackMember> pack;
void ClearPack(bool destroy) {
    auto old=pack;pack.clear();
    if(destroy)for(auto member:old)if(member.actor && member.actor->update)Actor_Kill(member.actor);
}
void SpawnPack() {
    if(!pack.empty() || Expeditions::Busy())return;
    for(const auto& d:Catalog)if(Status(gARPGSave.campaign,d.id)==State::Active)for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
        const auto& o=d.objectives[i];if(o.kind!=Kind::Pack || o.scene!=gPlayState->sceneNum)continue;
        auto& p=gARPGSave.campaign.quests[d.id-1];int remaining=o.count-p.progress[i];if(!remaining)continue;
        if(Party::Shared(d.id)) {
            for(int n=0;n<o.count;++n)if(!(p.nodes[i]&(1<<n))) {
                const auto& site=p.sites[i][n];if(!site.placed||site.room!=gPlayState->roomCtx.curRoom.num)continue;
                Vec3f position{site.x,site.y,site.z};
                auto* actor=Enemies::SpawnSharedQuestMember(position,d.id,n);if(!actor)continue;
                pack.push_back({actor,d.id,i,n,true});
                NameTagOptions options{};options.tag="ARPGQuestPack";options.yOffset=30;options.scale=.3f;options.textColor={255,213,98,255};
                NameTag_RegisterForActorWithOptions(actor,d.id==5?"Milk-road ambusher":"Thornbound stalker",options);
            }
            if(!pack.empty())return;
            continue;
        }
        auto enemies=Enemies::SpawnQuestWave(remaining);if(int(enemies.size())!=remaining)return;
        size_t index=0;
        for(int n=0;n<o.count;++n)if(!(p.nodes[i]&(1<<n))) {
            auto* actor=enemies[index++];pack.push_back({actor,d.id,i,n});
            NameTagOptions options{};options.tag="ARPGQuestPack";options.yOffset=30;options.scale=.3f;options.textColor={255,213,98,255};
            NameTag_RegisterForActorWithOptions(actor,d.id==5?"Milk-road ambusher":"Thornbound stalker",options);
        }
        return;
    }
}
std::set<int> checked,unsupported;
int selected=1,filter=0,chapter=-1,channelKey=-1,channelFrames=0,lastHealth=-1;
int lastScene=-1,lastRoom=-1,restFrames=0,placementTick=0,markerTick=0;
bool openRequested=false,wasEnabled=false,compactDetails=false,partyPage=false;
std::string notice;
auto& Campaign() {return gARPGSave.campaign;}
// Capture only local mutations. Incoming party snapshots never run this scope,
// preventing echoed progress from becoming another contribution.
struct PartyChange {
    ARPGCampaignData before=Campaign();
    ~PartyChange() {Party::Publish(before);}
};
void ReconcilePack() {
    std::vector<Actor*> remove;
    pack.erase(std::remove_if(pack.begin(),pack.end(),[&](const auto& m) {
        if((Campaign().quests[m.quest-1].nodes[m.objective]&(1<<m.node))||m.shared!=Party::Shared(m.quest)) {remove.push_back(m.actor);return true;}
        return false;
    }),pack.end());
    for(auto* actor:remove)if(actor&&actor->update)Actor_Kill(actor);
    for(auto& m:pack)if(m.shared&&m.actor&&m.actor->update&&m.actor->colChkInfo.health>0) {
        float ratio=Party::PackHealthRatio(m.quest);
        if(std::abs(ratio-m.hpRatio)>.001f) {
            int maximum=GetActorMaximumHealth(m.actor);
            EnemyDifficulty::ApplyHealthMultiplier(m.actor,ratio/m.hpRatio);
            SetActorMaximumHealth(m.actor,std::clamp<int>(std::lround(maximum*ratio/m.hpRatio),1,255));
            m.hpRatio=ratio;
        }
    }
}
bool Adult() {return LINK_IS_ADULT;}
int Key(SiteRef r) {return ((r.quest-1)*ARPG_QUEST_OBJECTIVES+r.objective)*ARPG_QUEST_NODES+r.node;}
ARPGQuestSite& Site(SiteRef r) {return Campaign().quests[r.quest-1].sites[r.objective][r.node];}
float Distance(const Vec3f& a,const Vec3f& b) {return std::hypot(a.x-b.x,a.z-b.z);}
Vec3f Position(const ARPGQuestSite& s) {return {s.x,s.y,s.z};}
int ContactFor(Actor* actor) {
    if(!gPlayState || !actor || !actor->update || !actor->draw || actor->freezeTimer>0 || Enemies::IsWatchman(actor))return -1;
    int scene=gPlayState->sceneNum,id=actor->id;
    bool kokiri=scene==SCENE_KOKIRI_FOREST || scene==SCENE_KNOW_IT_ALL_BROS_HOUSE || scene==SCENE_TWINS_HOUSE ||
        scene==SCENE_MIDOS_HOUSE || scene==SCENE_SARIAS_HOUSE || scene==SCENE_KOKIRI_SHOP || scene==SCENE_LOST_WOODS;
    if(kokiri && (id==ACTOR_EN_KO || id==ACTOR_EN_MD || id==ACTOR_EN_SA))return int(Contact::Kokiri);
    if(scene==SCENE_LON_LON_RANCH && (id==ACTOR_EN_MA1 || id==ACTOR_EN_MA2 || id==ACTOR_EN_MA3))return int(Contact::Ranch);
    if(scene==SCENE_GORON_CITY && (id==ACTOR_EN_GO2 || id==ACTOR_EN_DU))return int(Contact::Goron);
    if(scene==SCENE_ZORAS_DOMAIN && (id==ACTOR_EN_ZO || id==ACTOR_EN_KZ))return int(Contact::Zora);
    if(scene==SCENE_KAKARIKO_VILLAGE && id==ACTOR_EN_NIW_LADY)return int(Contact::Kakariko);
    if(scene==SCENE_LAKESIDE_LABORATORY && id==ACTOR_EN_MK)return int(Contact::Scientist);
    if((scene==SCENE_GERUDO_VALLEY || scene==SCENE_CARPENTERS_TENT) && id==ACTOR_EN_TORYO)return int(Contact::Carpenter);
    if(scene==SCENE_GERUDOS_FORTRESS && id==ACTOR_EN_GE1)return int(Contact::Gerudo);
    return -1;
}
Dialogue::Voice VoiceFor(Actor* actor) {
    using V=Dialogue::Voice;
    switch(actor->id) {
        case ACTOR_EN_MD:return V::Mido;case ACTOR_EN_SA:return V::Saria;
        case ACTOR_EN_MA1:case ACTOR_EN_MA2:case ACTOR_EN_MA3:return V::Malon;
        case ACTOR_EN_DU:return V::Darunia;case ACTOR_EN_GO2:return V::Goron;
        case ACTOR_EN_KZ:return V::KingZora;case ACTOR_EN_ZO:return V::Zora;
        case ACTOR_EN_NIW_LADY:return V::CuccoKeeper;case ACTOR_EN_MK:return V::Scientist;
        case ACTOR_EN_TORYO:return V::Carpenter;case ACTOR_EN_GE1:return V::Gerudo;
        default:return V::Kokiri;
    }
}
bool PendingDelivery(const Definition& d,int i,int contact) {
    const auto& o=d.objectives[i];const auto& p=Campaign().quests[d.id-1];
    return AgeMatches(d.age,Adult()) && Status(Campaign(),d.id)==State::Active && o.kind==Kind::Delivery && o.scene==gPlayState->sceneNum &&
        o.detail==contact && p.progress[i]<o.count && PriorComplete(p,d,i);
}
bool ClearLine(Vec3f a,Vec3f b) {
    a.y+=35;b.y+=35;Vec3f hit{};CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
    return !BgCheck_EntityLineTest1(&gPlayState->colCtx,&a,&b,&hit,&poly,true,true,true,true,&bg);
}
bool ContactNear(Actor* actor) {
    if(!Enabled() || ContactFor(actor)<0 ||
       (actor->room!=-1 && actor->room!=gPlayState->roomCtx.curRoom.num))return false;
    auto* player=GET_PLAYER(gPlayState);
    return player && Distance(actor->world.pos,player->actor.world.pos)<220 &&
        std::abs(actor->world.pos.y-player->actor.world.pos.y)<100 && ClearLine(actor->world.pos,player->actor.world.pos);
}
Actor* NearbyActor() {
    if(!Enabled())return nullptr;
    auto* player=GET_PLAYER(gPlayState);if(!player)return nullptr;
    Actor* nearest=nullptr;float range=220;
    for(int c=0;c<ACTORCAT_MAX;++c)for(auto* a=gPlayState->actorCtx.actorLists[c].head;a;a=a->next) {
        if(ContactFor(a)<0 || (a->room!=-1 && a->room!=gPlayState->roomCtx.curRoom.num))continue;
        float d=Distance(a->world.pos,player->actor.world.pos);
        if(d<range && std::abs(a->world.pos.y-player->actor.world.pos.y)<100 && ClearLine(a->world.pos,player->actor.world.pos)) {range=d;nearest=a;}
    }
    return nearest;
}
bool AtContact(int id) {const auto* d=Find(id);auto* a=NearbyActor();return d && a && AgeMatches(d->age,Adult()) && ContactFor(a)==int(d->contact);}
bool AtContact(Actor* actor,int id) {
    const auto* d=Find(id);
    return d && ContactNear(actor) && AgeMatches(d->age,Adult()) && ContactFor(actor)==int(d->contact);
}
void Announce(std::string text) {
    // The native toast window does not wrap text. Keep quest messages readable at 640px, too.
    for(size_t line=0;text.size()-line>32;) {
        size_t space=text.rfind(' ',line+32);
        if(space==std::string::npos || space<=line)break;
        text[space]='\n';line=space+1;
    }
    Notification::Emit({.message=text,.remainingTime=4.f});
}
void AnnounceReady(const std::array<uint8_t,ARPG_QUEST_COUNT>& before) {
    for(const auto& d:Catalog)if(before[d.id-1]!=uint8_t(State::Ready) && Status(Campaign(),d.id)==State::Ready)
        Announce(std::string("Quest ready: ")+d.title+". Return to "+Contacts[int(d.contact)]+".");
}
std::array<uint8_t,ARPG_QUEST_COUNT> States() {
    std::array<uint8_t,ARPG_QUEST_COUNT> result{};for(int i=0;i<ARPG_QUEST_COUNT;++i)result[i]=Campaign().quests[i].state;return result;
}
uint32_t NativeStory() {
    uint32_t flags=0;
    const int items[]={QUEST_KOKIRI_EMERALD,QUEST_GORON_RUBY,QUEST_ZORA_SAPPHIRE,QUEST_MEDALLION_FOREST,
        QUEST_MEDALLION_FIRE,QUEST_MEDALLION_WATER,QUEST_MEDALLION_SHADOW,QUEST_MEDALLION_SPIRIT};
    for(int i=0;i<8;++i)if(CHECK_QUEST_ITEM(items[i]))flags|=1<<i;
    if(GET_EVENTCHKINF_CARPENTERS_FREE_ALL())flags|=Carpenters;
    if(GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_FOREST_TRIAL) && GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_FIRE_TRIAL) &&
       GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_WATER_TRIAL) && GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_SHADOW_TRIAL) &&
       GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_SPIRIT_TRIAL) && GET_EVENTCHKINF(EVENTCHKINF_COMPLETED_LIGHT_TRIAL))flags|=Trials;
    return flags;
}
Foe EnemyType(int id) {
    switch(id) {
        case ACTOR_EN_DEKUBABA:case ACTOR_EN_DEKUNUTS:case ACTOR_EN_KAREBABA:return Foe::Plant;
        case ACTOR_EN_ST:case ACTOR_EN_SW:return Foe::Spider;
        case ACTOR_EN_FIREFLY:return Foe::Bat;
        case ACTOR_EN_ZF:return Foe::Lizard;
        case ACTOR_EN_WF:return Foe::Wolf;
        case ACTOR_EN_TEST:case ACTOR_EN_SKB:return Foe::Skeleton;
        case ACTOR_EN_TITE:return Foe::Tektite;
        case ACTOR_EN_RD:case ACTOR_EN_POH:case ACTOR_EN_PO_FIELD:return Foe::Undead;
        case ACTOR_EN_DODONGO:case ACTOR_EN_DODOJR:return Foe::Dodongo;
        case ACTOR_EN_FZ:return Foe::Ice;
        case ACTOR_EN_IK:return Foe::IronKnuckle;
        default:return Foe::Any;
    }
}
bool ThreatNearby() {
    auto position=GET_PLAYER(gPlayState)->actor.world.pos;
    for(int category:{ACTORCAT_ENEMY,ACTORCAT_BOSS})for(auto* a=gPlayState->actorCtx.actorLists[category].head;a;a=a->next)
        if(a->update && a->colChkInfo.health>0 && !Enemies::IsWatchman(a) && std::abs(a->world.pos.y-position.y)<100 && Distance(a->world.pos,position)<260)return true;
    return false;
}
void Reset() {
    ClearPack(false);watchClock={};ResetWatches(Campaign());
    visible.clear();checked.clear();unsupported.clear();channelKey=-1;channelFrames=0;restFrames=0;placementTick=0;markerTick=0;
    lastHealth=lastScene=lastRoom=-1;openRequested=false;notice.clear();lastDeliveryQuest=0;deliveryReply.clear();NameTag_RemoveAllByTag("ARPGQuest");
}
void RefreshMarkers() {
    NameTag_RemoveAllByTag("ARPGQuest");
    if(!Enabled())return;
    for(int category=0;category<ACTORCAT_MAX;++category)for(auto* a=gPlayState->actorCtx.actorLists[category].head;a;a=a->next) {
        int contact=ContactFor(a);if(contact<0)continue;
        bool available=false,ready=false,active=false;
        for(const auto& d:Catalog)if(int(d.contact)==contact && AgeMatches(d.age,Adult())) {
            available|=Available(Campaign(),d.id,Adult());ready|=Status(Campaign(),d.id)==State::Ready;active|=Status(Campaign(),d.id)==State::Active;
        }
        for(const auto& d:Catalog)for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)ready|=PendingDelivery(d,i,contact);
        if(!available && !ready && !active)continue;
        NameTagOptions options{};options.tag="ARPGQuest";options.yOffset=30;options.scale=.28f;
        options.textColor=ready?Color_RGBA8{255,220,90,255}:available?Color_RGBA8{255,237,150,255}:Color_RGBA8{175,205,214,255};
        NameTag_RegisterForActorWithOptions(a,ready?"? Quest ready":available?"! Quests available":"Quest contact",options);
    }
}
void RefreshSites() {
    visible.clear();int scene=gPlayState->sceneNum,room=gPlayState->roomCtx.curRoom.num;
    for(const auto& d:Catalog) {
        auto& p=Campaign().quests[d.id-1];if(!p.state)continue;
        for(int o=0;o<ARPG_QUEST_OBJECTIVES;++o) {
            const auto& objective=d.objectives[o];if(!Physical(objective) || objective.scene!=scene || !PriorComplete(p,d,o))continue;
            for(int n=0;n<objective.count;++n) {
                bool done=(p.nodes[o]&(1<<n));if(done && objective.kind!=Kind::Work)continue;
                SiteRef ref{d.id,o,n};auto& site=Site(ref);
                if(site.placed && site.room>=gPlayState->numRooms)site={};
                if(!site.placed || site.room!=room)continue;
                int key=Key(ref);
                if(!checked.count(key)) {
                    auto pos=Position(site);checked.insert(key);
                    if(!Enemies::QuestSiteSupported(pos)) {
                        unsupported.insert(key);if(!done)site={};continue;
                    }
                    site.y=pos.y;
                }
                if(!unsupported.count(key))visible.push_back(ref);
            }
        }
    }
}
bool Spaced(Vec3f candidate) {
    for(const auto& d:Catalog)for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
        if(d.objectives[i].scene!=gPlayState->sceneNum)continue;
        for(const auto& s:Campaign().quests[d.id-1].sites[i])if(s.placed && s.room==gPlayState->roomCtx.curRoom.num &&
            std::abs(s.y-candidate.y)<80 && Distance(Position(s),candidate)<130)return false;
    }
    return true;
}
void PlaceNextSite() {
    // The tracked contract gets placement priority. Position is then saved, so revisiting never duplicates pickups.
    for(int pass=0;pass<2;++pass)for(const auto& d:Catalog) {
        if((pass==0)!=(Campaign().tracked==d.id) || Status(Campaign(),d.id)!=State::Active)continue;
        auto& p=Campaign().quests[d.id-1];
        if(!Party::PlaceSites(d.id))continue;
        for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
            const auto& o=d.objectives[i];if(!(Physical(o)||(o.kind==Kind::Pack&&Party::Shared(d.id))) || o.scene!=gPlayState->sceneNum || !PriorComplete(p,d,i))continue;
            for(int n=0;n<o.count;++n) {
                auto& s=p.sites[i][n];if(s.placed || (p.nodes[i]&(1<<n)))continue;
                for(int attempt=0;attempt<4;++attempt) {
                    Vec3f pos{};if(!Enemies::FindQuestSitePosition(pos,d.id*19+i*7+n*23+attempt*41+placementTick))continue;
                    if(!Spaced(pos))continue;
                    s={pos.x,pos.y,pos.z,int8_t(gPlayState->roomCtx.curRoom.num),1,uint8_t(Adult())};
                    int key=Key({d.id,i,n});checked.erase(key);unsupported.erase(key);return;
                }
                return; // Bound collision work, even with many active contracts.
            }
        }
    }
}
const char* SceneName(int scene) {
    if(scene==SCENE_BOTTOM_OF_THE_WELL)return "Bottom of the Well";
    return Enemies::RegionFor(scene,Adult()).name;
}
int ChannelDuration(const Objective& o) {return o.kind==Kind::Gather?20:60;}
void Conversation(const Definition& d) {
    auto* actor=NearbyActor();bool local=actor && ContactFor(actor)==int(d.contact) && AgeMatches(d.age,Adult());
    if(dialogueQuest!=d.id) {dialogueQuest=d.id;dialogueTopic=0;}
    auto state=Status(Campaign(),d.id);const auto& lines=Dialogue::For(d.id);
    Theme::Colored({.6f,.88f,.7f,1},"%s",local?Dialogue::Name(VoiceFor(actor)):Contacts[int(d.contact)]);
    if(local)Theme::Wrapped("%s",Dialogue::Greeting(VoiceFor(actor),Adult(),Campaign().milestones));
    if(state==State::Unaccepted && !Available(Campaign(),d.id,Adult())) {
        Theme::Muted("This conversation becomes available as the story and local quest chain advance.");return;
    }
    const char* topics[]={"Tell me about it","Why does it matter?","Any advice?"};
    bool changed=false;
    for(int t=0;t<3;++t)if(Theme::Button(topics[t],{-1,30},dialogueTopic==t)) {dialogueTopic=t;changed=true;}
    auto stage=dialogueTopic==1?Dialogue::Stage::Why:dialogueTopic==2?Dialogue::Stage::Hint:
        state==State::Rewarded?Dialogue::Stage::Thanks:state==State::Ready?Dialogue::Stage::Ready:Dialogue::Stage::Offer;
    const char* line=Dialogue::Line(d.id,stage,local?VoiceFor(actor):Dialogue::Voice::Kokiri);
    Theme::Wrapped("%s",line);
    if(changed && ImGui::GetWindowHeight()<250)ImGui::SetScrollHereY(.2f);
    if(!local)Theme::Muted("Journal record. Approach the contact to accept or collect your reward.");
}
void DeliveryTopics(const Definition& d) {
    if(lastDeliveryQuest==d.id) {Theme::Wrapped("%s",deliveryReply.c_str());Theme::Rule();}
    auto* actor=NearbyActor();if(!actor)return;int contact=ContactFor(actor);
    for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)if(PendingDelivery(d,i,contact)) {
        Theme::Colored({.6f,.88f,.7f,1},"Speak with %s",Dialogue::Name(VoiceFor(actor)));
        Theme::Wrapped("%s",d.objectives[i].text);
        ImGui::PushID(i);
        if(Theme::Button("Discuss / hand over quest delivery",{-1,40}))DeliverNearby(d.id,i);
        ImGui::PopID();
    }
}
void ObjectiveText(const Definition& d) {
    const auto& p=Campaign().quests[d.id-1];
    for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
        const auto& o=d.objectives[i];if(o.kind==Kind::None)continue;
        bool done=p.progress[i]>=Target(p,o);
        Theme::Colored(done?ImVec4{.55f,.9f,.66f,1}:ImVec4{.95f,.91f,.78f,1},"%s %u / %u",done?"Complete":"Objective",p.progress[i],Target(p,o));
        Theme::Wrapped("%s",o.text);
        if(o.kind==Kind::Pack)Theme::Muted("Only the named quest pack counts. Walk onto open ground in the target region to find it.");
        if(o.kind==Kind::Recover)Theme::Muted("One quest item per matching defeat; carried in your quest satchel.");
        if(o.kind==Kind::Song)Theme::Muted("Play the learned melody freely on your ocarina in this region, after preceding steps.");
        if(o.kind==Kind::Delivery)Theme::Muted("Approach the named recipient, press J, and choose the delivery topic.");
        if(o.kind==Kind::Vigil)Theme::Muted("Remain still and unharmed on solid ground. Movement, damage, leaving or reloading restarts an unfinished watch. Pausing freezes it.");
        if(o.kind==Kind::Survey)Theme::Muted("Stand still in each survey ring for 3 seconds. Examine all three distinct sites.");
        if(o.kind==Kind::Survey || o.kind==Kind::Vigil)Theme::Muted("Time: %s. In regions with a fixed sky, leave and return at the required time.",o.detail==uint8_t(Period::Night)?"night":o.detail==uint8_t(Period::Day)?"day":"any");
        if(o.kind==Kind::Gather || o.kind==Kind::Work)Theme::Muted(o.kind==Kind::Work?"Stand still inside each ring for 3 seconds. Clear nearby threats first.":"Stand still inside each ring for 1 second to gather.");
        if(Ordered(o) && !PriorComplete(p,d,i))Theme::Muted("Complete the preceding objectives first.");
        ImGui::Spacing();
    }
}
void Detail(const Definition& d) {
    static int previous=0;if(previous!=d.id) {ImGui::SetScrollY(0);previous=d.id;}
    auto status=Status(Campaign(),d.id);
    Theme::Label(d.title,26,IM_COL32(255,232,158,255));Theme::Muted("%s / Suggested level %u",Chapters[d.chapter],d.level);
    Party::DrawDetail(d.id);
    DeliveryTopics(d);Conversation(d);ImGui::Spacing();Theme::Rule();
    Theme::Wrapped("Contact: %s",Contacts[int(d.contact)]);Theme::Wrapped("Find them: %s",ContactPlaces[int(d.contact)]);
    if(d.age!=Age::Either)Theme::Muted("Speak as %s Link to accept or turn in. Accepted objectives survive time travel.",d.age==Age::Adult?"adult":"child");
    ImGui::Spacing();ObjectiveText(d);Theme::Rule();
    Theme::Print("Rewards: %u XP / %u rupees / choose one piece of gear",RewardXp(Campaign().quests[d.id-1],d),RewardRupees(Campaign().quests[d.id-1],d));
    for(int choice=0;choice<2;++choice) {
        auto item=RewardItem(d,choice,Adult());
        Theme::Wrapped("%s",ItemModifierSystem::ItemName(item).c_str());
        for(int m=0;m<item.modCount;++m)Theme::Muted("%s",ItemModifierSystem::DescribeModifier(item.mods[m]).c_str());
    }
    if(status==State::Unaccepted) {
        if(!Available(Campaign(),d.id,Adult())) {
            if(d.previous && !Rewarded(Campaign(),d.previous))Theme::Wrapped("First complete: %s",Find(d.previous)->title);
            if((Campaign().milestones&d.gate)!=d.gate)Theme::Wrapped("Continue the main story: %s",d.gate==AllSages?"awaken the five temple sages":d.gate==Emerald?"Kokiri Emerald":d.gate==Ruby?"Goron Ruby":d.gate==Sapphire?"Zora Sapphire":d.gate==Forest?"Forest Medallion":d.gate==Fire?"Fire Medallion":d.gate==Water?"Water Medallion":d.gate==Shadow?"Shadow Medallion":d.gate==Spirit?"Spirit Medallion":d.gate==(Forest|Fire)?"Forest and Fire Medallions":"rescue all four carpenters");
        } else {
            bool near=AtContact(d.id);ImGui::BeginDisabled(!near);
            if(Theme::Button("I will help / Accept quest",{-1,40}))AcceptNearby(d.id);
            ImGui::EndDisabled();if(!near)Theme::Muted("Speak with the contact nearby to accept.");
        }
    } else if(status==State::Ready) {
        Theme::Colored({1,.86f,.4f,1},"Return to your contact to collect your reward.");
        bool near=AtContact(d.id);ImGui::BeginDisabled(!near||!Party::CanClaim(d.id));
        if(!Party::CanClaim(d.id))Theme::Wrapped("Confirming party progress...");
        for(int choice=0;choice<2;++choice) {
            auto label=std::string("Choose ")+Loot::SlotNames[RewardItem(d,choice,Adult()).slot];
            if(Theme::Button(label.c_str(),{-1,36}))ClaimNearby(d.id,choice);
        }
        ImGui::EndDisabled();
    } else if(status==State::Rewarded)Theme::Colored({.55f,.9f,.66f,1},"Quest complete. Reward received.");
    if(status==State::Active || status==State::Ready) {
        bool tracked=Campaign().tracked==d.id;
        if(Theme::Button(tracked?"Stop tracking":"Track this quest",{-1,36},tracked))Campaign().tracked=tracked?0:d.id;
    }
    if(!notice.empty()) {ImGui::Spacing();Theme::Wrapped("%s",notice.c_str());}
}
}
bool Enabled() {return ARPG::Enabled() && gPlayState && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.Quests"),1)!=0;}
int NearbyContact() {auto* actor=NearbyActor();return actor?ContactFor(actor):-1;}
bool IsQuestEnemy(Actor* actor) {return std::any_of(pack.begin(),pack.end(),[actor](auto m){return m.actor==actor;});}
bool HasQuests(Actor* actor) {return Enabled() && ContactFor(actor)>=0;}
void RequestJournal(int quest) {partyPage=false;openRequested=true;if(Find(quest)){selected=quest;compactDetails=true;}}
bool ConsumeJournalRequest() {bool value=openRequested;openRequested=false;return value;}
std::string DialogueContext(Actor* actor) {
    if(!HasQuests(actor))return {};
    int contact=ContactFor(actor);std::ostringstream out;int shown=0;
    out<<"\nCharacter voice: "<<AuthoredGreeting(actor)<<" Only discuss what this person can know. Kokiri do not recognize adult Link. Kakariko's cucco keeper does not know about time travel. The adult Zora domain remains frozen even after the lake recovers. Never identify a temple's hidden enemy before the player discovers it.";
    out<<"\nVerified local quest ledger. Accepting quests, delivering items and receiving rewards require an explicit Local quests menu choice or journal action. Generated dialogue never changes progress.";
    for(const auto& d:Catalog) {
        bool delivery=false;for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)delivery|=PendingDelivery(d,i,contact);
        if(int(d.contact)!=contact && !delivery)continue;
        auto status=Status(Campaign(),d.id);bool available=Available(Campaign(),d.id,Adult());
        if(!AgeMatches(d.age,Adult()) || (!available && status!=State::Active && status!=State::Ready))continue;
        if(shown++>=6)break;
        out<<"\n"<<d.title<<" ["<<(available?"can offer":status==State::Ready?"ready to turn in":"in progress")<<"]: "<<(delivery?"A delivery awaits your explicit Local quests menu choice.":status==State::Ready?Dialogue::For(d.id).ready:d.offer);
        if(!delivery)out<<" Background: "<<Dialogue::For(d.id).why<<" Advice: "<<Dialogue::For(d.id).hint;
        for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)if(d.objectives[i].count)
            out<<" "<<d.objectives[i].text<<" ("<<int(Campaign().quests[d.id-1].progress[i])<<"/"<<Target(Campaign().quests[d.id-1],d.objectives[i])<<").";
        out<<" Rewards: "<<RewardXp(Campaign().quests[d.id-1],d)<<" XP, "<<RewardRupees(Campaign().quests[d.id-1],d)<<" rupees and a choice of gear.";
    }
    if(!shown)out<<" No available or unfinished quests from this contact at present.";
    return out.str();
}
std::string AuthoredGreeting(Actor* actor) {
    return HasQuests(actor)?Dialogue::Greeting(VoiceFor(actor),Adult(),Campaign().milestones):"";
}
int LocalQuest(Actor* actor) {
    if(!HasQuests(actor))return 0;int contact=ContactFor(actor),best=0,priority=0;
    for(const auto& d:Catalog) {
        auto status=Status(Campaign(),d.id);int rank=0;
        if(int(d.contact)==contact && AgeMatches(d.age,Adult()))rank=status==State::Ready?4:status==State::Active?3:Available(Campaign(),d.id,Adult())?2:0;
        for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)if(PendingDelivery(d,i,contact))rank=5;
        if(rank>priority) {priority=rank;best=d.id;}
    }
    return best;
}
std::string AuthoredReply(Actor* actor,bool advice) {
    int id=LocalQuest(actor);if(!id)return AuthoredGreeting(actor);
    const auto& d=*Find(id);auto state=Status(Campaign(),id);
    for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)if(PendingDelivery(d,i,ContactFor(actor)))
        return std::string(d.title)+": Choose Local quests and the delivery topic so we can take care of this.";
    return std::string(d.title)+"\n"+Dialogue::Line(id,advice?Dialogue::Stage::Hint:state==State::Ready?Dialogue::Stage::Ready:state==State::Active?Dialogue::Stage::Why:Dialogue::Stage::Offer,VoiceFor(actor));
}
void OpenFor(Actor* actor) {
    if(!HasQuests(actor))return;filter=2;chapter=-1;
    int best=LocalQuest(actor);if(best)selected=best;
    RequestJournal(selected);Inventory::Open();
}
static bool DeliverAt(Actor* actor,int id,int objective) {
    PartyChange change;
    if(!ContactNear(actor))return false;
    auto before=States();
    if(!Deliver(Campaign(),id,objective,gPlayState->sceneNum,Contact(ContactFor(actor))))return false;
    lastDeliveryQuest=id;deliveryReply=std::string(Dialogue::Name(VoiceFor(actor)))+": "+Dialogue::DeliveryReply(id,objective);
    notice=deliveryReply;
    Announce("Delivery recorded in your journal.");AnnounceReady(before);RefreshMarkers();return true;
}
static bool AcceptAt(Actor* actor,int id) {
    if(!AtContact(actor,id))return false;
    ObserveStory(Campaign(),NativeStory());
    if(!Accept(Campaign(),id,Adult()))return false;
    selected=id;notice=std::string("Quest accepted. ")+Dialogue::Line(id,Dialogue::Stage::Hint,VoiceFor(actor));Announce(std::string("Accepted: ")+Find(id)->title);RefreshMarkers();return true;
}
static bool ClaimAt(Actor* actor,int id,int choice) {
    PartyChange change;
    if(!AtContact(actor,id))return false;
    if(!Party::CanClaim(id)) {notice="Waiting for the party to confirm your progress...";return false;}
    if(!Claim(gARPGSave,id,choice,Adult())) {notice="Make room for one item in your backpack. Your reward is waiting here.";return false;}
    const auto& d=*Find(id);SkillSystem::GrantXp(RewardXp(Campaign().quests[id-1],d));Rupees_ChangeBy(RewardRupees(Campaign().quests[id-1],d));
    notice=d.thanks;Announce(std::string("Completed: ")+d.title+" / +"+std::to_string(RewardXp(Campaign().quests[d.id-1],d))+" XP");RefreshMarkers();return true;
}
bool DeliverNearby(int id,int objective) {return DeliverAt(NearbyActor(),id,objective);}
bool AcceptNearby(int id) {return AcceptAt(NearbyActor(),id);}
bool ClaimNearby(int id,int choice) {return ClaimAt(NearbyActor(),id,choice);}
std::vector<int> ConversationQuests(Actor* actor) {
    std::vector<int> result;
    if(!HasQuests(actor))return result;
    ObserveStory(Campaign(),NativeStory());
    for(const auto& d:Catalog) {
        auto state=Status(Campaign(),d.id);
        bool local=int(d.contact)==ContactFor(actor) && AgeMatches(d.age,Adult()) &&
            (Available(Campaign(),d.id,Adult()) || state==State::Active || state==State::Ready);
        for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)local|=PendingDelivery(d,i,ContactFor(actor));
        if(local)result.push_back(d.id);
    }
    return result;
}
std::vector<TalkOption> ConversationOptions(Actor* actor,int id) {
    auto quests=ConversationQuests(actor);
    if(std::find(quests.begin(),quests.end(),id)==quests.end())return {};
    const auto& d=*Find(id);
    std::vector<TalkOption> options;
    for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i)if(PendingDelivery(d,i,ContactFor(actor)))
        options.push_back({"Deliver / "+std::string(d.objectives[i].text),TalkAction::Deliver,id,i});
    // A recipient may discuss a delivery without impersonating the quest giver.
    if(int(d.contact)!=ContactFor(actor))return options;
    auto state=Status(Campaign(),id);
    options.push_back({"Tell me about it",TalkAction::About,id});
    if(state==State::Unaccepted)options.push_back({"I will help",TalkAction::Accept,id});
    if(state==State::Ready)for(int choice=0;choice<2;++choice)
        options.push_back({std::string("Choose ")+Loot::SlotNames[RewardItem(d,choice,Adult()).slot],TalkAction::Reward,id,choice});
    options.push_back({"Why does it matter?",TalkAction::Why,id});
    options.push_back({"Any advice?",TalkAction::Advice,id});
    if(state==State::Active || state==State::Ready)
        options.push_back({Campaign().tracked==id?"Stop tracking":"Track this quest",TalkAction::Track,id});
    return options;
}
std::string ChooseConversationOption(Actor* actor,const TalkOption& option) {
    // Revalidate at activation: scene, age, actor, quest state and objective
    // ordering may have changed since the menu was assembled.
    if(!ContactNear(actor))return "Come closer so we can talk.";
    auto options=ConversationOptions(actor,option.quest);
    if(std::none_of(options.begin(),options.end(),[&](const auto& current) {
        return current.action==option.action && current.quest==option.quest && current.value==option.value;
    }))return "Let us check what still needs doing.";
    const auto& d=*Find(option.quest);
    auto stage=Dialogue::Stage::Offer;
    switch(option.action) {
        case TalkAction::Accept:
            return AcceptAt(actor,d.id)?std::string("Thank you. ")+Dialogue::Line(d.id,Dialogue::Stage::Hint,VoiceFor(actor)):
                "That request is not available right now.";
        case TalkAction::Deliver:
            return DeliverAt(actor,d.id,option.value)?Dialogue::DeliveryReply(d.id,option.value):"We are not ready for that delivery yet.";
        case TalkAction::Reward:
            return ClaimAt(actor,d.id,option.value)?Dialogue::Line(d.id,Dialogue::Stage::Thanks,VoiceFor(actor)):
                "Make room for one item in your backpack. Your reward is waiting here.";
        case TalkAction::Track:
            Campaign().tracked=Campaign().tracked==d.id?0:d.id;
            return Campaign().tracked?"I have marked the task in your journal.":"You can find the task in your journal whenever you need it.";
        case TalkAction::Why:stage=Dialogue::Stage::Why;break;
        case TalkAction::Advice:stage=Dialogue::Stage::Hint;break;
        case TalkAction::About:stage=Status(Campaign(),d.id)==State::Ready?Dialogue::Stage::Ready:Dialogue::Stage::Offer;break;
    }
    std::string text=Dialogue::Line(d.id,stage,VoiceFor(actor));
    if(option.action==TalkAction::About) {
        text=std::string(d.title)+". "+text;
        text+=" Rewards: "+std::to_string(RewardXp(Campaign().quests[d.id-1],d))+" XP, "+std::to_string(RewardRupees(Campaign().quests[d.id-1],d))+" rupees, and your choice of gear: ";
        for(int choice=0;choice<2;++choice) {
            auto item=RewardItem(d,choice,Adult());
            if(choice)text+=" or ";
            text+=ItemModifierSystem::ItemName(item);
            for(int m=0;m<item.modCount;++m)text+=". "+ItemModifierSystem::DescribeModifier(item.mods[m]);
        }
    }
    return text;
}
void OnDefeat(Actor* actor,bool boss) {
    PartyChange change;
    if(!Enabled() || !actor)return;auto before=States();
    if(boss) {
        uint32_t flag=0;
        switch(actor->id) {
            case ACTOR_BOSS_GOMA:flag=Emerald;break;case ACTOR_BOSS_DODONGO:flag=Ruby;break;case ACTOR_BOSS_VA:flag=Sapphire;break;
            case ACTOR_BOSS_GANONDROF:flag=Forest;break;case ACTOR_BOSS_FD:flag=Fire;break;case ACTOR_BOSS_MO:flag=Water;break;
            case ACTOR_BOSS_SST:flag=Shadow;break;case ACTOR_BOSS_TW:flag=Spirit;break;case ACTOR_BOSS_GANON:flag=Ganondorf;break;
        }
        ObserveStory(Campaign(),flag);
    } else {
        for(auto member:pack)if(member.actor==actor)UseNode(Campaign(),member.quest,member.objective,member.node);
        RecordKill(Campaign(),gPlayState->sceneNum,EnemyType(actor->id));
    }
    AnnounceReady(before);
}
void Tick() {
    PartyChange change;
    if(!Enabled()) {if(wasEnabled){ClearPack(true);Reset();}wasEnabled=false;return;}wasEnabled=true;
    if(!GameInteractor::IsPlayerInControl() || gPlayState->pauseCtx.state || Inventory::IsOpen())return;
    auto* player=GET_PLAYER(gPlayState);if(!player)return;
    if(gPlayState->transitionTrigger!=TRANS_TRIGGER_OFF || gPlayState->roomCtx.status!=0 ||
       (gPlayState->numRooms>1 && gPlayState->roomCtx.prevRoom.num>=0)) {channelFrames=0;watchClock={};ResetWatches(Campaign());return;}
    int scene=gPlayState->sceneNum,room=gPlayState->roomCtx.curRoom.num;
    if(lastScene!=scene || lastRoom!=room) {checked.clear();unsupported.clear();visible.clear();lastScene=scene;lastRoom=room;channelKey=-1;channelFrames=0;}
    auto before=States();ObserveStory(Campaign(),NativeStory());VisitScene(Campaign(),scene);AnnounceReady(before);
    ReconcilePack();
    if(++markerTick%30==1)RefreshMarkers();
    if(markerTick%60==1)SpawnPack();
    bool grounded=player->actor.bgCheckFlags&BGCHECKFLAG_GROUND;
    if(grounded && ++placementTick%20==1)PlaceNextSite();
    RefreshSites();
    bool damaged=lastHealth>=0 && gSaveContext.health<lastHealth;lastHealth=gSaveContext.health;
    bool quiet=grounded && std::abs(player->actor.speedXZ)<.3f && !player->meleeWeaponState && !SkillSystem::IsPerformingAction() &&
        !(player->stateFlags1&(PLAYER_STATE1_IN_WATER|PLAYER_STATE1_ON_HORSE|PLAYER_STATE1_DAMAGED|PLAYER_STATE1_CARRYING_ACTOR|
                              PLAYER_STATE1_READY_TO_FIRE|PLAYER_STATE1_CHARGING_SPIN_ATTACK|PLAYER_STATE1_USING_BOOMERANG)) && !damaged;
    auto watchBefore=States();AdvanceWatches(Campaign(),watchClock,scene,gSaveContext.nightFlag!=0,quiet);AnnounceReady(watchBefore);
    SiteRef target{};int nextKey=-1;float nearest=52;bool rest=false;
    for(auto ref:visible) {
        const auto& s=Site(ref);auto& p=Campaign().quests[ref.quest-1];float distance=Distance(Position(s),player->actor.world.pos);
        if(std::abs(s.y-player->actor.world.pos.y)>30)continue;
        bool done=p.nodes[ref.objective]&(1<<ref.node);
        if(done && distance<90 && Find(ref.quest)->objectives[ref.objective].kind==Kind::Work)rest=true;
        if(!done && PeriodMatches(Find(ref.quest)->objectives[ref.objective],gSaveContext.nightFlag!=0) && distance<nearest && ClearLine(player->actor.world.pos,Position(s))) {nearest=distance;target=ref;nextKey=Key(ref);}
    }
    if(nextKey<0 || nextKey!=channelKey || !quiet) {channelKey=nextKey;channelFrames=0;}
    bool safe=(rest || nextKey>=0) && quiet && !ThreatNearby();
    if(nextKey>=0 && safe) {
        const auto& objective=Find(target.quest)->objectives[target.objective];int duration=ChannelDuration(objective);
        if(++channelFrames>=duration) {
            auto old=States();UseNode(Campaign(),target.quest,target.objective,target.node);AnnounceReady(old);
            Announce(std::string(objective.kind==Kind::Work?"Restored: ":objective.kind==Kind::Survey?"Observed: ":"Gathered: ")+Find(target.quest)->objectives[target.objective].text);
            channelFrames=0;channelKey=-1;restFrames=0;
        }
    } else channelFrames=0;
    if(rest && safe && gSaveContext.health<gSaveContext.healthCapacity) {if(++restFrames>=40) {Health_ChangeBy(gPlayState,4);lastHealth=gSaveContext.health;restFrames=0;}}
    else restFrames=0;
}
int VisibleSites() {return int(visible.size());}
float ChannelProgress() {
    if(channelKey<0)return 0;int quest=channelKey/(ARPG_QUEST_OBJECTIVES*ARPG_QUEST_NODES)+1;
    int objective=(channelKey/ARPG_QUEST_NODES)%ARPG_QUEST_OBJECTIVES;
    return channelFrames/float(ChannelDuration(Find(quest)->objectives[objective]));
}
void DrawJournal() {
    PartyChange change;
    if(!Enabled()) {Theme::Wrapped("Enable Hyrule Reborn quests in Settings & testing to begin.");return;}
    ObserveStory(Campaign(),NativeStory());
    int completed=0,active=0;for(const auto& d:Catalog) {auto s=Status(Campaign(),d.id);completed+=s==State::Rewarded;active+=s==State::Active || s==State::Ready;}
    float width=ImGui::GetContentRegionAvail().x;
    bool compact=width<680 || ImGui::GetContentRegionAvail().y<330;
    if(!compact)Theme::Label("HYRULE REBORN",25,IM_COL32(255,228,152,255));
    Theme::Muted("%d / %d completed   /   %d in progress",completed,ARPG_QUEST_COUNT,active);
    if(openRequested)partyPage=false;
    float tabWidth=(width-6)*.5f;
    if(Theme::Button("My quests",{tabWidth,32},!partyPage))partyPage=false;
    ImGui::SameLine(0,6);
    std::string partyTab="Party";if(Party::Invitations())partyTab+=" ("+std::to_string(Party::Invitations())+")";
    if(Theme::Button(partyTab.c_str(),{tabWidth,32},partyPage))partyPage=true;
    if(partyPage) {Theme::BeginPane("partyQuests",{0,0},true);Party::DrawBoard();Theme::EndPane();return;}
    if(compact && compactDetails) {
        if(Theme::Button("Back to quest list",{-1,34}))compactDetails=false;
        Theme::BeginPane("questDetail",{0,0},true);Detail(*Find(selected));Theme::EndPane();return;
    }
    int near=NearbyContact();if(near>=0)Theme::Colored({1,.87f,.5f,1},"Nearby contact: %s",Contacts[near]);
    else Theme::Muted("Find a contact with a gold !, approach, then open this page to accept quests.");
    const char* filters[]={"In progress","Available","All quests","Completed"};
    float button=(width-18)/4;
    for(int i=0;i<4;++i) {if(i)ImGui::SameLine(0,6);if(Theme::Button(filters[i],{button,32},filter==i))filter=i;}
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##chapter",chapter<0?"All chapters":Chapters[chapter])) {
        if(ImGui::Selectable("All chapters",chapter<0))chapter=-1;
        for(int i=0;i<int(std::size(Chapters));++i)if(ImGui::Selectable(Chapters[i],chapter==i))chapter=i;
        ImGui::EndCombo();
    }
    ImVec2 listSize=compact?ImVec2{0,0}:ImVec2{width*.36f,0};
    Theme::BeginPane("questList",listSize,true);int count=0;
    for(const auto& d:Catalog) {
        auto status=Status(Campaign(),d.id);bool available=Available(Campaign(),d.id,Adult());
        if(chapter>=0 && d.chapter!=chapter)continue;
        if((filter==0 && status!=State::Active && status!=State::Ready) || (filter==1 && !available) || (filter==3 && status!=State::Rewarded))continue;
        ++count;ImGui::PushID(d.id);
        const char* prefix=status==State::Ready?"? ":status==State::Rewarded?"+ ":available?"! ":status==State::Active?"> ":"";
        std::string label=prefix+std::string(d.title);
        if(Theme::Button(label.c_str(),{-1,44},selected==d.id)) {selected=d.id;notice.clear();compactDetails=true;}
        ImGui::PopID();
    }
    if(!count) {Theme::Wrapped(filter==0?"No quests in progress. Choose Available to find your first contact.":"No quests match this view.");}
    Theme::EndPane();if(compact)return;ImGui::SameLine();
    Theme::BeginPane("questDetail",{0,0},true);Detail(*Find(selected));Theme::EndPane();
}
void DrawHud() {
    if(!Enabled())return;
    if(!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_J,false)) {
        if(Inventory::IsOpen())Inventory::Close();else {auto* a=NearbyActor();if(a)OpenFor(a);else {RequestJournal(Campaign().tracked);Inventory::Open();}}
    }
    if(!CVarGetInteger(CVAR_ENHANCEMENT("ARPG.QuestTracker"),1) || GameInteractor::IsGameplayPaused() || !GameInteractor::IsPlayerInControl())return;
    int near=NearbyContact();const auto* d=Find(Campaign().tracked);if(!d && near<0 && !VisibleSites() && !Party::Invitations())return;
    auto* viewport=ImGui::GetMainViewport();float width=std::min(315.f,viewport->WorkSize.x*.34f);
    if(width<190)return;
    ImGui::SetNextWindowPos({viewport->WorkPos.x+viewport->WorkSize.x-16,viewport->WorkPos.y+viewport->WorkSize.y*.24f},ImGuiCond_Always,{1,0});
    ImGui::SetNextWindowSize({width,0},ImGuiCond_Always);ImGui::SetNextWindowBgAlpha(.75f);
    if(ImGui::Begin("##QuestTracker",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoFocusOnAppearing)) {
        Theme::Label("HYRULE REBORN",18,IM_COL32(255,224,143,255));
        Party::DrawTracker(d?d->id:0);
        if(near>=0)Theme::Wrapped("! %s nearby / J or Start > Quests",Contacts[near]);
        if(d) {
            Theme::Wrapped("%s",d->title);const auto& p=Campaign().quests[d->id-1];
            if(Status(Campaign(),d->id)==State::Ready)Theme::Wrapped("? Return to %s: %s",Contacts[int(d->contact)],ContactPlaces[int(d->contact)]);
            else for(int i=0;i<ARPG_QUEST_OBJECTIVES;++i) {
                const auto& o=d->objectives[i];if(!o.count || p.progress[i]>=Target(p,o))continue;
                Theme::Wrapped("%u/%u  %s",p.progress[i],Target(p,o),o.text);
            }
        }
        SiteRef closest{};float range=2000;
        for(auto ref:visible) {
            if(Campaign().quests[ref.quest-1].nodes[ref.objective]&(1<<ref.node))continue;
            if(d && ref.quest!=d->id)continue;
            float distance=Distance(Position(Site(ref)),GET_PLAYER(gPlayState)->actor.world.pos);
            if(distance<range) {closest=ref;range=distance;}
        }
        if(closest.quest) {
            auto pos=Position(Site(closest));auto player=GET_PLAYER(gPlayState)->actor.world.pos;
            int16_t yaw=Math_Vec3f_Yaw(&player,&pos)-GET_PLAYER(gPlayState)->actor.shape.rot.y;
            const char* direction=std::abs(int(yaw))<8192?"Ahead":std::abs(int(yaw))>24576?"Behind":yaw>0?"Right":"Left";
            Theme::Muted("%s / %.0f paces to marked site",direction,range/20.f);
        }
        if(channelKey>=0) {Theme::Wrapped("Stand still in the ring. Clear nearby threats.");ImGui::ProgressBar(ChannelProgress(),{-1,5},"");}
        if(restFrames)Theme::Muted("Resting at a restored site...");
        if(!d && near<0)Theme::Muted("J / Start > Quests to track a contract");
    }
    ImGui::End();
}
void OnSong() {
    PartyChange change;
    if(!Enabled() || gSaveContext.health<=0 || gPlayState->pauseCtx.state || Inventory::IsOpen() ||
       gPlayState->msgCtx.ocarinaAction!=OCARINA_ACTION_FREE_PLAY_DONE)return;
    int tune=gPlayState->msgCtx.lastPlayedSong;
    int learned=tune==int(Tune::Saria)?QUEST_SONG_SARIA:tune==int(Tune::Epona)?QUEST_SONG_EPONA:
        tune==int(Tune::Sun)?QUEST_SONG_SUN:tune==int(Tune::Storms)?QUEST_SONG_STORMS:-1;
    if(learned<0 || !CHECK_QUEST_ITEM(learned))return;
    auto before=States();RecordSong(Campaign(),gPlayState->sceneNum,tune);AnnounceReady(before);
}
static void Register() {
    COND_HOOK(OnGameFrameUpdate,true,Party::Tick);
    COND_HOOK(OnOcarinaSongAction,true,OnSong);
    COND_HOOK(OnPlayerUpdate,true,Tick);
    COND_HOOK(OnActorDestroy,true,[](void* actor){pack.erase(std::remove_if(pack.begin(),pack.end(),[actor](auto m){return m.actor==actor;}),pack.end());});
    COND_HOOK(OnSceneInit,true,[](int16_t){Reset();});
    COND_HOOK(OnLoadGame,true,[](int32_t){Party::Reset();ClearPack(true);Reset();});
    COND_HOOK(OnExitGame,true,[](int32_t){Party::Reset();Reset();});
    COND_HOOK(OnPlayDestroy,true,Reset);
    COND_HOOK(OnPlayDrawEnd,true,[](){
        if(!Enabled())return;
        auto position=GET_PLAYER(gPlayState)->actor.world.pos;
        for(auto ref:visible) {
            const auto& site=Site(ref);if(Distance(Position(site),position)>2400)continue;
            bool done=Campaign().quests[ref.quest-1].nodes[ref.objective]&(1<<ref.node);
            DrawProp(gPlayState,Find(ref.quest)->objectives[ref.objective].prop,site.x,site.y,site.z,done,
                Campaign().tracked==ref.quest,Key(ref)==channelKey?ChannelProgress():0);
        }
    });
}
static RegisterShipInitFunc init(Register);
}
