#pragma once

#include "ARPGRules.h"
#include "SkillDefinitions.h"
#include "SkillRules.h"
#include "LootCatalog.h"
#include "ExpeditionRules.h"
#include "QuestRules.h"
#include <nlohmann/json.hpp>

namespace ARPG::SaveCodec {

// [TIME] | [INFO] | SaveCodec.Number | Validate before narrowing so malformed or newer saves cannot wrap indices or powers.
inline uint32_t Number(const nlohmann::json& object, const char* key, uint32_t fallback, uint32_t max) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) {
        return fallback;
    }
    if (it->is_number_unsigned()) {
        return static_cast<uint32_t>(std::min<uint64_t>(it->get<uint64_t>(), max));
    }
    return static_cast<uint32_t>(std::clamp<int64_t>(it->get<int64_t>(), 0, max));
}

inline ARPGItemInstance DecodeItem(const nlohmann::json& source, uint8_t slot) {
    ARPGItemInstance item{};
    item.slot = slot;
    if (!source.is_object()) { return item; }
    if (Loot::WeaponSlot(slot) && Number(source,"baseEquip",0,255)!=1) { return item; }
    item.rarity = Number(source, "rarity", 0, ARPG_RARITY_UNIQUE);
    item.baseEquip = Number(source, "baseEquip", 0, 3);
    item.itemLevel = Number(source, "itemLevel", 0, 50);
    item.uniqueId = Number(source,"uniqueId",0,255);
    item.setId = Number(source,"setId",0,255);
    if(!Loot::Unique(item)) { item.uniqueId=0; }
    if(!Loot::Set(item)) { item.setId=0; }
    item.socketCount = std::min<int>(Number(source,"socketCount",0,ARPG_ITEM_SOCKET_COUNT),Loot::MaxSockets(item));
    auto sockets=source.find("sockets");
    if(sockets!=source.end() && sockets->is_array()) {
        for(size_t i=0;i<item.socketCount && i<sockets->size();++i) {
            // Empty mounts retain their position so targeted socket placement survives saving.
            if((*sockets)[i].is_number_integer() && (*sockets)[i]==0)continue;
            auto material=Number({{"id",(*sockets)[i]}},"id",0,UINT32_MAX);
            if(!Loot::SocketMaterial(material)) { break; }
            item.sockets[i]=material;
        }
    }
    auto mods = source.find("mods");
    auto powers = source.find("powers");
    uint32_t count = Number(source, "modCount", 0, ARPG_ITEM_MOD_COUNT);
    if (mods == source.end() || powers == source.end() || !mods->is_array() || !powers->is_array()) {
        return item;
    }
    for (size_t m = 0; m < count && m < mods->size() && m < powers->size(); ++m) {
        uint32_t id = Number({ { "id", (*mods)[m] } }, "id", 0, UINT32_MAX);
        int power = Number({ { "power", (*powers)[m] } }, "power", 0, 100);
        bool duplicate = false;
        for (int n = 0; n < item.modCount; ++n) {
            duplicate |= item.mods[n].modId == id;
        }
        if (id > ARPG_MOD_NONE && id < ARPG_MOD_COUNT_ && power > 0 && !duplicate) {
            item.mods[item.modCount++] = { static_cast<uint8_t>(id), static_cast<int16_t>(power) };
        }
    }
    return item;
}

inline nlohmann::json EncodeItem(const ARPGItemInstance& item) {
    nlohmann::json mods = nlohmann::json::array(), powers = nlohmann::json::array();
    for (const auto& mod : item.mods) { mods.push_back(mod.modId); powers.push_back(mod.power); }
    return { { "rarity", item.rarity }, { "slot", item.slot }, { "baseEquip", item.baseEquip },
             { "uniqueId",item.uniqueId }, { "setId",item.setId }, { "socketCount",item.socketCount }, { "sockets",item.sockets },
             { "modCount", item.modCount }, { "itemLevel", item.itemLevel }, { "mods", mods }, { "powers", powers } };
}

