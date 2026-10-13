#include "../../soh/soh/Network/Anchor/FellowshipModel.h"
#include <cassert>
#include <iostream>
using nlohmann::json;
int main() {
    Fellowship::Model m;
    json caps={{"type","SERVER_CAPABILITIES"},{"socialProtocol",1}};
    json message={{"id",1},{"clientId",7},{"name","Link"},{"text","Let's go."},{"channel","party"},{"requestId",9}};
    json chat={{"type","SOCIAL"},{"version",1},{"op","chat"},{"message",message}};
    assert(!m.Receive(chat,0));assert(m.Receive(caps,0));assert(m.supported);
    assert(m.Receive(chat,0));assert(m.Receive(chat,1));assert(m.history.size()==1);
    json state={{"type","SOCIAL"},{"version",1},{"op","state"},{"leader",7},{"roomId","Kokiri"},
        {"history",json::array({message})},{"check",{{"id",2},{"members",{{"7","ready"},{"9","pending"}}}}},
        {"remainingMs",60000},{"active",true}};
    assert(m.Receive(state,10));assert(m.CheckActive(69));assert(!m.CheckActive(70));
    auto bad=state;bad["check"]["members"]["9"]="injected";
    assert(!m.Receive(bad,0));assert(m.deadline==70 && m.members[9]=="pending");
    bad=state;bad["history"][0]["text"]=std::string(481,'x');assert(!m.Receive(bad,0));
    bad=state;bad["remainingMs"]=60001;assert(!m.Receive(bad,0));
    bad=state;bad["history"]="bad";assert(!m.Receive(bad,0));
    bad=state;bad["check"]["members"]={{"9evil","ready"}};assert(!m.Receive(bad,0));
    for(int i=2;i<150;++i) {chat["message"]["id"]=i;assert(m.Receive(chat,0));}
    assert(m.history.size()==100 && m.history.front().id==50);
    state.erase("check");state["history"]=json::array();assert(m.Receive(state,0));assert(m.members.empty() && m.checkId==0 && m.history.empty());
    for(auto malformed:{json(nullptr),json::array(),json({{"type",2}})}) assert(!m.Receive(malformed,0));
    std::cout<<"Fellowship protocol, history, expiry and malformed-input checks passed\n";
}
