#include "Fellowship.h"
#include "FellowshipModel.h"
#include "Anchor.h"
#include "ServerBrowserWindow.h"
#include "soh/Enhancements/arpg/ARPGTheme.h"
#include "soh/Enhancements/arpg/InventorySystem.h"
#include "soh/Notification/Notification.h"
#include "soh/util.h"
#include <libultraship/libultraship.h>
#include <cmath>
#include <set>

namespace SohGui { extern std::shared_ptr<ServerBrowserWindow> mServerBrowserWindow; }
namespace Fellowship {
namespace {
namespace Theme = ARPG::Theme;
Model model;
std::set<uint32_t> muted;
char draft[481] = {};
int channel = 0, compactTab = 0;
uint32_t selected = 0, promote = 0;
uint64_t requestId = 0, pendingMessage = 0, notifiedCheck = 0;
double refreshAt = 0, pendingAt = 0;
bool scrollChat = false, memberPage = false;
constexpr ImVec4 Gold{.92f,.80f,.51f,1}, Green{.56f,.88f,.70f,1}, Muted{.63f,.71f,.78f,1};
void Send(const char* op, nlohmann::json data=nlohmann::json::object()) {
    if (!Anchor::Instance->isConnected || !model.supported) return;
    data["type"]="SOCIAL";data["version"]=1;data["op"]=op;
    Anchor::Instance->SendJsonToRemote(std::move(data));
}
void Browser() {
    ARPG::Inventory::Close();
    if (SohGui::mServerBrowserWindow) SohGui::mServerBrowserWindow->Show();
}
void MemberActions() {
    auto* anchor=Anchor::Instance;
    if(selected && anchor->clients.contains(selected) && selected!=anchor->ownClientId) {
        if (Theme::Button(muted.contains(selected)?"Unmute member chat":"Mute member chat",{-1,32})) {
            if(muted.contains(selected)) muted.erase(selected); else muted.insert(selected);
        }
        ImGui::BeginDisabled(!anchor->CanTeleportTo(selected));
        if(Theme::Button("Travel to member",{-1,34})) anchor->SendPacket_RequestTeleport(selected);
        ImGui::EndDisabled();
        if(anchor->roomState.ownerClientId==anchor->ownClientId && model.supported) {
            if(promote==selected) {
                Theme::Wrapped("Pass room leadership to %s?",anchor->clients[selected].name.c_str());
                if(Theme::Button("Confirm leadership",{-1,32})) {Send("promote",{{"targetClientId",selected}});promote=0;}
                if(Theme::Button("Keep leadership",{-1,32})) promote=0;
            } else if(Theme::Button("Pass leadership",{-1,32})) promote=selected;
        }
    }
}
void Party() {
    auto* anchor=Anchor::Instance;
    const bool compact=ImGui::GetContentRegionAvail().y<360;
    if(compact && memberPage && anchor->clients.contains(selected)) {
        if(Theme::Button("Back to party",{-1,30})) memberPage=false;
        Theme::Colored(Gold,"%s",anchor->clients[selected].name.c_str());
        MemberActions();
        return;
    }
    if(!compact) Theme::TitleBar("YOUR FELLOWSHIP");
    Theme::Muted("%zu adventurers / %s",anchor->clients.size(),CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"),"default"));
    const float rosterHeight=std::max(48.f,ImGui::GetContentRegionAvail().y-(compact?(model.CheckActive(ImGui::GetTime())?106.f:77.f):190.f));
    ImGui::BeginChild("fellowshipRoster",{0,rosterHeight});
    for (auto& [id,client]:anchor->clients) {
        if (!client.online) continue;
        ImGui::PushID(int(id));
        const auto p=ImGui::GetCursorScreenPos();
        const auto width=ImGui::GetContentRegionAvail().x;
        if (ImGui::Selectable("##member",selected==id,0,{width,compact?52.f:66.f})) {selected=id;promote=0;if(compact && id!=anchor->ownClientId) memberPage=true;}
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled({p.x+1,p.y+3},{p.x+4,p.y+(compact?49.f:59.f)},id==model.leader?IM_COL32(231,194,114,255):IM_COL32(80,146,166,255),2);
        std::string name=client.name+(client.self?" (you)":"");
        const float maxWidth=std::max(30.f,width-26);
        while(!name.empty() && Theme::TextWidth(name,19)>maxWidth) {
            size_t end=name.size()-1;while(end>0 && (static_cast<unsigned char>(name[end])&0xc0)==0x80)--end;
            name.erase(end);
        }
        Theme::Text(draw,{p.x+15,p.y+3},name,19,IM_COL32(243,235,210,255));
        const bool sameTeam=client.teamId==CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"),"default");
        std::string detail=client.isSaveLoaded?"Adventuring":"Choosing a character";
        if(client.isSaveLoaded && (anchor->roomState.showLocationsMode==2 || (anchor->roomState.showLocationsMode==1 && sameTeam)))
            detail=SohUtils::GetSceneName(client.sceneNum);
        if (id==model.leader) detail="Leader / "+detail;
        if (!sameTeam) detail="Other party / "+detail;
        const auto status=model.members.find(id);
        if(status!=model.members.end()) detail+=(status->second=="ready"?" / Ready":status->second=="away"?" / Not ready":status->second=="pending"?" / Waiting":" / Left");
        draw->PushClipRect({p.x+15,p.y},{p.x+width-4,p.y+64},true);
        Theme::Text(draw,{p.x+15,p.y+(compact?27.f:32.f)},detail,15,IM_COL32(169,190,204,255));
        draw->PopClipRect();
        ImGui::PopID();
    }
    ImGui::EndChild();
    Theme::Rule();
    if(model.checkId) {
        const auto ready=std::count_if(model.members.begin(),model.members.end(),[](auto& m){return m.second=="ready";});
        Theme::Colored(Green,"%d / %zu ready%s",int(ready),model.members.size(),model.CheckActive(ImGui::GetTime())?" / Checking":ready==int(model.members.size())?" / All ready":" / Check ended");
        if(model.CheckActive(ImGui::GetTime())) {
            Theme::Muted("%d seconds remaining",int(std::ceil(model.deadline-ImGui::GetTime())));
            if(model.members.contains(anchor->ownClientId)) {
                float half=(ImGui::GetContentRegionAvail().x-8)/2;
                if(Theme::Button("Ready",{half,34},model.members[anchor->ownClientId]=="ready")) Send("ready",{{"checkId",model.checkId},{"ready",true}});
                ImGui::SameLine(0,8);
                if(Theme::Button("Not ready",{half,34},model.members[anchor->ownClientId]=="away")) Send("ready",{{"checkId",model.checkId},{"ready",false}});
            }
        }
    }
    if(!compact || !model.CheckActive(ImGui::GetTime())) {
    ImGui::BeginDisabled(!model.supported || model.leader!=anchor->ownClientId || model.CheckActive(ImGui::GetTime()));
    if(Theme::Button("Start ready check",{-1,34})) Send("ready_check");
    ImGui::EndDisabled();
    }
    if(!compact) MemberActions();
}
void Chat() {
    const bool compact=ImGui::GetContentRegionAvail().y<360;
    if(!compact) Theme::TitleBar("CAMPFIRE CHAT");
    float half=(ImGui::GetContentRegionAvail().x-8)/2;
    if(Theme::Button("Party",{half,32},channel==0)) {channel=0;scrollChat=true;}
    ImGui::SameLine(0,8);
    if(Theme::Button("Room",{half,32},channel==1)) {channel=1;scrollChat=true;}
    const char* scope=channel==0?"party":"room";
    ImGui::BeginChild("chatHistory",{0,std::max(40.f,ImGui::GetContentRegionAvail().y-(compact?43.f:103.f))});
    bool atBottom=ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-20;
    bool any=false;
    for(auto& m:model.history) {
        if(m.channel!=scope || muted.contains(m.clientId)) continue;
        any=true;
        Theme::Colored(m.clientId==Anchor::Instance->ownClientId?Gold:Green,"%s",m.name.c_str());
        Theme::Wrapped("%s",m.text.c_str());
        ImGui::Spacing();
    }
    if(!any) {
        ImGui::Spacing();Theme::Colored(Gold,"Every adventure starts with a hello.");
        Theme::Wrapped(channel==0?"Plan your next quest with your party.":"Meet the adventurers sharing this room.");
    }
    if(scrollChat && atBottom) ImGui::SetScrollHereY(1.f);
    scrollChat=false;
    ImGui::EndChild();
    ImGui::BeginDisabled(!model.supported || pendingMessage!=0);
    ImGui::SetNextItemWidth(compact?std::max(60.f,ImGui::GetContentRegionAvail().x-86):-1);
    bool submit=ImGui::InputTextWithHint("##fellowshipMessage",channel==0?"Message your party...":"Message this room...",draft,sizeof(draft),ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::BeginDisabled(draft[0]=='\0');
    if(compact) ImGui::SameLine(0,8);
    bool send=Theme::Button(pendingMessage?"Sending...":compact?"Send":"Send message",{compact?78.f:-1.f,34});
    ImGui::EndDisabled();
    if((submit||send) && draft[0] && !pendingMessage && model.supported) {
        model.error.clear();pendingMessage=++requestId;pendingAt=ImGui::GetTime();
        Send("chat",{{"channel",scope},{"text",draft},{"requestId",pendingMessage}});
    }
    ImGui::EndDisabled();
    if(!compact) Theme::Muted(channel==0?"Visible to your party / Enter to send":"Visible to everyone in this room / Enter to send");
}
}
void Reset() {
    model={};muted.clear();memberPage=false;selected=promote=0;pendingMessage=notifiedCheck=0;refreshAt=0;
    // Retain an unsent draft for retry, but never carry another room's history.
}
void Receive(const nlohmann::json& packet) {
    const bool supported=model.supported;
    if(!model.Receive(packet,ImGui::GetTime())) return;
    if(!supported && model.supported) Send("hello");
    if(packet.value("op","")=="error") pendingMessage=0;
    for(auto& m:model.history) {
        if(pendingMessage && m.clientId==Anchor::Instance->ownClientId && m.requestId==pendingMessage) {
            pendingMessage=0;draft[0]='\0';scrollChat=true;
        }
    }
    if(packet.value("op","")=="chat") scrollChat=true;
    if(model.checkId && model.checkId!=notifiedCheck) {
        notifiedCheck=model.checkId;
        if(model.CheckActive(ImGui::GetTime()) && model.members.contains(Anchor::Instance->ownClientId))
            Notification::Emit({.prefix="Fellowship",.message="Ready check started. Open Fellowship in your journal to respond."});
    }
}
void DrawJournal() {
    auto* anchor=Anchor::Instance;
    if(!anchor->isConnected || !anchor->ownClientId) {
        Theme::BeginPane("fellowshipOffline",{0,0},true);
        Theme::TitleBar("ADVENTURE TOGETHER");
        Theme::Label(anchor->isEnabled?"Finding your fellowship...":"Hyrule is better with company.",23);
        Theme::Wrapped(anchor->isEnabled?"Connecting to your party. Your character stays with you.":"Find a public game or invite friends to a private adventure. Your gear, skills and story travel with your character.");
        ImGui::Spacing();
        if(Theme::Button("Public games & connection",{-1,42})) Browser();
        Theme::EndPane();return;
    }
    if(model.supported && ImGui::GetTime()>=refreshAt) {Send("hello");refreshAt=ImGui::GetTime()+3;}
    if(pendingMessage && ImGui::GetTime()-pendingAt>8) {pendingMessage=0;model.error="Message not confirmed. Your draft is saved; check chat before retrying.";}
    Theme::Colored(Green,"CONNECTED  /  %s",CVarGetString(CVAR_REMOTE_ANCHOR("RoomId"),""));
    if(!model.supported) Theme::Wrapped("This relay supports classic co-op. Chat and ready checks become available on an updated relay.");
    if(!model.error.empty()) Theme::Wrapped("%s",model.error.c_str());
    bool wide=ImGui::GetContentRegionAvail().x>=820;
    if(!wide) {
        float half=(ImGui::GetContentRegionAvail().x-8)/2;
        if(Theme::Button("Party",{half,34},compactTab==0))compactTab=0;
        ImGui::SameLine(0,8);
        if(Theme::Button("Chat",{half,34},compactTab==1))compactTab=1;
    }
    float width=ImGui::GetContentRegionAvail().x;
    float height=std::max(140.f,ImGui::GetContentRegionAvail().y-48);
    if(wide || compactTab==0) {
        Theme::BeginPane("fellowshipParty",{wide?(width-12)*.46f:width,height},true);Party();Theme::EndPane();
    }
    if(wide) ImGui::SameLine(0,12);
    if(wide || compactTab==1) {
        Theme::BeginPane("fellowshipChat",{wide?(width-12)*.54f:width,height},true);Chat();Theme::EndPane();
    }
    if(Theme::Button("Public games & connection",{-1,34})) Browser();
}
}