inline ARPGSaveData Decode(const nlohmann::json& json, uint32_t fallbackSeed) {
    ARPGSaveData save{};
    save.seed = Number(json, "seed", fallbackSeed, UINT32_MAX);
    save.rngState = Number(json, "rngState", save.seed, UINT32_MAX);
    save.xp = Number(json, "xp", 0, Rules::XpThreshold(ARPG_MAX_LEVEL - 1));
    save.level = 1;
    while (save.level < ARPG_MAX_LEVEL && save.xp >= Rules::XpThreshold(save.level)) {
        ++save.level;
    }
    save.passivePoints = Number(json, "passivePoints", 0, 255);
    save.activePoints = Number(json, "activePoints", 0, 255);
    save.defeatedBosses = Number(json, "defeatedBosses", 0, UINT32_MAX);
    save.vitalityBonus = Number(json, "vitalityBonus", 0, 320);
    save.kills = Number(json, "kills", 0, UINT32_MAX);
    save.eliteKills = Number(json, "eliteKills", 0, UINT32_MAX);
    save.bossKills = Number(json, "bossKills", 0, UINT32_MAX);
    save.itemsFound = Number(json, "itemsFound", 0, UINT32_MAX);
    save.acquiredEquipment = Number(json, "acquiredEquipment", 0, (1u << (12 + ARPG_WEAPON_SLOT_COUNT)) - 1);
    save.skullRewardTier = Number(json, "skullRewardTier", 0, 32);
    auto secrets = json.find("secrets");
    if (secrets != json.end() && secrets->is_array()) {
        for (size_t i=0;i<std::min<size_t>(secrets->size(),ARPG_SECRET_COUNT);++i) {
            const auto& src=(*secrets)[i];if(!src.is_object())continue;
            auto& site=save.secrets[i];
            auto coordinate=[&](const char* key) {
                auto v=src.find(key);if(v==src.end() || !v->is_number())return NAN;
                double n=v->get<double>();return std::isfinite(n) && std::abs(n)<=32760?float(n):NAN;
            };
            site.x=coordinate("x");site.y=coordinate("y");site.z=coordinate("z");
            site.room=Number(src,"room",127,127);
            site.claimed=Number(src,"claimed",0,1);
            site.placed=Number(src,"placed",0,1) && std::isfinite(site.x) && std::isfinite(site.y) && std::isfinite(site.z) && site.room<64;
            if(!site.placed) { site.x=site.y=site.z=0; }
        }
    }
    auto ranks = json.find("skillRanks");
    if (ranks != json.end() && ranks->is_array()) {
        for (size_t i = 0; i < std::min<size_t>(ranks->size(), ARPG_SKILL_COUNT_); ++i) {
            save.skillRanks[i] = Number({ { "rank", (*ranks)[i] } }, "rank", 0, ARPG_SKILLS[i].maxRank);
        }
    }
    save.skillRevision = Number(json, "skillRevision", 0, 1);
    auto runes = json.find("skillRunes");
    if (runes != json.end() && runes->is_array()) {
        for (size_t i = 0; i < std::min<size_t>(runes->size(), ARPG_SKILL_COUNT_); ++i) {
            int rune = Number({ { "rune", (*runes)[i] } }, "rune", 0, 2);
            if (SkillRules::CanRune(save, static_cast<int>(i), rune)) { save.skillRunes[i] = rune; }
        }
    }
    std::fill(std::begin(save.actionSlots), std::end(save.actionSlots), ARPG_SKILL_NONE);
    auto slots = json.find("actionSlots");
    if (slots != json.end() && slots->is_array()) {
        for (size_t i = 0; i < std::min<size_t>(slots->size(), ARPG_ACTION_SLOTS); ++i) {
            int id = Number({ { "id", (*slots)[i] } }, "id", ARPG_SKILL_NONE, ARPG_SKILL_NONE);
            if (SkillRules::Castable(id)) {
                bool duplicate = false;
                for (size_t j = 0; j < i; ++j) { duplicate |= save.actionSlots[j] == id; }
                if (!duplicate) { save.actionSlots[i] = id; }
            }
        }
    }
    SkillRules::Initialize(save);
    auto gear = json.find("equipment");
    for (size_t i = 0; i < ARPG_SLOT_COUNT_; ++i) {
        save.equipment[i].slot = static_cast<uint8_t>(i);
        if (gear != json.end() && gear->is_array() && i < gear->size()) {
            save.equipment[i] = DecodeItem((*gear)[i], static_cast<uint8_t>(i));
        }
    }
    // [TIME] | [INFO] | SaveCodec.Decode | Older saves retain equipped rolls and start with an empty backpack.
    auto bag = json.find("backpack");
    if (bag != json.end() && bag->is_array()) {
        for (size_t i = 0; i < std::min<size_t>(bag->size(), ARPG_BACKPACK_CAPACITY); ++i) {
            const auto& source = (*bag)[i];
            auto slot = source.find("slot");
            if (!source.is_object() || slot == source.end() || !slot->is_number_integer() ||
                *slot < 0 || *slot >= ARPG_LOOT_SLOT_COUNT) { continue; }
            save.backpack[i] = DecodeItem(source, slot->get<uint8_t>());
        }
    }
    auto weapons=json.find("weapons");
    for(int i=0;i<ARPG_WEAPON_SLOT_COUNT;++i) {
        save.weapons[i].slot=ARPG_SLOT_COUNT_+i;
        if(weapons!=json.end() && weapons->is_array() && i<int(weapons->size())) {
            save.weapons[i]=DecodeItem((*weapons)[i],ARPG_SLOT_COUNT_+i);
        }
    }
    auto materials=json.find("materials");
    if(materials!=json.end() && materials->is_array()) {
        for(size_t i=1;i<std::min<size_t>(materials->size(),ARPG_MATERIAL_COUNT);++i) {
            save.materials[i]=Number({{"count",(*materials)[i]}},"count",0,ARPG_MATERIAL_CAPACITY);
        }
    }
    auto expedition=json.find("expedition");
    if(expedition!=json.end() && expedition->is_object()) {
        auto& r=save.expedition; const auto& source=*expedition;
        r.seed=Number(source,"seed",0,UINT32_MAX); r.depth=Number(source,"depth",0,UINT32_MAX);
        r.bestDepth=Number(source,"bestDepth",0,UINT32_MAX); r.clears=Number(source,"clears",0,UINT32_MAX);
        r.runs=Number(source,"runs",0,UINT32_MAX); r.phase=Number(source,"phase",0,255);
        r.targetSlot=Number(source,"targetSlot",0,255); r.baseEquip=Number(source,"baseEquip",0,255);
        r.baseLevel=Number(source,"baseLevel",0,50); r.mutator=Number(source,"mutator",0,4);
        r.objective=Number(source,"objective",0,255);
        ExpeditionRules::Recover(r);
    }
    auto campaign=json.find("campaign");
    if(campaign!=json.end() && campaign->is_object()) {
        auto& c=save.campaign;
        c.milestones=Number(*campaign,"milestones",0,UINT32_MAX);
        c.tracked=Number(*campaign,"tracked",0,255);
        auto quests=campaign->find("quests");
        if(quests!=campaign->end() && quests->is_array())for(size_t q=0;q<std::min<size_t>(quests->size(),ARPG_QUEST_COUNT);++q) {
            const auto& src=(*quests)[q];if(!src.is_object())continue;
            auto& p=c.quests[q];p.state=Number(src,"state",0,255);p.party=Number(src,"party",0,UINT32_MAX);p.partySize=Number(src,"partySize",0,8);
            for(const char* key:{"progress","nodes"}) {
                auto values=src.find(key);if(values==src.end() || !values->is_array())continue;
                for(size_t i=0;i<std::min<size_t>(values->size(),ARPG_QUEST_OBJECTIVES);++i)
                    (std::string(key)=="progress"?p.progress:p.nodes)[i]=Number({{"n",(*values)[i]}},"n",0,255);
            }
            auto sites=src.find("sites");
            if(sites!=src.end() && sites->is_array())for(size_t i=0;i<std::min<size_t>(sites->size(),ARPG_QUEST_OBJECTIVES);++i) {
                const auto& row=(*sites)[i];if(!row.is_array())continue;
                for(size_t n=0;n<std::min<size_t>(row.size(),ARPG_QUEST_NODES);++n) {
                    const auto& point=row[n];if(!point.is_object())continue;
                    auto& site=p.sites[i][n];
                    auto coordinate=[&](const char* key) {
                        auto v=point.find(key);if(v==point.end() || !v->is_number())return NAN;
                        double value=v->get<double>();return std::isfinite(value) && std::abs(value)<=32760?float(value):NAN;
                    };
                    site.x=coordinate("x");site.y=coordinate("y");site.z=coordinate("z");
                    site.room=Number(point,"room",127,127);site.adult=Number(point,"adult",0,255);
                    site.placed=Number(point,"placed",0,255);
                }
            }
        }
        Quests::Sanitize(c);
    }
    return save;
}

