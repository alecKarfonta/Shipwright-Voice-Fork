#include "soh/Enhancements/local-npc/NpcDialogue.h"
#include "soh/Enhancements/arpg/QuestSystem.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/ARPGSaveCodec.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include <libultraship/libultraship.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <spdlog/spdlog.h>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;
extern GameState* gGameState;
void Sram_InitDebugSave(void);
}
namespace {
using namespace ARPG::Quests;
int phase=0,frames=0,totalFrames=0;
Actor* contact=nullptr;
bool nativeChoice=false,nativeBranch=false,advanceReply=false;
std::filesystem::path output;
nlohmann::json checks=nlohmann::json::array();
void Report(const char* status,const std::string& error="") {
    nlohmann::json result={{"status",status},{"phase",phase},{"checks",checks},{"error",error}};
    if(gPlayState)result["message"]={{"id",gPlayState->msgCtx.textId},{"mode",gPlayState->msgCtx.msgMode},
        {"length",gPlayState->msgCtx.decodedTextLen},{"drawPosition",gPlayState->msgCtx.textDrawPos}};
    std::ofstream(output)<<result.dump(2);
}
void Check(bool ok,const char* text) {
    if(!ok)throw std::runtime_error(text);
    checks.push_back(text);SPDLOG_INFO("[Dialogue QA] PASS {}",text);Report("running");
}
void Next(int next) {phase=next;frames=0;Report("running");}
void Choose(int choice) {
    auto& message=gPlayState->msgCtx;
    message.choiceIndex=choice;
    gPlayState->state.input[0].press.button=BTN_A;
    gPlayState->state.input[0].cur.button=BTN_A;
    GameInteractor_ExecuteOnDialogMessage();
}
bool Menu() {
    auto& m=gPlayState->msgCtx;
    return LocalNpc::IsOpen() && m.msgMode==MSGMODE_TEXT_DONE && m.choiceNum>0;
}
void Input() {
    if(!gPlayState || phase<2 || phase==99 || phase==98)return;
    auto& m=gPlayState->msgCtx;
    if(m.msgMode==MSGMODE_TEXT_DISPLAYING)m.textDrawPos=m.decodedTextLen+1;
    if((phase==2 && !LocalNpc::IsOpen()) || advanceReply) {
        if(m.msgMode==MSGMODE_TEXT_AWAIT_NEXT ||
           (m.msgMode==MSGMODE_TEXT_DONE && (!LocalNpc::IsOpen() || m.choiceNum==0))) {
            if(phase==2 && m.textId==0x1035) {m.choiceIndex=1;nativeChoice=true;}
            gPlayState->state.input[0].press.button|=BTN_A;
            gPlayState->state.input[0].cur.button|=BTN_A;
        }
    }
}
void Tick() {
    if(phase==99 || phase==98)return;
    try {
        ++frames;++totalFrames;
        if(totalFrames%100==0)Report("running");
        if(totalFrames>3000)throw std::runtime_error("Timeout in phase "+std::to_string(phase));
        if(phase==0 && frames>60 && gGameState) {
            CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);
            gSaveContext.fileNum=0xFE;Sram_InitDebugSave();gSaveContext.fileNum=0xFF;
            gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.linkAge=LINK_AGE_CHILD;
            gSaveContext.inventory.questItems=0;gSaveContext.entranceIndex=ENTR_KOKIRI_FOREST_0;
            gSaveContext.cutsceneIndex=0;gSaveContext.sceneLayer=0;gSaveContext.respawnFlag=0;
            gSaveContext.dayTime=gSaveContext.skyboxTime=0x8000;
            for(const char* key:{"ARPG.Enabled","ARPG.Items","ARPG.Quests","LocalNpc.Enabled"})
                CVarSetInteger((std::string("gEnhancements.")+key).c_str(),1);
            for(const char* key:{"ARPG.Enemies","ARPG.Packs","ARPG.Affixes","ARPG.RegionalWorld","LocalNpc.Voices"})
                CVarSetInteger((std::string("gEnhancements.")+key).c_str(),0);
            CVarSetInteger("gOpenWindows.ARPG",0);CVarSetInteger("gOpenWindows.ARPGInventory",0);
            gGameState->running=false;SET_NEXT_GAMESTATE(gGameState,Play_Init,PlayState);Next(1);return;
        }
        if(!gPlayState)return;
        auto* player=GET_PLAYER(gPlayState);
        if(!player)return;
        auto& message=gPlayState->msgCtx;
        if(phase==1 && frames>100 && GameInteractor::IsPlayerInControl()) {
            for(auto* a=gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head;a;a=a->next)
                if(a->id==ACTOR_EN_KO && a->update && a->draw) {contact=a;break;}
            Check(contact!=nullptr,"Native Kokiri contact exists");
            auto position=contact->world.pos;position.x+=60;
            player->actor.world.pos=player->actor.prevPos=position;player->actor.velocity={};player->actor.speedXZ=0;
            player->talkActor=contact;player->stateFlags2|=PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER;
            contact->textId=0x1035; // Original Kokiri two-choice conversation.
            gARPGSave.campaign={};for(auto& item:gARPGSave.backpack)item={};
            Check(LocalNpc::TalkTo(contact),"F7 entry accepts a native offered conversation");
            Check(!LocalNpc::IsOpen(),"Native campaign owns the interaction before custom topics");
            Next(2);return;
        }
        if(phase==2) {
            if(message.textId==0x1037)nativeBranch=true;
            if(LocalNpc::IsOpen()) {
                Check(nativeChoice && nativeBranch,"Native two-choice actor script preserves its selected continuation");
                Check(message.textId==0xF700 && !message.talkActor,"Campaign hands off to one native custom textbox without an actor event target");
                Check(player->csAction==PLAYER_CSACTION_8,"Custom conversation holds the player in the native conversation pose");
                Next(3);
            }
        } else if(phase==3 && Menu()) {
            Check(gSaveContext.hudVisibilityMode==HUD_VISIBILITY_A_HEARTS_MAGIC_FORCE,"Conversation HUD keeps the minimap clear of native dialogue");
            Choose(1);Next(4);
        }
        else if(phase==4 && Menu()) {Choose(0);advanceReply=true;Next(5);}
        else if(phase==5 && Menu()) {advanceReply=false;Choose(1);Next(6);}
        else if(phase==6 && frames>5) {
            Check(Status(gARPGSave.campaign,1)==State::Active,"Native quest choice accepts the local mission");
            const auto before=ARPG::SaveCodec::Encode(gARPGSave);
            ChooseConversationOption(contact,{"I will help",TalkAction::Accept,1});
            Check(before==ARPG::SaveCodec::Encode(gARPGSave),"A stale accept choice cannot repeat its transaction");
            // Native enemy callbacks are covered by quest_runtime_test. Use
            // the production ledger to prepare the native reward interaction.
            for(int i=0;i<4;++i)RecordKill(gARPGSave.campaign,gPlayState->sceneNum,Foe::Plant);
            for(auto& item:gARPGSave.backpack)item=RewardItem(Catalog[0],0,false);
            auto full=ARPG::SaveCodec::Encode(gARPGSave);auto rupees=gSaveContext.rupees;
            ChooseConversationOption(contact,{"Reward",TalkAction::Reward,1,0});
            Check(full==ARPG::SaveCodec::Encode(gARPGSave) && rupees==gSaveContext.rupees,
                "A full backpack preserves the reward, XP and currency until space is available");
            for(auto& item:gARPGSave.backpack)item={};
            advanceReply=true;Next(7);
        } else if(phase==7 && Menu()) {advanceReply=false;Choose(1);Next(8);}
        else if(phase==8 && frames>5) {
            Check(Status(gARPGSave.campaign,1)==State::Rewarded,"Native reward choice grants the selected gear and completes the quest");
            auto before=ARPG::SaveCodec::Encode(gARPGSave);auto rupees=gSaveContext.rupees;
            ChooseConversationOption(contact,{"Reward",TalkAction::Reward,1,0});
            Check(before==ARPG::SaveCodec::Encode(gARPGSave) && rupees==gSaveContext.rupees,"Repeated reward selection cannot duplicate inventory, XP or currency");
            LocalNpc::EndConversation();advanceReply=false;Next(9);
        } else if(phase==9 && frames>10 && GameInteractor::IsPlayerInControl()) {
            Check(!LocalNpc::IsOpen() && message.msgMode==MSGMODE_NONE,"Leaving closes the native textbox and returns player control");
            // An unrelated event and its choice index must survive an attempted open.
            Message_StartTextbox(gPlayState,0x1035,contact);message.choiceIndex=1;
            Check(!LocalNpc::OpenConversation(contact) && message.textId==0x1035 && message.choiceIndex==1,
                "An active campaign message cannot be replaced by custom topics");
            Message_CloseTextbox(gPlayState);Next(10);
        } else if(phase==10 && frames>15 && GameInteractor::IsPlayerInControl()) {
            CVarSetInteger("gEnhancements.ARPG.Enabled",0);
            Check(LocalNpc::OpenConversation(contact),"Unified conversations work with ARPG gameplay disabled");
            Next(11);
        } else if(phase==11 && Menu()) {
            // Actor lifetime invalidates every request and releases control.
            GameInteractor_ExecuteOnActorKill(contact);
            Check(!LocalNpc::IsOpen(),"Actor destruction closes custom dialogue and invalidates its session");
            Next(12);
        } else if(phase==12 && frames>15 && GameInteractor::IsPlayerInControl()) {
            Check(LocalNpc::OpenConversation(contact),"Conversation can reopen after cancellation");
            CVarSetInteger("gEnhancements.LocalNpc.Enabled",0);Next(13);
        } else if(phase==13 && frames>15) {
            Check(!LocalNpc::IsOpen() && GameInteractor::IsPlayerInControl(),"Disabling dialogue releases the textbox and native player lock");
            CVarSetInteger("gEnhancements.LocalNpc.Enabled",1);
            CVarSetInteger("gEnhancements.ARPG.Enabled",1);
            LocalNpc::OpenConversation(contact);
            Next(98);Report("passed");
        }
    } catch(const std::exception& e) {Report("failed",e.what());phase=99;SPDLOG_ERROR("[Dialogue QA] {}",e.what());}
}
void Register() {
    const char* enabled=std::getenv("SOH_DIALOGUE_QA"),*home=std::getenv("SHIP_HOME");
    if(!enabled || std::string(enabled)!="1" || !home ||
       !std::filesystem::path(home).filename().string().starts_with("soh-dialogue-qa-"))return;
    output=std::filesystem::path(home)/"dialogue-results.json";
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(Input);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Tick);
}
static RegisterShipInitFunc init(Register);
}
