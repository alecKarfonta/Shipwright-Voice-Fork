#pragma once
#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <nlohmann/json.hpp>

namespace Fellowship {
struct Message {
    uint64_t id = 0, requestId = 0;
    uint32_t clientId = 0;
    std::string name, text, channel;
};
struct Model {
    bool supported = false, active = false;
    uint32_t leader = 0;
    uint64_t checkId = 0;
    double deadline = 0;
    std::string room, error;
    std::map<uint32_t, std::string> members;
    std::deque<Message> history;
    static Message ParseMessage(const nlohmann::json& j) {
        Message m{j.at("id").get<uint64_t>(), j.value("requestId",uint64_t(0)),j.at("clientId").get<uint32_t>(),
                  j.at("name").get<std::string>(),j.at("text").get<std::string>(),j.at("channel").get<std::string>()};
        if (!m.id || m.name.size()>48 || m.text.size()>480 || (m.channel!="party" && m.channel!="room"))
            throw std::runtime_error("Invalid chat message");
        return m;
    }
    bool Receive(const nlohmann::json& p, double now) {
        try {
            if (p.at("type")=="SERVER_CAPABILITIES") { supported=p.value("socialProtocol",0)==1; return true; }
            if (!supported || p.at("type")!="SOCIAL" || p.value("version",0)!=1) return false;
            const auto op=p.at("op").get<std::string>();
            if (op=="error") { error=p.at("message").get<std::string>().substr(0,240); return true; }
            if (op=="chat") {
                auto m=ParseMessage(p.at("message"));
                if (std::none_of(history.begin(),history.end(),[&](auto& old){return old.id==m.id;})) history.push_back(std::move(m));
                while(history.size()>100) history.pop_front();
                return true;
            }
            if (op!="state") return false;
            Model next=*this;
            next.leader=p.at("leader").get<uint32_t>();
            next.room=p.at("roomId").get<std::string>();
            if (next.room.size()>64 || !p.at("history").is_array() || p.at("history").size()>100) return false;
            next.history.clear();
            for (auto& item:p.at("history")) next.history.push_back(ParseMessage(item));
            next.checkId=0;next.active=false;next.deadline=0;next.members.clear();
            if (p.contains("check")) {
                auto& check=p.at("check");
                next.checkId=check.at("id").get<uint64_t>();
                const auto remaining=p.at("remainingMs").get<int64_t>();
                if (remaining<0 || remaining>60000 || !check.at("members").is_object() || check.at("members").size()>8) return false;
                next.deadline=now+remaining/1000.0;
                next.active=p.at("active").get<bool>();
                for(auto it=check.at("members").begin();it!=check.at("members").end();++it) {
                    const auto status=it.value().get<std::string>();
                    if(status!="ready" && status!="away" && status!="pending" && status!="departed") return false;
                    size_t end=0;auto id=std::stoull(it.key(),&end);
                    if(end!=it.key().size() || id>UINT32_MAX) return false;
                    next.members[uint32_t(id)]=status;
                }
            }
            *this=std::move(next);
            return true;
        } catch (const std::exception&) { return false; }
    }
    bool CheckActive(double now) const {return active && now<deadline;}
};
}