inline nlohmann::json Encode(const ARPGSaveData& save) {
    nlohmann::json json = {
        { "seed", save.seed }, { "rngState", save.rngState }, { "xp", save.xp }, { "level", save.level },
        { "passivePoints", save.passivePoints }, { "activePoints", save.activePoints },
        { "defeatedBosses", save.defeatedBosses }, { "vitalityBonus", save.vitalityBonus }, { "kills", save.kills }, { "eliteKills", save.eliteKills },
        { "acquiredEquipment", save.acquiredEquipment }, { "bossKills", save.bossKills }, { "itemsFound", save.itemsFound }, { "materials",save.materials },
        { "skillRevision", save.skillRevision }, { "skillRunes", save.skillRunes },
        { "actionSlots", save.actionSlots }, { "skillRanks", save.skillRanks }, { "equipment", nlohmann::json::array() }
    };
    json["skullRewardTier"]=save.skullRewardTier;
    json["secrets"]=nlohmann::json::array();
    for(const auto& site:save.secrets) {
        if(!site.placed && !site.claimed) { json["secrets"].push_back(nullptr);continue; }
        json["secrets"].push_back({{"x",site.x},{"y",site.y},{"z",site.z},{"room",site.room},{"placed",site.placed},{"claimed",site.claimed}});
    }
    json["campaign"]={{"milestones",save.campaign.milestones},{"tracked",save.campaign.tracked},{"quests",nlohmann::json::array()}};
    for(const auto& p:save.campaign.quests) {
        if(!p.state) {json["campaign"]["quests"].push_back(nullptr);continue;}
        nlohmann::json sites=nlohmann::json::array();
        for(const auto& row:p.sites) {
            nlohmann::json nodes=nlohmann::json::array();
            for(const auto& site:row) {
                if(!site.placed)nodes.push_back(nullptr);
                else nodes.push_back({{"x",site.x},{"y",site.y},{"z",site.z},{"room",site.room},{"placed",site.placed},{"adult",site.adult}});
            }
            sites.push_back(std::move(nodes));
        }
        json["campaign"]["quests"].push_back({{"state",p.state},{"party",p.party},{"partySize",p.partySize},{"progress",p.progress},{"nodes",p.nodes},{"sites",std::move(sites)}});
    }
    const auto& r=save.expedition;
    json["expedition"]={{"seed",r.seed},{"depth",r.depth},{"bestDepth",r.bestDepth},{"clears",r.clears},
        {"runs",r.runs},{"phase",r.phase},{"targetSlot",r.targetSlot},{"baseEquip",r.baseEquip},{"baseLevel",r.baseLevel},{"mutator",r.mutator},{"objective",r.objective}};
    for (const auto& item : save.equipment) { json["equipment"].push_back(EncodeItem(item)); }
    json["weapons"] = nlohmann::json::array();
    for (int i=0;i<ARPG_WEAPON_SLOT_COUNT;++i) { auto item=save.weapons[i];item.slot=ARPG_SLOT_COUNT_+i;json["weapons"].push_back(EncodeItem(item)); }
    json["backpack"] = nlohmann::json::array();
    for (const auto& item : save.backpack) { json["backpack"].push_back(EncodeItem(item)); }
    return json;
}

} // namespace ARPG::SaveCodec
