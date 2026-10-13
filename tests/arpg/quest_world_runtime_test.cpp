#include "soh/Enhancements/arpg/QuestSystem.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/EnemySystem.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cmath>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;extern GameState* gGameState;
void Sram_InitDebugSave(void);
}
namespace {
using namespace ARPG;
namespace Q=ARPG::Quests;
struct Visit {int scene;bool adult;int quest,objective,contact;bool beforeForest=false;bool night=false;};
const Visit visits[]={
    {0x55,false,2,0,int(Q::Contact::Kokiri)},
    {0x63,false,7,0,int(Q::Contact::Ranch)},
    {0x60,false,9,0,-1},
    {0x54,false,16,0,-1},
    {0x52,false,18,1,int(Q::Contact::Kakariko)},
    {0x53,false,20,0,-1},
    {0x58,false,0,0,int(Q::Contact::Zora)},
    {0x62,false,0,0,int(Q::Contact::Goron)},
    {SCENE_KNOW_IT_ALL_BROS_HOUSE,true,0,0,int(Q::Contact::Kokiri),true},
    {0x55,true,4,0,int(Q::Contact::Kokiri)},
    {0x51,true,7,1,-1},
    {0x5B,true,23,0,-1},
    {0x56,true,23,1,-1},
    {0x62,true,26,0,int(Q::Contact::Goron)},
    {0x09,true,27,0,-1},
    {0x57,true,29,0,-1},
    {0x38,true,0,0,int(Q::Contact::Scientist)},
    {0x52,true,32,0,int(Q::Contact::Kakariko)},
    {0x5A,true,34,1,int(Q::Contact::Carpenter)},
    {0x5D,true,0,0,int(Q::Contact::Gerudo)},
    {0x5E,true,35,0,-1},
    {0x51,true,5,0,-1},
    {0x5B,true,21,0,-1},
    {0x55,false,41,1,int(Q::Contact::Kokiri)},
    {0x63,false,43,0,int(Q::Contact::Ranch)},
    {0x63,false,43,1,-1,false,true},
    {0x52,false,44,1,int(Q::Contact::Kakariko)},
    {0x53,false,45,0,-1},
    {0x53,false,46,0,-1,false,true},
    {0x54,false,49,1,-1,false,true},
    {0x63,false,50,0,int(Q::Contact::Ranch)},
    {0x58,false,50,1,int(Q::Contact::Zora)},
    {0x57,true,51,0,-1},
    {0x56,true,52,0,-1},
    {0x56,true,52,1,-1},
    {0x5D,true,53,1,int(Q::Contact::Gerudo)},
    {0x5E,true,54,1,-1},
    {0x38,true,55,0,int(Q::Contact::Scientist)},
    {0x57,true,55,1,-1},
    {0x52,true,56,0,-1,false,true},
    {0x5A,true,58,0,int(Q::Contact::Carpenter)}
};
int phase=0,frames=0,index=0,standing=0;bool captured=false,contact=false,packChecked=false;
uint32_t storyItems=0;
Vec3f entry{};
std::filesystem::path output;
nlohmann::json results=nlohmann::json::array();
int Entrance(int scene) {
    // Spawn 0 here is Jabu's blue warp in deep water, not the normal walking entrance.
    if(scene==SCENE_ZORAS_FOUNTAIN)return ENTR_ZORAS_FOUNTAIN_TUNNEL_EXIT;
    for(int n=0;n<ENTR_MAX;++n)if(gEntranceTable[n].scene==scene && gEntranceTable[n].spawn==0)return n;
    throw std::runtime_error("No entrance");
}
void Report(const char* state,const std::string& error="") {std::ofstream(output)<<nlohmann::json{{"status",state},{"visits",results},{"error",error},{"index",index}}.dump(2);}
void SetAge() {
    int age=visits[index].adult?LINK_AGE_ADULT:LINK_AGE_CHILD;
    if(gPlayState && phase)gPlayState->linkAgeOnLoad=age;else gSaveContext.linkAge=age;
    gSaveContext.cutsceneIndex=0;gSaveContext.nextCutsceneIndex=0xFFEF;gSaveContext.sceneLayer=0;gSaveContext.dayTime=gSaveContext.skyboxTime=visits[index].night?0:0x8000;gSaveContext.nightFlag=visits[index].night;
    gSaveContext.inventory.questItems=visits[index].beforeForest?storyItems&~(1u<<QUEST_MEDALLION_FOREST):storyItems;
    // Kokiri's restored outdoor actor list uses the blue-warp flag, not the medallion alone.
    if(visits[index].beforeForest)CLEAR_EVENTCHKINF(EVENTCHKINF_USED_FOREST_TEMPLE_BLUE_WARP);
    else SET_EVENTCHKINF(EVENTCHKINF_USED_FOREST_TEMPLE_BLUE_WARP);
    captured=contact=packChecked=false;standing=frames=0;
}
void Move(Vec3f p) {auto* player=GET_PLAYER(gPlayState);player->actor.world.pos=player->actor.prevPos=p;player->actor.velocity={};player->actor.speedXZ=0;}
void Tick() {
    if(phase==99)return;
    try {
        ++frames;
        if(!phase && frames>60 && gGameState) {
            CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;
            gSaveContext.gameMode=GAMEMODE_NORMAL;storyItems=gSaveContext.inventory.questItems;
            for(int item:{QUEST_KOKIRI_EMERALD,QUEST_GORON_RUBY,QUEST_ZORA_SAPPHIRE,QUEST_MEDALLION_FOREST,
                          QUEST_MEDALLION_FIRE,QUEST_MEDALLION_WATER,QUEST_MEDALLION_SHADOW,QUEST_MEDALLION_SPIRIT})storyItems|=1u<<item;
            SetAge();gSaveContext.entranceIndex=Entrance(visits[index].scene);gSaveContext.respawnFlag=0;
            for(int n=0;n<4;++n)SET_EVENTCHKINF(EVENTCHKINF_CARPENTERS_FREE(n));
            SET_EVENTCHKINF(EVENTCHKINF_TALON_RETURNED_FROM_CASTLE);
            gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);
            for(const char* key:{"gEnhancements.ARPG.Enabled","gEnhancements.ARPG.Quests","gEnhancements.ARPG.Enemies"})CVarSetInteger(key,1);
            for(const char* key:{"gEnhancements.ARPG.Packs","gEnhancements.ARPG.Affixes","gEnhancements.ARPG.RegionalWorld","gOpenWindows.ARPG","gOpenWindows.ARPGInventory","gEnhancements.LocalNpc.Enabled"})CVarSetInteger(key,0);
            phase=1;return;
        }
        const auto& v=visits[index];
        if(gPlayState && gPlayState->sceneNum==v.scene && GameInteractor::IsPlayerInControl()) {
            auto* player=GET_PLAYER(gPlayState);player->invincibilityTimer=40;gSaveContext.health=gSaveContext.healthCapacity;
            if(!captured) {
                captured=true;frames=0;entry=player->actor.world.pos;gARPGSave.campaign={};
                if(v.quest) {
                    auto& p=gARPGSave.campaign.quests[v.quest-1];p.state=uint8_t(Q::State::Active);gARPGSave.campaign.tracked=v.quest;
                    for(int o=0;o<v.objective;++o) {p.progress[o]=Q::Find(v.quest)->objectives[o].count;p.nodes[o]=7;}
                }
                for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);
            }
            if(v.contact>=0 && !contact) {
                auto original=player->actor.world.pos;
                for(int c=0;c<ACTORCAT_MAX && !contact;++c)for(auto* a=gPlayState->actorCtx.actorLists[c].head;a && !contact;a=a->next) {
                    if(!Q::HasQuests(a))continue;
                    for(int offset:{60,-60,100,-100}) {Vec3f p=a->world.pos;p.x+=offset;Move(p);if(Q::NearbyContact()==v.contact) {
                        contact=!v.beforeForest || Q::AcceptNearby(21);
                        if(v.quest && Q::Find(v.quest)->objectives[v.objective].kind==Q::Kind::Delivery) {
                            if(!Q::DeliverNearby(v.quest,v.objective) || Q::DeliverNearby(v.quest,v.objective))throw std::runtime_error("Native recipient delivery must record exactly once");
                        }
                        if(Q::AuthoredGreeting(a).empty() || Q::AuthoredReply(a,true).empty())throw std::runtime_error("Native contact lacks authored dialogue");
                        break;
                    }}
                }
                Move(original);
            }
            if(!v.quest) {if(frames<100)return;}
            else {
                const auto& o=Q::Find(v.quest)->objectives[v.objective];
                auto& p=gARPGSave.campaign.quests[v.quest-1];
                if(o.kind==Q::Kind::Song) {
                    auto& message=gPlayState->msgCtx;auto oldSong=message.lastPlayedSong;auto oldAction=message.ocarinaAction;
                    message.lastPlayedSong=o.detail;message.ocarinaAction=OCARINA_ACTION_TEACH_EPONA;
                    GameInteractor_ExecuteOnOcarinaSongAction();
                    if(p.progress[v.objective])throw std::runtime_error("Song lessons must not satisfy performance quests");
                    int item=o.detail==int(Q::Tune::Epona)?QUEST_SONG_EPONA:o.detail==int(Q::Tune::Saria)?QUEST_SONG_SARIA:QUEST_SONG_STORMS;
                    gSaveContext.inventory.questItems|=1u<<item;message.ocarinaAction=OCARINA_ACTION_FREE_PLAY_DONE;
                    GameInteractor_ExecuteOnOcarinaSongAction();GameInteractor_ExecuteOnOcarinaSongAction();
                    message.lastPlayedSong=oldSong;message.ocarinaAction=oldAction;
                    if(p.progress[v.objective]!=1)throw std::runtime_error("Native learned free-play song must count once");
                } else if(o.kind==Q::Kind::Vigil) {
                    if(frames<100)return;
                    Move(player->actor.world.pos);player->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
                    for(int n=0;n<50;++n)Q::Tick();
                    int before=p.progress[v.objective];auto paused=gPlayState->pauseCtx.state;gPlayState->pauseCtx.state=6;
                    for(int n=0;n<40;++n)Q::Tick();gPlayState->pauseCtx.state=paused;
                    if(p.progress[v.objective]!=before)throw std::runtime_error("Pause advanced native watch");
                    gSaveContext.health-=4;Q::Tick();gSaveContext.health=gSaveContext.healthCapacity;
                    if(p.progress[v.objective])throw std::runtime_error("Damage did not restart native watch");
                    for(int n=0;n<o.count*20+2;++n)Q::Tick();
                    if(p.progress[v.objective]!=o.count)throw std::runtime_error("Native watch did not finish");
                } else if(o.kind==Q::Kind::Delivery) {
                    if(!contact && frames<150)return;
                } else if(o.kind==Q::Kind::Pack) {
                    if(frames>100 && !packChecked) {
                        for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)if(a->update && Q::IsQuestEnemy(a)) {
                            a->colChkInfo.health=0;GameInteractor_ExecuteOnEnemyDefeat(a);Actor_Kill(a);
                        }
                        if(p.progress[v.objective]==o.count)packChecked=true;
                    }
                    if(!packChecked && frames<600)return;
                } else {
                    int placed=0;for(const auto& site:p.sites[v.objective])placed+=site.placed!=0;
                    if(placed<3 && frames<600) {
                        // Let gravity settle. Explore to an already validated site when the entrance
                        // itself is a narrow ledge; placement must still validate every new approach.
                        if(player->actor.bgCheckFlags&BGCHECKFLAG_GROUND)++standing;
                        if(standing==1)entry=player->actor.world.pos;
                        if(placed && frames%100==0) {
                            float farthest=0;Vec3f next=entry;
                            for(const auto& site:p.sites[v.objective])if(site.placed) {
                                float distance=std::hypot(site.x-entry.x,site.z-entry.z);
                                if(distance>farthest) {farthest=distance;next={site.x,site.y,site.z};}
                            }
                            entry=next;
                        }
                        if(standing>0)Move(entry);
                        return;
                    }
                    if(o.kind==Q::Kind::Survey && placed==3) {
                        for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head;a;a=a->next)Actor_Kill(a);
                        auto site=p.sites[v.objective][0];Move({site.x,site.y,site.z});player->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
                        if(o.detail!=uint8_t(Q::Period::Any)) {
                            auto night=gSaveContext.nightFlag;gSaveContext.nightFlag=!night;
                            for(int n=0;n<65;++n)Q::Tick();gSaveContext.nightFlag=night;
                            if(p.progress[v.objective])throw std::runtime_error("Survey accepted wrong time of day");
                        }
                        for(int n=0;n<3;++n) {
                            auto site=p.sites[v.objective][n];Move({site.x,site.y,site.z});player->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
                            for(int tick=0;tick<65;++tick)Q::Tick();
                        }
                        if(p.progress[v.objective]!=3)throw std::runtime_error("Three native observation channels did not finish");
                    }
                }
            }
            if(v.contact>=0 && !contact && frames<150)return;
            bool passed=bool(LINK_IS_ADULT)==v.adult && (v.contact<0 || contact);
            int placed=0;bool pack=v.quest && Q::Find(v.quest)->objectives[v.objective].kind==Q::Kind::Pack;
            if(v.quest) {for(const auto& s:gARPGSave.campaign.quests[v.quest-1].sites[v.objective])placed+=s.placed!=0;const auto& o=Q::Find(v.quest)->objectives[v.objective];
                passed&=pack?packChecked:Q::Physical(o)?placed==3:gARPGSave.campaign.quests[v.quest-1].progress[v.objective]==o.count;}
            nlohmann::json actors=nlohmann::json::array();
            if(v.contact>=0 && !contact)for(int c=0;c<ACTORCAT_MAX;++c)for(auto* a=gPlayState->actorCtx.actorLists[c].head;a;a=a->next)
                if(c==ACTORCAT_NPC)actors.push_back({{"id",a->id},{"draw",a->draw!=nullptr},{"update",a->update!=nullptr},{"room",a->room}});
            results.push_back({{"scene",v.scene},{"adult",v.adult},{"night",v.night},{"quest",v.quest},{"objective",v.objective},{"contact",contact},
                {"placed",placed},{"packDefeated",packChecked},{"passed",passed},{"entry",{entry.x,entry.y,entry.z}},
                {"storyItems",gSaveContext.inventory.questItems},{"npcs",actors}});
            SPDLOG_INFO("[Quest world QA] scene {} quest {} placed {} contact {} passed {}",v.scene,v.quest,placed,contact,passed);Report("running");
            if(++index==int(std::size(visits))) {bool all=true;for(const auto& r:results)all&=r["passed"].get<bool>();Report(all?"passed":"failed");phase=99;return;}
            SetAge();gPlayState->nextEntranceIndex=Entrance(visits[index].scene);gPlayState->transitionTrigger=TRANS_TRIGGER_START;gPlayState->transitionType=TRANS_TYPE_INSTANT;
        }
        if(frames>1400)throw std::runtime_error("Timed out at visit "+std::to_string(index));
    }catch(const std::exception& e){Report("failed",e.what());phase=99;}
}
void Register() {
    const char* enabled=std::getenv("SOH_ARPG_QUEST_SWEEP"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home || !std::filesystem::path(home).filename().string().starts_with("soh-arpg-qa-"))return;
    if(const char* start=std::getenv("SOH_ARPG_QUEST_START"))index=std::clamp(std::atoi(start),0,int(std::size(visits))-1);
    output=std::filesystem::path(home)/"quest-world-results.json";GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
}
static RegisterShipInitFunc init(Register);
}
