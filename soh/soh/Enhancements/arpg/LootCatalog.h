#pragma once
#include "ARPGRules.h"
#include <array>
#include <string>

namespace ARPG::Loot {
// [TIME] | [INFO] | Loot.Catalog | IDs are append-only save identities; definitions contain the actual powers used by combat and the journal.
constexpr int GemKinds=6, GemRanks=3, RuneFirst=19, RuneLast=24, Essence=25;
constexpr int UniqueCount=30, SetCount=3;
inline bool WeaponSlot(int slot) { return slot>=ARPG_SLOT_BOW && slot<ARPG_LOOT_SLOT_COUNT; }
inline constexpr const char* SlotNames[]={"Sword","Shield","Tunic","Boots","Bow","Hammer","Bombs","Slingshot","Hookshot","Boomerang","Deku nuts","Bombchus","Deku stick","Lens","Ocarina","Bottle"};
inline constexpr const char* WeaponNames[]={"Recurve Bow","War Hammer","Bomb Satchel","Seed Satchel","Hookshot","Boomerang","Nut Satchel","Bombchu Case","Deku Branch","Lens of Truth","Ocarina","Spirit Bottle"};
inline bool RangedSlot(int slot) { return slot==ARPG_SLOT_BOW || slot==ARPG_SLOT_SLINGSHOT || slot==ARPG_SLOT_BOMBS || slot==ARPG_SLOT_BOMBCHU; }
inline const char* WeaponPowerName(const ARPGItemInstance& item) {
    constexpr const char* names[]={"Patient hunter","Mountain's echo","Goron powder","Bloomchain","Ember seeds","Winter seeds","Far reach","Iron snare","Hunter's crescent","Deku ambush","Frost shatter","Stormthread","Thornwheel","Dodongo chase","Cinder sweep","Dawn charge","Hidden weakness","Mountain anchor","Gale landing","Stormsong","Guiding flame","Scarecrow reel"};
    return item.uniqueId>=9 && item.uniqueId<=30 ? names[item.uniqueId-9] : "";
}
inline const char* WeaponPowerDescription(const ARPGItemInstance& item) {
    constexpr const char* descriptions[]={
        "Bow hits mark an unmarked foe for 4 seconds. The next bow or seed hit consumes the mark for +75% damage.",
        "Hammer fault lines recover twice as fast and secondary hits use full damage instead of 65%.",
        "Bomb explosions reach 60% farther. The larger native blast can also hurt Link: leave room before it bursts.",
        "A thrown bomb releases three staggered secondary explosions around its detonation. Each enemy takes at most one secondary hit at half damage. Needs solid ground and a clear path; echoes cannot create more echoes.",
        "Every damaging seed carries fire: +50% elemental damage and the foe's native fire reaction when it is susceptible.",
        "Every damaging seed carries ice: +50% elemental damage and the foe's native ice reaction. Susceptible arsenal foes are rooted for 1 second; bosses keep their own movement rules.",
        "Hookshot and Longshot travel 75% farther. Native latch surfaces, retraction and item progression still apply.",
        "Hookshot travels 25% farther. Accepted stuns tether foes for 6 seconds; the next hammer or bomb consumes the tether for +125% damage.",
        "Boomerang travels 35% farther. Accepted stuns mark foes for 6 seconds; the next arrow or seed consumes the mark for +125% damage.",
        "Nut flashes reach 40% farther. Accepted stuns also mark foes for 5.4 seconds, setting up +95% arrow or seed finishers.",
        "Hammer a frozen enemy to consume the freeze and scatter ice fragments over 135 units. Each nearby susceptible foe is hit once. Recharges in 1 second.",
        "An arrow into a tethered foe sends lightning along the tether toward Link. Wet enemies within 170 units also conduct up to three arcs. Recharges in 1 second.",
        "An accepted boomerang stun starts a 1.5-second orbit around its marked target, interrupting susceptible foes and gathering nearby dropped ammo. Quick recall ends the orbit.",
        "Released Bombchus gently steer toward your locked enemy. A damaging explosion on a burning foe creates one delayed blast behind it. Each Bombchu can trigger this only once.",
        "Strike a burning foe to kindle your Deku stick. Its next damaging hit breaks it in a fiery sweep. The charge lasts 6 seconds; normal stick ammo is consumed on breaking.",
        "Block within 0.3 seconds of raising the Mirror Shield to store light for 6 seconds. Your next released bow shot spends it on a piercing beam that strikes each susceptible foe once.",
        "Use the Lens of Truth to reveal a moving weak point on your locked foe. Hit the visible point with an arrow or seed to stagger it for 1 second, extended by Control. Native Lens magic drain applies; 2-second recovery per foe.",
        "Wear Iron Boots and stand grounded for 2 seconds to charge your next hammer hit. It draws nearby movable enemies toward the impact. Moving or changing boots clears the charge.",
        "Wear Hover Boots and release an arrow during a hover to arrest drift briefly. Your next landing sends a gust that pushes nearby movable enemies away. One gust per airborne volley.",
        "Play the learned Song of Storms in free play to create a rain patch for 8 seconds, extended by Control. Wet foes conduct Stormthread arcs; Winterbud freezes them for 2 seconds. One patch at a time; 10-second recovery.",
        "Catch blue fire in a bottle, then release it with this relic equipped. A wisp pursues the nearest boomerang-marked foe for 8 seconds, extended by Control, and bursts in flame. The native bottle is emptied as usual.",
        "Accepted Hookshot stuns reel light foes toward Link. Heavy susceptible foes instead draw Link toward them, stopping at striking range. Pulls follow clear, safe ground and cannot move bosses."
    };
    return item.uniqueId>=9 && item.uniqueId<=30 ? descriptions[item.uniqueId-9] : "";
}
struct ModPool { std::array<uint8_t,6> ids{}; int count=0; };
inline ModPool Pool(int slot) {
    switch(slot) {
        case ARPG_SLOT_SWORD: return {{{ARPG_MOD_BRUTAL,ARPG_MOD_KEEN,ARPG_MOD_FLAME,ARPG_MOD_FROST,ARPG_MOD_STORM,ARPG_MOD_ARCHERY}},6};
        case ARPG_SLOT_SHIELD: case ARPG_SLOT_TUNIC: return {{{ARPG_MOD_AEGIS,ARPG_MOD_VITALITY,ARPG_MOD_MAGICAFF}},3};
        case ARPG_SLOT_BOOTS: return {{{ARPG_MOD_SWIFT,ARPG_MOD_LUCKY,ARPG_MOD_VITALITY}},3};
        case ARPG_SLOT_HAMMER: case ARPG_SLOT_STICK: return {{{ARPG_MOD_BRUTAL,ARPG_MOD_KEEN,ARPG_MOD_FLAME,ARPG_MOD_FROST,ARPG_MOD_STORM}},5};
        case ARPG_SLOT_BOMBS: case ARPG_SLOT_BOMBCHU: return {{{ARPG_MOD_ARCHERY,ARPG_MOD_KEEN,ARPG_MOD_BLAST,ARPG_MOD_CONTROL}},4};
        case ARPG_SLOT_HOOKSHOT: case ARPG_SLOT_BOOMERANG: return {{{ARPG_MOD_REACH,ARPG_MOD_CONTROL,ARPG_MOD_FOLLOWUP}},3};
        case ARPG_SLOT_LENS: case ARPG_SLOT_OCARINA: case ARPG_SLOT_BOTTLE: return {{{ARPG_MOD_CONTROL,ARPG_MOD_REACH,ARPG_MOD_LUCKY}},3};
        case ARPG_SLOT_NUTS: return {{{ARPG_MOD_BLAST,ARPG_MOD_CONTROL,ARPG_MOD_FOLLOWUP}},3};
        default: return {{{ARPG_MOD_ARCHERY,ARPG_MOD_KEEN,ARPG_MOD_FLAME,ARPG_MOD_FROST,ARPG_MOD_STORM}},5};
    }
}
enum class CombatPower : uint8_t { None, FurnaceRhythm, MoonwellWeave, BriarReprisal };
inline const char* PowerName(CombatPower power) {
    switch(power) {
        case CombatPower::FurnaceRhythm: return "Furnace rhythm";
        case CombatPower::MoonwellWeave: return "Moonwell weave";
        case CombatPower::BriarReprisal: return "Briar reprisal";
        default: return "";
    }
}
inline const char* PowerDescription(CombatPower power) {
    switch(power) {
        case CombatPower::FurnaceRhythm: return "After a sword technique, land a basic weapon hit within 6 seconds to shorten all sword cooldowns by 1 second. One follow-up per technique.";
        case CombatPower::MoonwellWeave: return "Land 3 basic weapon hits, no more than 6 seconds apart. Your next Flame Burst, Frost Nova, Chain Lightning or Meteor costs no magic. Keep striking to hold the charge; spend it within 6 seconds.";
        case CombatPower::BriarReprisal: return "After a guardian technique, taking health damage within 6 seconds arms a counter. Your next basic weapon hit within 6 seconds restores that loss, up to half a heart. Recharges for 10 seconds when armed.";
        default: return "";
    }
}
struct Style { int tree=-1, power=0, recovery=0, manaOnHit=0, lifeOnKill=0; CombatPower combatPower=CombatPower::None; };
struct UniqueDef { const char* name; uint8_t slot, minLevel; ARPGItemMod mods[3]; Style style; const char* lore; uint8_t requiredBase=0; };
inline constexpr UniqueDef Uniques[] = {
    {"",0,0,{},{} ,""},
    {"Cinderwake",0,1,{{ARPG_MOD_FLAME,28},{ARPG_MOD_BRUTAL,24},{ARPG_MOD_KEEN,8}},{ARPG_TREE_SWORD,20,0,0,0,CombatPower::FurnaceRhythm},"A furnace edge that rewards committed sword techniques."},
    {"Moonwell Fang",0,18,{{ARPG_MOD_FROST,30},{ARPG_MOD_ARCHERY,30},{ARPG_MOD_KEEN,12}},{ARPG_TREE_MAGIC,0,0,2,0,CombatPower::MoonwellWeave},"Basic sword hits restore two magic. Keep striking between spells."},
    {"Briar Oath",1,1,{{ARPG_MOD_AEGIS,5},{ARPG_MOD_VITALITY,18},{ARPG_MOD_LUCKY,18}},{ARPG_TREE_GUARDIAN,25,15,0,0,CombatPower::BriarReprisal},"Guardians answer pressure with faster, stronger techniques."},
    {"Dawnglass",1,22,{{ARPG_MOD_MAGICAFF,2},{ARPG_MOD_VITALITY,20},{ARPG_MOD_STORM,25}},{ARPG_TREE_MAGIC,20,0,0,0},"A prism that strengthens magical techniques."},
    {"Ashen Pilgrim",2,1,{{ARPG_MOD_VITALITY,24},{ARPG_MOD_AEGIS,4},{ARPG_MOD_FLAME,20}},{ARPG_TREE_SWORD,0,12,0,4},"Kills restore a quarter heart; sword techniques return sooner."},
    {"Veil of the Deep",2,20,{{ARPG_MOD_MAGICAFF,3},{ARPG_MOD_FROST,25},{ARPG_MOD_VITALITY,22}},{ARPG_TREE_MAGIC,0,18,0,0},"A spellcaster's mantle woven for rapid recasting."},
    {"Wayfarer's Promise",3,1,{{ARPG_MOD_SWIFT,10},{ARPG_MOD_LUCKY,28},{ARPG_MOD_VITALITY,16}},{ARPG_TREE_GUARDIAN,0,20,0,0},"Mobility and defensive techniques recover faster."},
    {"Redstep",3,24,{{ARPG_MOD_SWIFT,12},{ARPG_MOD_KEEN,10},{ARPG_MOD_BRUTAL,22}},{ARPG_TREE_SWORD,15,10,0,0},"Keep the offensive rhythm while moving between packs."},
    {"Windpiercer",ARPG_SLOT_BOW,1,{{ARPG_MOD_ARCHERY,30},{ARPG_MOD_KEEN,12},{ARPG_MOD_STORM,20}},{},"Wait for the opening. The second arrow remembers the first."},
    {"Mountainwake",ARPG_SLOT_HAMMER,1,{{ARPG_MOD_BRUTAL,32},{ARPG_MOD_FLAME,25},{ARPG_MOD_KEEN,8}},{},"Its bronze face carries the echo of a mountain's first fracture."},
    {"Goron Powderkeg",ARPG_SLOT_BOMBS,1,{{ARPG_MOD_BLAST,60},{ARPG_MOD_ARCHERY,20},{ARPG_MOD_CONTROL,20}},{},"A pinch of Death Mountain in every fuse."},
    {"Bombflower Chorus",ARPG_SLOT_BOMBS,1,{{ARPG_MOD_ARCHERY,25},{ARPG_MOD_BLAST,15},{ARPG_MOD_CONTROL,25}},{},"One bloom wakes three echoes beneath the earth."},
    {"Emberheart Seeds",ARPG_SLOT_SLINGSHOT,1,{{ARPG_MOD_FLAME,100},{ARPG_MOD_ARCHERY,20},{ARPG_MOD_KEEN,8}},{},"Seeds warmed in the heart of a sleeping volcano."},
    {"Winterbud Seeds",ARPG_SLOT_SLINGSHOT,1,{{ARPG_MOD_FROST,100},{ARPG_MOD_ARCHERY,20},{ARPG_MOD_KEEN,8}},{},"A Kokiri winter, waiting in a handful of seeds."},
    {"Skyvine",ARPG_SLOT_HOOKSHOT,1,{{ARPG_MOD_REACH,75},{ARPG_MOD_CONTROL,25},{ARPG_MOD_FOLLOWUP,10}},{},"A silver vine that remembers the sky."},
    {"Ironroot",ARPG_SLOT_HOOKSHOT,1,{{ARPG_MOD_REACH,25},{ARPG_MOD_CONTROL,50},{ARPG_MOD_FOLLOWUP,50}},{},"Catch the opening. Let the hammer answer."},
    {"Gale Crescent",ARPG_SLOT_BOOMERANG,1,{{ARPG_MOD_REACH,35},{ARPG_MOD_CONTROL,50},{ARPG_MOD_FOLLOWUP,50}},{},"A hunter's promise carried on the forest wind."},
    {"Deku Starburst",ARPG_SLOT_NUTS,1,{{ARPG_MOD_BLAST,40},{ARPG_MOD_CONTROL,35},{ARPG_MOD_FOLLOWUP,20}},{},"A bright flash turns a crowded clearing into an ambush."},
    {"Frostbreaker",ARPG_SLOT_HAMMER,1,{{ARPG_MOD_BRUTAL,28},{ARPG_MOD_FROST,20},{ARPG_MOD_KEEN,8}},{},"Winter splits where the mountain strikes.",0},
    {"Stormthread",ARPG_SLOT_BOW,1,{{ARPG_MOD_ARCHERY,25},{ARPG_MOD_STORM,25},{ARPG_MOD_KEEN,8}},{},"A silver string remembers every thunderstorm.",0},
    {"Thornwheel",ARPG_SLOT_BOOMERANG,1,{{ARPG_MOD_REACH,20},{ARPG_MOD_CONTROL,25},{ARPG_MOD_FOLLOWUP,20}},{},"A ring of thorns keeps the hunt alive.",0},
    {"Dodongo Bombchus",ARPG_SLOT_BOMBCHU,1,{{ARPG_MOD_ARCHERY,20},{ARPG_MOD_BLAST,20},{ARPG_MOD_CONTROL,20}},{},"Little feet carry a hungry mountain flame.",0},
    {"Cinderbranch",ARPG_SLOT_STICK,1,{{ARPG_MOD_BRUTAL,20},{ARPG_MOD_FLAME,25},{ARPG_MOD_KEEN,8}},{},"An old Deku branch keeps one last summer inside.",0},
    {"Dawnmirror",ARPG_SLOT_SHIELD,1,{{ARPG_MOD_AEGIS,4},{ARPG_MOD_VITALITY,15},{ARPG_MOD_MAGICAFF,1}},{},"Catch the first light. Send it flying.",3},
    {"Sheikah's Eye",ARPG_SLOT_LENS,1,{{ARPG_MOD_CONTROL,25},{ARPG_MOD_REACH,20},{ARPG_MOD_LUCKY,10}},{},"Truth moves beneath the surface.",0},
    {"Goron Anchor",ARPG_SLOT_BOOTS,1,{{ARPG_MOD_AEGIS,4},{ARPG_MOD_VITALITY,18},{ARPG_MOD_BRUTAL,15}},{},"Stand like a mountain. Strike like its fall.",2},
    {"Galecrest",ARPG_SLOT_BOOTS,1,{{ARPG_MOD_SWIFT,8},{ARPG_MOD_ARCHERY,20},{ARPG_MOD_VITALITY,12}},{},"A feather on the wind, a storm upon the earth.",3},
    {"Stormsong",ARPG_SLOT_OCARINA,1,{{ARPG_MOD_CONTROL,25},{ARPG_MOD_REACH,20},{ARPG_MOD_LUCKY,10}},{},"Six notes call the rain back home.",0},
    {"Bottled Will-o'-Wisp",ARPG_SLOT_BOTTLE,1,{{ARPG_MOD_CONTROL,25},{ARPG_MOD_REACH,20},{ARPG_MOD_LUCKY,10}},{},"A wandering blue flame follows the forest hunter.",0},
    {"Scarecrow Hookshot",ARPG_SLOT_HOOKSHOT,1,{{ARPG_MOD_REACH,25},{ARPG_MOD_CONTROL,25},{ARPG_MOD_FOLLOWUP,20}},{},"If the mountain will not dance, dance toward it.",0}
};
struct SetDef { const char* name; int minLevel; ARPGItemMod two[2], four[2]; Style style; };
inline constexpr SetDef Sets[] = {
    {"",0,{}, {},{}},
    {"Briarwatch",1,{{ARPG_MOD_AEGIS,4},{ARPG_MOD_VITALITY,12}},{{ARPG_MOD_LUCKY,25},{ARPG_MOD_VITALITY,15}},{ARPG_TREE_GUARDIAN,30,20,0,4,CombatPower::BriarReprisal}},
    {"Cinderbound",14,{{ARPG_MOD_BRUTAL,20},{ARPG_MOD_FLAME,15}},{{ARPG_MOD_KEEN,12},{ARPG_MOD_SWIFT,6}},{ARPG_TREE_SWORD,30,15,0,0,CombatPower::FurnaceRhythm}},
    {"Tidecaller",22,{{ARPG_MOD_MAGICAFF,2},{ARPG_MOD_FROST,15}},{{ARPG_MOD_STORM,20},{ARPG_MOD_VITALITY,20}},{ARPG_TREE_MAGIC,30,20,1,0,CombatPower::MoonwellWeave}}
};
struct RuneworkDef { const char* name; uint16_t slotMask; uint8_t count, runes[3]; ARPGItemMod mods[3]; Style style; };
inline constexpr RuneworkDef Runeworks[] = {
    {"",0,0,{}, {},{}},
    {"Kindling",1,2,{19,20,0},{{ARPG_MOD_FLAME,25},{ARPG_MOD_BRUTAL,18},{}},{ARPG_TREE_SWORD,15,0,0,0}},
    {"Stillwater",6,2,{21,22,0},{{ARPG_MOD_MAGICAFF,2},{ARPG_MOD_FROST,20},{}},{ARPG_TREE_MAGIC,0,12,0,0}},
    {"Waywind",8,2,{20,22,0},{{ARPG_MOD_SWIFT,10},{ARPG_MOD_LUCKY,20},{}},{ARPG_TREE_GUARDIAN,0,15,0,0}},
    {"Bulwark",6,2,{21,19,0},{{ARPG_MOD_AEGIS,6},{ARPG_MOD_VITALITY,20},{}},{ARPG_TREE_GUARDIAN,20,0,0,0}},
    {"Sunbreaker",1,3,{19,23,20},{{ARPG_MOD_BRUTAL,35},{ARPG_MOD_FLAME,30},{ARPG_MOD_KEEN,12}},{ARPG_TREE_SWORD,25,10,0,0,CombatPower::FurnaceRhythm}},
    {"Nightwell",5,3,{24,21,22},{{ARPG_MOD_MAGICAFF,3},{ARPG_MOD_FROST,25},{ARPG_MOD_VITALITY,15}},{ARPG_TREE_MAGIC,25,15,2,0,CombatPower::MoonwellWeave}},
    {"Oathkeeper",6,3,{21,23,19},{{ARPG_MOD_AEGIS,8},{ARPG_MOD_VITALITY,25},{ARPG_MOD_LUCKY,20}},{ARPG_TREE_GUARDIAN,30,15,0,4,CombatPower::BriarReprisal}},
    {"Stormpath",9,3,{20,23,24},{{ARPG_MOD_STORM,35},{ARPG_MOD_SWIFT,10},{ARPG_MOD_ARCHERY,30}},{ARPG_TREE_SWORD,15,15,1,0}},
    {"Galeweave",1<<ARPG_SLOT_BOW,3,{20,23,22},{{ARPG_MOD_ARCHERY,40},{ARPG_MOD_KEEN,14},{ARPG_MOD_STORM,30}},{}},
    {"Deepforge",1<<ARPG_SLOT_HAMMER,3,{19,21,23},{{ARPG_MOD_BRUTAL,45},{ARPG_MOD_FLAME,30},{ARPG_MOD_KEEN,10}},{}}
};
inline const UniqueDef* Unique(const ARPGItemInstance& item) {
    return item.uniqueId>0 && item.uniqueId<=UniqueCount && !item.setId &&
        (Uniques[item.uniqueId].requiredBase==0 || Uniques[item.uniqueId].requiredBase==item.baseEquip) &&
        Uniques[item.uniqueId].slot==item.slot && item.rarity==ARPG_RARITY_UNIQUE ? &Uniques[item.uniqueId] : nullptr;
}
inline const SetDef* Set(const ARPGItemInstance& item) {
    return item.slot<ARPG_SLOT_COUNT_ && item.setId>0 && item.setId<=SetCount && !item.uniqueId && item.rarity==ARPG_RARITY_UNIQUE ? &Sets[item.setId] : nullptr;
}
inline bool Gem(int id) { return id>=1 && id<=18; }
inline bool Rune(int id) { return id>=RuneFirst && id<=RuneLast; }
inline bool SocketMaterial(int id) { return Gem(id)||Rune(id); }
inline std::string MaterialName(int id) {
    static const char* grades[]={"Rough","Cut","Perfect"};
    static const char* gems[]={"Ruby","Sapphire","Topaz","Emerald","Amethyst","Diamond"};
    static const char* runes[]={"Em","Va","Kor","Ser","Lux","Umbra"};
    if(Gem(id)) { return std::string(grades[(id-1)/GemKinds])+" "+gems[(id-1)%GemKinds]; }
    if(Rune(id)) { return std::string(runes[id-RuneFirst])+" Rune"; }
    return id==Essence?"Forge Essence":"Empty";
}
inline int Filled(const ARPGItemInstance& item) {
    int n=0; for(int i=0;i<std::min<int>(item.socketCount,ARPG_ITEM_SOCKET_COUNT);++i) { n+=SocketMaterial(item.sockets[i]); } return n;
}
inline int Runework(const ARPGItemInstance& item) {
    if(item.slot>=ARPG_LOOT_SLOT_COUNT || item.rarity!=ARPG_RARITY_COMMON || item.modCount || item.uniqueId || item.setId) { return 0; }
    for(int i=1;i<int(std::size(Runeworks));++i) {
        const auto& r=Runeworks[i];
        if(!(r.slotMask&(1<<item.slot)) || item.socketCount!=r.count) { continue; }
        bool match=true; for(int j=0;j<r.count;++j) { match&=item.sockets[j]==r.runes[j]; }
        if(match) { return i; }
    }
    return 0;
}
inline const char* Quality(const ARPGItemInstance& item) {
    if(Set(item)) { return "SET"; } if(Runework(item)) { return "RUNEWORK"; }
    static const char* names[]={"COMMON","MAGIC","RARE","UNIQUE"}; return names[std::min<int>(3,item.rarity)];
}
inline ARPGItemMod SocketBonus(int material,int slot) {
    if(slot<0 || slot>=ARPG_LOOT_SLOT_COUNT) { return {}; }
    if(WeaponSlot(slot)) {
        if(slot==ARPG_SLOT_HOOKSHOT || slot==ARPG_SLOT_BOOMERANG || slot==ARPG_SLOT_NUTS || slot>=ARPG_SLOT_LENS) {
            int rank=Gem(material)?(material-1)/GemKinds+1:1;
            int kind=Gem(material)?(material-1)%GemKinds:material-RuneFirst;
            if(!SocketMaterial(material))return {};
            int ids[]={ARPG_MOD_CONTROL,ARPG_MOD_CONTROL,ARPG_MOD_REACH,ARPG_MOD_REACH,ARPG_MOD_CONTROL,ARPG_MOD_CONTROL};
            if(slot==ARPG_SLOT_NUTS) { ids[2]=ids[3]=ARPG_MOD_BLAST; }
            return {uint8_t(ids[kind]),int16_t(5*rank)};
        }
        int damage=RangedSlot(slot)?ARPG_MOD_ARCHERY:ARPG_MOD_BRUTAL;
        if(Gem(material)) {
            int rank=(material-1)/GemKinds+1,kind=(material-1)%GemKinds;
            int ids[]={ARPG_MOD_FLAME,ARPG_MOD_FROST,ARPG_MOD_STORM,ARPG_MOD_KEEN,damage,damage};
            return {uint8_t(ids[kind]),int16_t((kind==3?2:5)*rank)};
        }
        int ids[]={ARPG_MOD_FLAME,ARPG_MOD_KEEN,damage,damage,ARPG_MOD_STORM,ARPG_MOD_KEEN};
        return Rune(material)?ARPGItemMod{uint8_t(ids[material-RuneFirst]),int16_t(ids[material-RuneFirst]==ARPG_MOD_KEEN?3:6)}:ARPGItemMod{};
    }
    if(Gem(material)) {
        int rank=(material-1)/GemKinds+1, kind=(material-1)%GemKinds;
        static constexpr uint8_t ids[4][6]={
            {ARPG_MOD_FLAME,ARPG_MOD_FROST,ARPG_MOD_STORM,ARPG_MOD_KEEN,ARPG_MOD_MAGICAFF,ARPG_MOD_BRUTAL},
            {ARPG_MOD_VITALITY,ARPG_MOD_AEGIS,ARPG_MOD_STORM,ARPG_MOD_LUCKY,ARPG_MOD_MAGICAFF,ARPG_MOD_AEGIS},
            {ARPG_MOD_VITALITY,ARPG_MOD_FROST,ARPG_MOD_STORM,ARPG_MOD_LUCKY,ARPG_MOD_MAGICAFF,ARPG_MOD_AEGIS},
            {ARPG_MOD_VITALITY,ARPG_MOD_FROST,ARPG_MOD_STORM,ARPG_MOD_SWIFT,ARPG_MOD_MAGICAFF,ARPG_MOD_AEGIS}
        };
        int id=ids[slot][kind]; int unit=id==ARPG_MOD_AEGIS||id==ARPG_MOD_MAGICAFF?1:id==ARPG_MOD_KEEN||id==ARPG_MOD_SWIFT?2:5;
        return {uint8_t(id),int16_t(unit*rank)};
    }
    static constexpr ARPGItemMod runePowers[]={{ARPG_MOD_FLAME,6},{ARPG_MOD_SWIFT,2},{ARPG_MOD_AEGIS,1},{ARPG_MOD_MAGICAFF,1},{ARPG_MOD_STORM,10},{ARPG_MOD_KEEN,4}};
    return Rune(material)?runePowers[material-RuneFirst]:ARPGItemMod{};
}
inline int Power(const ARPGItemInstance& item,int mod) {
    if(mod<=0 || mod>=ARPG_MOD_COUNT_) { return 0; }
    int value=0;
    for(int i=0;i<std::min<int>(item.modCount,ARPG_ITEM_MOD_COUNT);++i) { if(item.mods[i].modId==mod) { value+=item.mods[i].power; } }
    for(int i=0;i<std::min<int>(item.socketCount,ARPG_ITEM_SOCKET_COUNT);++i) {
        auto bonus=SocketBonus(item.sockets[i],item.slot); if(bonus.modId==mod) { value+=bonus.power; }
    }
    if(int word=Runework(item)) { for(auto bonus:Runeworks[word].mods) { if(bonus.modId==mod) { value+=bonus.power; } } }
    return value;
}
inline int SetPieces(const ARPGSaveData& save,int set,uint8_t activeMask=15) {
    if(set<1 || set>SetCount) { return 0; }
    int count=0; for(int i=0;i<ARPG_SLOT_COUNT_;++i) { const auto& item=save.equipment[i]; count+=(activeMask&(1<<i)) && item.slot==i && Set(item) && item.setId==set; } return count;
}
inline int TotalPower(const ARPGSaveData& save,int mod,uint8_t activeMask=15) {
    int value=0; for(int slot=0;slot<ARPG_SLOT_COUNT_;++slot) { if(activeMask&(1<<slot)) { value+=Power(save.equipment[slot],mod); } }
    for(int set=1;set<=SetCount;++set) {
        int count=SetPieces(save,set,activeMask);
        if(count>=2) { for(auto bonus:Sets[set].two) { if(bonus.modId==mod) { value+=bonus.power; } } }
        if(count==4) { for(auto bonus:Sets[set].four) { if(bonus.modId==mod) { value+=bonus.power; } } }
    }
    return value;
}
struct BuildStyle { std::array<int,3> power{},recovery{}; int manaOnHit=0,lifeOnKill=0; uint8_t combatPowers=0; };
inline BuildStyle StyleBonuses(const ARPGSaveData& save,uint8_t activeMask=15) {
    BuildStyle result;
    auto add=[&](Style s) { if(s.tree>=0 && s.tree<3) { result.power[s.tree]+=s.power; result.recovery[s.tree]+=s.recovery; } result.manaOnHit+=s.manaOnHit; result.lifeOnKill+=s.lifeOnKill; if(s.combatPower!=CombatPower::None) { result.combatPowers|=1u<<(int(s.combatPower)-1); } };
    for(int i=0;i<ARPG_SLOT_COUNT_;++i) {
        if(!(activeMask&(1<<i))) { continue; } const auto& item=save.equipment[i];
        if(auto unique=Unique(item)) { add(unique->style); } if(int word=Runework(item)) { add(Runeworks[word].style); }
    }
    for(int set=1;set<=SetCount;++set) { if(SetPieces(save,set,activeMask)==4) { add(Sets[set].style); } }
    for(int i=0;i<3;++i) { result.power[i]=std::min(100,result.power[i]); result.recovery[i]=std::min(40,result.recovery[i]); }
    result.manaOnHit=std::min(4,result.manaOnHit); result.lifeOnKill=std::min(16,result.lifeOnKill); return result;
}
inline void IdentifyUnique(ARPGItemInstance& item,int id) {
    if(id<1 || id>UniqueCount || Uniques[id].slot!=item.slot) { return; }
    if(Uniques[id].requiredBase)item.baseEquip=Uniques[id].requiredBase;
    item.uniqueId=id; item.setId=0; item.rarity=ARPG_RARITY_UNIQUE; item.modCount=3;
    for(int i=0;i<3;++i) { item.mods[i]=Uniques[id].mods[i]; }
}
inline void IdentifySet(ARPGItemInstance& item,int id) {
    if(id<1 || id>SetCount || item.slot>=4) { return; }
    item.uniqueId=0; item.setId=id; item.rarity=ARPG_RARITY_UNIQUE; item.modCount=2;
    static constexpr ARPGItemMod core[3][4]={
        {{ARPG_MOD_BRUTAL,18},{ARPG_MOD_AEGIS,4},{ARPG_MOD_VITALITY,18},{ARPG_MOD_SWIFT,7}},
        {{ARPG_MOD_FLAME,25},{ARPG_MOD_KEEN,8},{ARPG_MOD_BRUTAL,18},{ARPG_MOD_SWIFT,8}},
        {{ARPG_MOD_FROST,22},{ARPG_MOD_MAGICAFF,2},{ARPG_MOD_VITALITY,20},{ARPG_MOD_LUCKY,20}}
    };
    item.mods[0]=core[id-1][item.slot]; item.mods[1]={ARPG_MOD_LUCKY,12};
    if(item.mods[0].modId==ARPG_MOD_LUCKY) { item.mods[1]={ARPG_MOD_VITALITY,12}; } item.mods[2]={};
}
inline int MaxSockets(const ARPGItemInstance& item) {
    if(!item.baseEquip || item.slot>=ARPG_LOOT_SLOT_COUNT) { return 0; }
    if(Unique(item)||Set(item)) { return 1; }
    return item.rarity==ARPG_RARITY_COMMON?(item.itemLevel>=24?3:2):2;
}
inline void RollIdentity(ARPGItemInstance& item,uint32_t& rng) {
    if(item.rarity==ARPG_RARITY_UNIQUE) {
        if(Rules::RandomRange(rng,0,99)<40 && !WeaponSlot(item.slot)) {
            int choices[SetCount],n=0; for(int i=1;i<=SetCount;++i) { if(item.itemLevel>=Sets[i].minLevel) { choices[n++]=i; } }
            if(n) { IdentifySet(item,choices[Rules::RandomRange(rng,0,n-1)]); }
        } else {
            int choices[UniqueCount],n=0; for(int i=1;i<=UniqueCount;++i) { if(item.slot==Uniques[i].slot && (!Uniques[i].requiredBase || item.baseEquip==Uniques[i].requiredBase) && item.itemLevel>=Uniques[i].minLevel) { choices[n++]=i; } }
            if(n) { IdentifyUnique(item,choices[Rules::RandomRange(rng,0,n-1)]); }
        }
    }
    if(Rules::RandomRange(rng,0,99)<(item.rarity==ARPG_RARITY_COMMON?65u:30u)) { item.socketCount=Rules::RandomRange(rng,1,std::max(1,MaxSockets(item))); }
}
// [TIME] | [INFO] | Loot.MaterialDrop | Low regions supply rough gems and common runes; higher regions unlock advanced crafting without scaling every zone to Link.
inline int RollMaterial(uint32_t& rng,int level,int tier) {
    int roll=Rules::RandomRange(rng,1,100);
    if(roll>std::clamp(8+tier*10,8,60)) { return 0; }
    if(Rules::RandomRange(rng,1,100)<=38) { return RuneFirst+Rules::RandomRange(rng,0,level>=24?5:3); }
    int rank=level>=32 && Rules::RandomRange(rng,1,100)<=15?2:level>=16 && Rules::RandomRange(rng,1,100)<=35?1:0;
    return 1+rank*GemKinds+Rules::RandomRange(rng,0,GemKinds-1);
}
}
