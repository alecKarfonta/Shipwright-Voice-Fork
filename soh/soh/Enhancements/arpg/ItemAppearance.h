#pragma once
#include "InventoryRules.h"

namespace ARPG::ItemAppearance {
enum class Kind { Native, Ember, Frost, Light, Root, Star, Wind,
    ForestBlade, KnightBlade, GreatBlade, WoodShield, KnightShield, MirrorShield,
    ForestTunic, EmberTunic, TideTunic, TravelBoots, IronBoots, HoverBoots,
    RecurveBow=24, WarHammer,
    Cinderwake=32, MoonwellFang, BriarOath, Dawnglass, AshenPilgrim, VeilOfTheDeep, WayfarersPromise, Redstep,
    BriarwatchBlade, BriarwatchShield, BriarwatchTunic, BriarwatchBoots,
    CinderboundBlade, CinderboundShield, CinderboundTunic, CinderboundBoots,
    TidecallerBlade, TidecallerShield, TidecallerTunic, TidecallerBoots };
struct Style { const char* artwork; const char* name; Kind kind = Kind::Native; };

inline constexpr int RelicCount=20;
inline int RelicIndex(Kind kind) { return kind>=Kind::Cinderwake && kind<=Kind::TidecallerBoots ? int(kind)-int(Kind::Cinderwake) : -1; }
inline int RelicSlot(Kind kind) {
    int id=RelicIndex(kind); return id<0 ? -1 : id<8 ? id/2 : (id-8)%4;
}
inline constexpr Style relics[RelicCount]={
    {"relics/cinderwake","Cinderwake",Kind::Cinderwake},
    {"relics/moonwell-fang","Moonwell Fang",Kind::MoonwellFang},
    {"relics/briar-oath","Briar Oath",Kind::BriarOath},
    {"relics/dawnglass","Dawnglass",Kind::Dawnglass},
    {"relics/ashen-pilgrim","Ashen Pilgrim",Kind::AshenPilgrim},
    {"relics/veil-of-the-deep","Veil of the Deep",Kind::VeilOfTheDeep},
    {"relics/wayfarers-promise","Wayfarer's Promise",Kind::WayfarersPromise},
    {"relics/redstep","Redstep",Kind::Redstep},
    {"relics/briarwatch-blade","Briarwatch blade",Kind::BriarwatchBlade},
    {"relics/briarwatch-shield","Briarwatch shield",Kind::BriarwatchShield},
    {"relics/briarwatch-tunic","Briarwatch tunic",Kind::BriarwatchTunic},
    {"relics/briarwatch-boots","Briarwatch boots",Kind::BriarwatchBoots},
    {"relics/cinderbound-blade","Cinderbound blade",Kind::CinderboundBlade},
    {"relics/cinderbound-shield","Cinderbound shield",Kind::CinderboundShield},
    {"relics/cinderbound-tunic","Cinderbound tunic",Kind::CinderboundTunic},
    {"relics/cinderbound-boots","Cinderbound boots",Kind::CinderboundBoots},
    {"relics/tidecaller-blade","Tidecaller blade",Kind::TidecallerBlade},
    {"relics/tidecaller-shield","Tidecaller shield",Kind::TidecallerShield},
    {"relics/tidecaller-tunic","Tidecaller tunic",Kind::TidecallerTunic},
    {"relics/tidecaller-boots","Tidecaller boots",Kind::TidecallerBoots},
};
// [TIME] | [INFO] | ItemAppearance.Relic | Saved catalog identity takes precedence over mutable socket powers.
inline Style Relic(const ARPGItemInstance& item) {
    if(item.slot>=ARPG_SLOT_COUNT_) { return {nullptr,nullptr}; }
    if(Loot::Unique(item) && item.uniqueId<=8) { return relics[item.uniqueId-1]; }
    if(Loot::Set(item)) { return relics[8+(item.setId-1)*4+item.slot]; }
    return {nullptr,nullptr};
}
// [TIME] | [INFO] | ItemAppearance.Get | Derive presentation from saved modifiers without consuming RNG or changing gear powers.
inline Style Get(const ARPGItemInstance& item) {
    auto relic=Relic(item); if(relic.kind!=Kind::Native) { return relic; }
    if(item.uniqueId>=19 && Loot::Unique(item))return {nullptr,nullptr};
    auto has = [&](int id) { return InventoryRules::Power(item, id) > 0; };
    switch (item.slot) {
        case ARPG_SLOT_SWORD:
            if (has(ARPG_MOD_FLAME)) { return {"ember-sword", "Ember-forged", Kind::Ember}; }
            if (has(ARPG_MOD_FROST)) { return {"gear/frost-sword", "Frostbound", Kind::Frost}; }
            if (has(ARPG_MOD_STORM)) { return {"gear/light-sword", "Lightforged", Kind::Light}; }
            break;
        case ARPG_SLOT_SHIELD:
            if (has(ARPG_MOD_AEGIS)) { return {"root-shield", "Rootwarden", Kind::Root}; }
            break;
        case ARPG_SLOT_TUNIC:
            if (has(ARPG_MOD_MAGICAFF)) { return {"star-tunic", "Star-woven", Kind::Star}; }
            break;
        case ARPG_SLOT_BOOTS:
            if (has(ARPG_MOD_SWIFT)) { return {"wind-boots", "Wind-stride", Kind::Wind}; }
            break;
    }
    return {nullptr, nullptr};
}
// [TIME] | [INFO] | ItemAppearance.Resolve | Inventory paintings and world geometry resolve through one catalog, including unmodified bases.
inline Style Resolve(const ARPGItemInstance& item) {
    if(item.baseEquip==1 && item.slot==ARPG_SLOT_BOW)return {"weapons/recurve-bow","Horn-tipped recurve",Kind::RecurveBow};
    if(item.baseEquip==1 && item.slot==ARPG_SLOT_HAMMER)return {"weapons/war-hammer","Bronze-bound war hammer",Kind::WarHammer};
    if(item.slot>=ARPG_SLOT_COUNT_ || item.baseEquip<1 || item.baseEquip>3) { return {nullptr,nullptr}; }
    auto style=Get(item);
    if(style.kind!=Kind::Native) { return style; }
    constexpr Style bases[4][3]={
        {{"gear/blade-forest","Forest blade",Kind::ForestBlade},{"gear/blade-knight","Knight blade",Kind::KnightBlade},{"gear/blade-heavy","Great blade",Kind::GreatBlade}},
        {{"gear/shield-wood","Wood shield",Kind::WoodShield},{"gear/shield-knight","Knight shield",Kind::KnightShield},{"gear/shield-mirror","Mirror shield",Kind::MirrorShield}},
        {{"gear/tunic-forest","Forest cloth",Kind::ForestTunic},{"gear/tunic-ember","Ember cloth",Kind::EmberTunic},{"gear/tunic-tide","Tide cloth",Kind::TideTunic}},
        {{"gear/boots-travel","Travel leather",Kind::TravelBoots},{"gear/boots-iron","Iron greaves",Kind::IronBoots},{"gear/boots-wind","Hover boots",Kind::HoverBoots}}
    };
    return bases[item.slot][item.baseEquip-1];
}
inline Kind WorldKind(const ARPGItemInstance& item) { return Resolve(item).kind; }
inline bool IsSword(Kind k) { return RelicSlot(k)==0 || k==Kind::Ember || k==Kind::Frost || k==Kind::Light || (k>=Kind::ForestBlade && k<=Kind::GreatBlade); }
inline bool IsShield(Kind k) { return RelicSlot(k)==1 || k==Kind::Root || (k>=Kind::WoodShield && k<=Kind::MirrorShield); }
inline bool IsTunic(Kind k) { return RelicSlot(k)==2 || k==Kind::Star || (k>=Kind::ForestTunic && k<=Kind::TideTunic); }
inline bool IsBoots(Kind k) { return RelicSlot(k)==3 || k==Kind::Wind || (k>=Kind::TravelBoots && k<=Kind::HoverBoots); }

}
