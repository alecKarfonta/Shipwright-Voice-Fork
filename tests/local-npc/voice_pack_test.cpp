#include "VoicePack.h"
#include <cassert>
#include <iostream>
using namespace LocalNpc;
using json=nlohmann::json;
int main(int argc,char** argv) {
    if(argc==2) {
        VoicePack pack;assert(pack.Load(argv[1]));
        std::ifstream input(std::filesystem::path(argv[1])/"manifest.json");auto m=json::parse(input);
        size_t misses=0,bytes=0;
        for(const auto& e:m["entries"]) {
            PackQuery q;q.authored=true;q.quest=e["scope"]=="authored_quest";q.character=e["character"];q.age=e["age"];q.messageId=e.value("message_id","");q.reaction=e.value("reaction","");
            auto selection=pack.Select(q,e["text"]);
            if(selection.empty()) {if(misses++<10)std::cerr<<"Miss "<<e["id"]<<" "<<e["text"]<<"\n";continue;}
            auto audio=pack.Audio(selection);assert(audio && audio->size()>44);bytes+=audio->size();
            const auto native=std::regex_replace(e.at("match_text").get<std::string>(),std::regex("\\{[^}]+\\}"),"12");
            if(pack.Select(q,native).empty()) {
                if(misses++<10)std::cerr<<"Native surface miss "<<e["id"]<<" "<<native<<"\n";
            }
        }
        std::cout<<"Pack integration: "<<m["entries"].size()<<" entries, "<<misses<<" misses, "<<bytes<<" decoded bytes\n";
        return misses?1:0;
    }
    const auto root=std::filesystem::temp_directory_path()/"soh-voice-pack-test";
    std::filesystem::create_directories(root/"audio");
    auto entry=[](const std::string& who,const std::string& text,const std::string& file,const std::string& kind="speech") {
        return json{{"character",who},{"text",text},{"match_text",text},{"file",file},{"message_id","1000"},{"bytes",48},{"kind",kind},{"scope","authored_message"}};
    };
    json rows=json::array({entry("saria","Welcome, Link!","audio/saria.wav"),entry("mido","Welcome, Link!","audio/mido.wav"),entry("narrator","Yes","audio/yes.wav","choice")});
    auto numeric=entry("saria","You've caught a big fish!","audio/fish.wav");numeric["template"]="You've caught a {FISH_INFO} pound fish!";rows.push_back(numeric);
    auto quest=entry("saria","The forest needs your help today.","audio/quest.wav");quest["scope"]="authored_quest";quest["age"]="child";rows.push_back(quest);
    auto reaction=entry("market-guard","Stop!","audio/guard.wav");
    reaction["scope"]="town_reaction";reaction["reaction"]="guard.warning";rows.push_back(reaction);
    auto write=[&] {std::ofstream(root/"manifest.json")<<json{{"format","soh-voice-pack-v1"},{"complete",true},{"language","en"},{"entries",rows}};};write();
    VoicePack pack;assert(pack.Load(root.string()));PackQuery q;q.authored=true;q.messageId="1000";q.character="saria";
    assert(pack.Select(q,"Welcome, Link!")==std::vector<std::string>{"audio/saria.wav"});
    q.character="mido";assert(pack.Select(q,"Welcome Link")==std::vector<std::string>{"audio/mido.wav"});
    q.character="";assert(pack.Select(q,"Welcome Link").empty());
    q.character="saria";q.playerName="Alec";assert(pack.Select(q,"Welcome, Alec!").size()==1);
    assert(pack.Select(q,"Welcome Link! Yes").size()==2);
    assert(pack.Select(q,"Welcome Ganon").empty());
    assert(pack.Select(q,"You've caught a 12 pound fish!")==std::vector<std::string>{"audio/fish.wav"});
    assert(pack.Select(q,"You've caught a red pound fish!").empty());
    q.quest=true;q.age="adult";assert(pack.Select(q,"The forest needs your help today.").empty());
    q.age="child";assert(pack.Select(q,"A forest task. The forest needs your help today. Rewards: 12 XP.").size()==1);
    q.authored=false;assert(pack.Select(q,"The forest needs your help today.").empty());
    q.authored=true;q.quest=false;q.reaction="guard.warning";q.character="market-guard";
    assert(pack.Select(q,"Stop!")==std::vector<std::string>{"audio/guard.wav"});
    q.character="hylian-resident-00";assert(pack.Select(q,"Stop!").empty());
    q.character="market-guard";q.reaction="guard.alarm";assert(pack.Select(q,"Stop!").empty());
    q.reaction="guard.warning";assert(pack.Select(q,"Please stop!").empty());
    q.reaction.clear();assert(pack.Select(q,"Stop!").empty());
    const unsigned char wav[]={82,73,70,70,40,0,0,0,87,65,86,69,102,109,116,32,16,0,0,0,1,0,1,0,192,93,0,0,128,187,0,0,2,0,16,0,100,97,116,97,4,0,0,0,1,0,2,0};
    std::ofstream audio(root/"audio/saria.wav",std::ios::binary);audio.write(reinterpret_cast<const char*>(wav),sizeof(wav));audio.close();
    assert(pack.Audio({"audio/saria.wav"})->size()==48);
    assert(pack.Audio({"audio/saria.wav","audio/saria.wav"})->size()==5812);
    // Skip padding without losing a quiet opening syllable or an internal pause.
    std::vector<uint8_t> padded(wav,wav+44);
    auto samples=[&](size_t count,int16_t amplitude) {
        for(size_t i=0;i<count;++i) {
            uint16_t sample=static_cast<uint16_t>(i%2?amplitude:-amplitude);
            padded.push_back(sample&255);padded.push_back(sample>>8);
        }
    };
    samples(14400,0);samples(480,48);samples(4800,1024);samples(7200,0);samples(4800,1024);
    auto size32=[&](size_t pos,uint32_t n) {for(size_t i=0;i<4;++i)padded[pos+i]=(n>>(8*i))&255;};
    size32(4,padded.size()-8);size32(40,padded.size()-44);
    {std::ofstream f(root/"audio/padded.wav",std::ios::binary);f.write(reinterpret_cast<const char*>(padded.data()),padded.size());}
    auto trimmed=pack.Audio({"audio/padded.wav"});constexpr size_t skipped=(14400-1920)*2;
    assert(trimmed && trimmed->size()==padded.size()-skipped);
    assert(std::equal(trimmed->begin()+44,trimmed->end(),padded.begin()+44+skipped));
    // Entirely quiet audio retains a valid WAV; already prompt speech stays untouched.
    std::fill(padded.begin()+44,padded.end(),0);
    {std::ofstream f(root/"audio/quiet.wav",std::ios::binary);f.write(reinterpret_cast<const char*>(padded.data()),padded.size());}
    assert(*pack.Audio({"audio/quiet.wav"})==padded);
    {std::ofstream f(root/"audio/prompt.wav",std::ios::binary);f.write(reinterpret_cast<const char*>(trimmed->data()),trimmed->size());}
    assert(*pack.Audio({"audio/prompt.wav"})==*trimmed);
    assert(!pack.Audio({"../outside.wav"}));assert(!pack.Audio({"audio/missing.wav"}));
    rows[0]["file"]="../outside.wav";write();assert(!pack.Load(root.string()));
    rows[0]["file"]="audio/saria.wav";write();
    std::filesystem::create_symlink(std::filesystem::temp_directory_path(),root/"escape");
    rows[0]["file"]="escape/elsewhere.wav";write();assert(!pack.Load(root.string()));
    std::filesystem::remove_all(root);
    std::cout<<"Voice pack selection, age, ambiguity, templates, WAV, and traversal tests passed\n";
}
