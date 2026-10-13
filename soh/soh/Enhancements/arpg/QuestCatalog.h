#pragma once
#include "ARPGTypes.h"
#include <array>
#include <cstdint>

namespace ARPG::Quests {
enum class State : uint8_t { Unaccepted, Active, Ready, Rewarded };
enum class Age : uint8_t { Either, Child, Adult };
enum class Contact : uint8_t { Kokiri, Ranch, Goron, Zora, Kakariko, Scientist, Carpenter, Gerudo };
enum class Kind : uint8_t { None, Hunt, Recover, Gather, Work, Milestone, Visit, Pack, Song, Delivery, Survey, Vigil };
enum class Period : uint8_t { Any, Day, Night };
// Native free-play song IDs, checked against the engine enums in QuestSystem.cpp.
enum class Tune : uint8_t { Saria=6, Epona=7, Lullaby=8, Sun=9, Storms=11 };
enum class Foe : uint8_t { Any, Plant, Spider, Bat, Lizard, Wolf, Skeleton, Tektite, Undead, Dodongo, Ice, IronKnuckle };
enum class Prop : uint8_t { Herbs, Timber, Ore, Parcel, Sapling, Beacon, Supplies, Memorial, Spring };
enum Milestone : uint32_t {
    Emerald=1<<0, Ruby=1<<1, Sapphire=1<<2, Forest=1<<3, Fire=1<<4, Water=1<<5,
    Shadow=1<<6, Spirit=1<<7, Carpenters=1<<8, Trials=1<<9, Ganondorf=1<<10,
    AllSages=Forest|Fire|Water|Shadow|Spirit,
};
struct Objective {
    Kind kind=Kind::None;
    int scene=-1;
    uint8_t count=0;
    Foe foe=Foe::Any;
    Prop prop=Prop::Parcel;
    uint32_t milestone=0;
    const char* text="";
    uint8_t detail=0; // Song, recipient contact, or Period for observation objectives.
    bool ordered=false;
};
struct Definition {
    uint8_t id, chapter, previous;
    Contact contact;
    Age age;
    uint32_t gate;
    const char *title, *offer, *thanks;
    std::array<Objective,ARPG_QUEST_OBJECTIVES> objectives;
    uint16_t xp;
    uint8_t rupees, level, rewardSlot;
};
constexpr Objective Hunt(int scene, Foe foe, int count, const char* text) {
    return {Kind::Hunt,scene,uint8_t(count),foe,Prop::Parcel,0,text};
}
constexpr Objective Pack(int scene,const char* text) { return {Kind::Pack,scene,3,Foe::Any,Prop::Parcel,0,text}; }
constexpr Objective Recover(int scene, Foe foe, int count, const char* text) {
    return {Kind::Recover,scene,uint8_t(count),foe,Prop::Parcel,0,text};
}
constexpr Objective Gather(int scene, Prop prop, const char* text) { return {Kind::Gather,scene,3,Foe::Any,prop,0,text}; }
constexpr Objective GatherAfter(int scene,Prop prop,const char* text) { auto o=Gather(scene,prop,text);o.ordered=true;return o; }
constexpr Objective Work(int scene, Prop prop, const char* text) { return {Kind::Work,scene,3,Foe::Any,prop,0,text}; }
constexpr Objective Story(uint32_t flag, const char* text) { return {Kind::Milestone,-1,1,Foe::Any,Prop::Parcel,flag,text}; }
constexpr Objective Visit(int scene, const char* text) { return {Kind::Visit,scene,1,Foe::Any,Prop::Parcel,0,text}; }
constexpr Objective Song(int scene,Tune tune,const char* text) { return {Kind::Song,scene,1,Foe::Any,Prop::Parcel,0,text,uint8_t(tune)}; }
constexpr Objective Delivery(int scene,Contact recipient,const char* text) { return {Kind::Delivery,scene,1,Foe::Any,Prop::Parcel,0,text,uint8_t(recipient)}; }
constexpr Objective Survey(int scene,Prop prop,Period period,const char* text) { return {Kind::Survey,scene,3,Foe::Any,prop,0,text,uint8_t(period)}; }
constexpr Objective Vigil(int scene,int seconds,Period period,const char* text) { return {Kind::Vigil,scene,uint8_t(seconds),Foe::Any,Prop::Beacon,0,text,uint8_t(period)}; }
inline constexpr const char* Chapters[] = {
    "Roots of Courage", "The Long Road Home", "Bread of the Mountain", "A River Remembers",
    "The Village Watch", "What the Forest Kept", "Embers of Hope", "The Thaw", "Names in the Dark",
    "Across the Divide", "Hyrule Reborn"
};
inline constexpr const char* Contacts[] = {"Kokiri villagers", "Malon", "Gorons", "Zora residents / King Zora",
    "Kakariko's cucco keeper", "Lakeside scientist", "Head carpenter", "Gerudo guards"};
inline constexpr const char* ContactPlaces[] = {"Kokiri Forest and its houses / Mido in Lost Woods", "Lon Lon Ranch (daytime, after Talon returns from the castle)", "Goron City",
    "Zora's Domain", "Kakariko Village (daytime)", "Lakeside Laboratory", "Gerudo Valley / carpenters' tent", "Gerudo's Fortress"};
// The IDs, order, and objective slots are save identities. Append instead of reordering.
inline constexpr Definition Catalog[] = {
 {1,0,0,Contact::Kokiri,Age::Child,0,"Bite Before Breakfast",
  "The Deku Babas are snapping at children on the way to the Great Deku Tree. Trim their appetites before somebody becomes breakfast!",
  "That's better. Now the only thing biting at breakfast will be us.",
  {Hunt(0x55,Foe::Plant,4,"Defeat Deku Babas in Kokiri Forest")},120,15,3,0},
 {2,0,1,Contact::Kokiri,Age::Child,0,"A Pocketful of Spring",
  "The paths have grown quiet. Gather three dewleaf bundles from the green rings in our forest; we will use them to nurse the young trees.",
  "Three bundles, still wet with dew! The seedlings have a chance now.",
  {Gather(0x55,Prop::Herbs,"Gather dewleaf bundles in Kokiri Forest")},160,15,4,3},
 {3,0,2,Contact::Kokiri,Age::Child,0,"The Heart Beneath the Bark",
  "Something is eating our guardian from within. Go inside the Great Deku Tree and find what is hurting him. We will keep watch outside.",
  "We cannot bring him back. But we can keep what he taught us alive.",
  {Story(Emerald,"Defeat Queen Gohma / earn the Kokiri Emerald")},450,25,6,1},
 {4,0,3,Contact::Kokiri,Age::Child,0,"For Every Fallen Leaf",
  "Help us plant three guardian saplings. Stand beside each marked seed bed and tend it. A new forest starts with small promises.",
  "Look at them! One day somebody will rest beneath those branches.",
  {Work(0x55,Prop::Sapling,"Plant and tend three guardian saplings")},300,30,6,2},
 {5,1,0,Contact::Ranch,Age::Either,0,"Milk Run, Monster Run",
  "Our milk cart keeps coming back with claw marks. Defeat the three marked Milk-road ambushers in Hyrule Field so the next driver can get home before dark.",
  "The next bottle of milk will taste a little less like fear.",
  {Pack(0x51,"Defeat the marked Milk-road ambusher pack")},230,25,5,3},
 {6,1,5,Contact::Ranch,Age::Either,0,"The Scarecrow's Missing Supper",
  "Those field raiders made off with six feed pouches. Recover them from defeated monsters. No wonder the horses have been chewing the fence.",
  "Thank you! And no, the scarecrow doesn't actually eat. Don't tell the horses.",
  {Recover(0x51,Foe::Any,6,"Recover feed pouches from Hyrule Field monsters")},280,30,7,1},
 {7,1,6,Contact::Ranch,Age::Either,0,"Room at the Stable",
  "Gather three bundles of sound timber on the ranch, then rebuild our roadside feed stations. Tired travelers need a place to stop, too.",
  "The stations are stocked. Rest beside them whenever the road wears you down.",
  {Gather(0x63,Prop::Timber,"Gather ranch repair timber"),Work(0x51,Prop::Supplies,"Rebuild three roadside supply stations")},360,35,8,2},
 {8,1,7,Contact::Ranch,Age::Either,0,"A Song for the Road",
  "Will you carry word that the ranch is open again? Visit Kakariko and Lake Hylia. Tell them there is still a warm welcome here.",
  "The world feels smaller when there are friends at the other end of the road.",
  {Visit(0x52,"Bring the ranch's news to Kakariko"),Visit(0x57,"Bring the ranch's news to Lake Hylia")},400,45,10,0},
 {9,2,0,Contact::Goron,Age::Child,0,"A Very Crunchy Shortage",
  "Everyone says we eat rocks. Not EVERY rock! Collect three veins of sweet red ore on Death Mountain Trail while our cavern is blocked.",
  "Now THAT is a meal. You can keep your soup.",
  {Gather(0x60,Prop::Ore,"Collect sweet red ore on Death Mountain Trail")},280,25,8,1},
 {10,2,9,Contact::Goron,Age::Child,0,"Hot Feet, Cold Dinner",
  "The cavern's Dodongos keep roasting our food before we can eat it. Defeat four Dodongos or baby Dodongos inside. Watch those tails!",
  "A little char is fine. Being the dinner is not.",
  {Hunt(0x01,Foe::Dodongo,4,"Defeat Dodongos in Dodongo's Cavern")},380,35,10,3},
 {11,2,10,Contact::Goron,Age::Child,0,"King of an Empty Table",
  "King Dodongo has claimed our only pantry. End his rule and bring back hope for the city.",
  "Tonight every Goron gets a second helping. Even you, little brother!",
  {Story(Ruby,"Defeat King Dodongo / earn the Goron Ruby")},650,45,12,0},
 {12,2,11,Contact::Goron,Age::Child,0,"Warm Lights, Full Bellies",
  "Rekindle the three trail beacons so our food carriers can find their way through the ash. Hold still at each broken brazier to repair it.",
  "Those lights say the mountain is home again.",
  {Work(0x60,Prop::Beacon,"Repair and light three mountain beacons")},420,40,12,2},
 {13,3,0,Contact::Zora,Age::Child,0,"Not a Drop to Drink",
  "Tektites are fouling the river shallows. Clear four from Zora's River before the next water bearers set out.",
  "Clear water is worth more than all the silver in the world.",
  {Hunt(0x54,Foe::Tektite,4,"Defeat Tektites in Zora's River")},300,25,9,3},
 {14,3,13,Contact::Zora,Age::Child,0,"Pearls Before Monsters",
  "The fountain creatures have swallowed our filter pearls. Recover five from them. Please wash your hands afterward.",
  "Beautiful! And only a little slimy. The filters will sing again.",
  {Recover(0x59,Foe::Any,5,"Recover filter pearls from fountain monsters")},380,30,11,1},
 {15,3,14,Contact::Zora,Age::Child,0,"The Belly of the Problem",
  "Lord Jabu-Jabu suffers, and our princess is missing. Please find the cause of his sickness, and look for Princess Ruto.",
  "Our guardian breathes easily again. We will remember your courage.",
  {Story(Sapphire,"Defeat Barinade / earn the Zora Sapphire")},700,45,14,0},
 {16,3,15,Contact::Zora,Age::Child,0,"Let the River Breathe",
  "The sickness left silt in our spring filters. Restore three marked filter stones beside the river and let clean water flow downstream.",
  "Listen. That is the river thanking you.",
  {Work(0x54,Prop::Spring,"Clean and restore three river spring filters")},450,40,14,2},
 {17,4,0,Contact::Kakariko,Age::Either,0,"Feathers in the Dark",
  "My cuccos hear things scratching in the graveyard. Clear four restless creatures there. The birds have enough nightmares already.",
  "They slept! Well, until sunrise. You can't win everything.",
  {Hunt(0x53,Foe::Any,4,"Defeat graveyard monsters (including Poes)")},300,30,10,1},
 {18,4,17,Contact::Kakariko,Age::Either,0,"No One Sleeps Outside",
  "Travelers arrive with nothing but the clothes on their backs. Gather three lost parcels in the village and rebuild three supply stations for them.",
  "Blankets, food, a place to sit. Small kindnesses hold a village together.",
  {Gather(0x52,Prop::Parcel,"Gather lost traveler parcels in Kakariko"),Work(0x52,Prop::Supplies,"Stock three village relief stations")},420,35,12,2},
 {19,4,18,Contact::Kakariko,Age::Either,0,"The Road Has Teeth",
  "The trail patrol never came back. Defeat six creatures on Death Mountain Trail and recover six patrol badges from them.",
  "I will give these to their families. Thank you for bringing them home.",
  {Hunt(0x60,Foe::Any,6,"Defeat creatures on Death Mountain Trail"),Recover(0x60,Foe::Any,6,"Recover six patrol badges from trail monsters")},520,40,15,0},
 {20,4,19,Contact::Kakariko,Age::Either,0,"Someone Will Remember",
  "Make three memorial lanterns in the graveyard. Take a quiet moment beside each stone. The dead deserve more than being counted.",
  "Now their names have a light. Come rest by the lanterns when you need one, too.",
  {Work(0x53,Prop::Memorial,"Restore three memorial lanterns in the graveyard")},480,45,16,3},
 {21,5,0,Contact::Kokiri,Age::Adult,Emerald,"Seven Years of Thorns",
  "The forest remembers your footsteps, even if we do not know your face. Defeat the three marked Thornbound stalkers in the Lost Woods so our friends can travel again.",
  "That path used to be a game. Perhaps it can be again.",
  {Pack(0x5B,"Defeat the marked Thornbound stalker pack")},700,45,19,1},
 {22,5,21,Contact::Kokiri,Age::Adult,0,"A Friend Beyond the Door",
  "Saria's song still reaches us from the temple. Please search the temple. Whatever is wrong in there has spread through our woods.",
  "We heard her song change. She is still with us, isn't she?",
  {Story(Forest,"Defeat Phantom Ganon / awaken the Forest Sage")},1200,60,22,0},
 {23,5,22,Contact::Kokiri,Age::Adult,0,"Roots Remember Rain",
  "Gather three woodland seed bundles, then plant guardian saplings in the Sacred Forest Meadow. Let the temple's shadow fall on something living.",
  "The old promise has new roots.",
  {Gather(0x5B,Prop::Herbs,"Gather woodland seed bundles in the Lost Woods"),Work(0x56,Prop::Sapling,"Plant three meadow guardian saplings")},900,50,23,2},
 {24,6,0,Contact::Goron,Age::Adult,Ruby,"Ashes of the Lunch Bell",
  "Our people were taken into the Fire Temple. Recover six meal tokens from its monsters. Every token belonged to someone who should be coming home.",
  "I know every mark on these. We will set their places at the table.",
  {Recover(0x04,Foe::Any,6,"Recover meal tokens from Fire Temple monsters")},900,50,25,1},
 {25,6,24,Contact::Goron,Age::Adult,0,"No More Sacrifices",
  "Volvagia feeds on our people. Follow Darunia into the Fire Temple and end this terror.",
  "There are no words big enough. So we will give you a Goron hug instead.",
  {Story(Fire,"Defeat Volvagia / awaken the Fire Sage")},1500,70,28,0},
 {26,6,25,Contact::Goron,Age::Adult,0,"A Hearth for the Survivors",
  "The rescued Gorons need somewhere warm and safe. Rebuild three relief stations in Goron City. We will fill them with the best rocks we have.",
  "A home is more than stone. You have shown us that.",
  {Work(0x62,Prop::Supplies,"Rebuild three Goron City relief stations")},1000,60,29,2},
 {27,7,0,Contact::Scientist,Age::Adult,Sapphire,"Cold Case at the Lake",
  "The lake is sick and the fountain is frozen. Collect three frost samples inside the Ice Cavern above Zora's Fountain. I need evidence, not another theory about angry fish.",
  "Fascinating! This ice holds a curse, not merely a chill.",
  {Gather(0x09,Prop::Ore,"Collect frost samples inside the Ice Cavern")},1000,50,27,3},
 {28,7,27,Contact::Scientist,Age::Adult,0,"The Thing Below the Surface",
  "Every current leads back to the Water Temple. Find what is disturbing the water in there. My measuring stick is becoming a walking stick.",
  "Water! Actual water! For once, being proven right feels wonderful.",
  {Story(Water,"Defeat Morpha / awaken the Water Sage")},1600,70,31,0},
 {29,7,28,Contact::Scientist,Age::Adult,0,"First Water, Then Life",
  "Restore three marked spring filters on Lake Hylia's shore. A full lake is only the beginning; now it must sustain life again.",
  "The first minnows are back. That is the best result I could ask for.",
  {Work(0x57,Prop::Spring,"Restore three lakeshore spring filters")},1100,60,32,2},
 {30,8,0,Contact::Kakariko,Age::Adult,Forest,"What the Well Kept",
  "An old letter mentions belongings lost below our well. The entrance is too narrow now, but if you find a way below Kakariko, please recover five keepsakes from its monsters.",
  "These are real names, real people. We cannot let the shadows swallow them again.",
  {Recover(0x08,Foe::Any,5,"As a child, recover keepsakes from well monsters")},1100,55,30,1},
 {31,8,30,Contact::Kakariko,Age::Adult,0,"A Drum No One Should Hear",
  "The village still trembles to a drum beneath the graveyard. Impa went to face that evil. Please follow her trail and put an end to it.",
  "For the first time in years, the silence feels kind.",
  {Story(Shadow,"Defeat Bongo Bongo / awaken the Shadow Sage")},1800,75,35,0},
 {32,8,31,Contact::Kakariko,Age::Adult,0,"Leave a Light On",
  "Repair three village beacons. I want every returning family to see a light in Kakariko, even on the darkest night.",
  "There. Nobody has to wonder whether they are welcome.",
  {Work(0x52,Prop::Beacon,"Repair and light three Kakariko beacons")},1200,65,36,2},
 {33,9,0,Contact::Carpenter,Age::Adult,0,"Measure Twice, Rescue Once",
  "My crew is locked up in the Gerudo fortress! Free all four carpenters. We'll rebuild the valley bridge as soon as they return.",
  "All four, safe! A carpenter's promise is good timber: the bridge is rebuilt.",
  {Story(Carpenters,"Rescue all four carpenters and restore the valley bridge")},1400,60,30,3},
 {34,9,33,Contact::Carpenter,Age::Adult,0,"The Last Supply Wagon",
  "The bridge is sound, but the wagons are empty. Recover six tool rolls from valley monsters, then rebuild three supply stations for the crossing.",
  "Tools, travelers, trade. Hear that? It's the sound of the valley coming back.",
  {Recover(0x5A,Foe::Any,6,"Recover tool rolls from Gerudo Valley monsters"),Work(0x5A,Prop::Supplies,"Rebuild three valley supply stations")},1300,65,33,1},
 {35,9,0,Contact::Gerudo,Age::Adult,Carpenters,"A Light the Sand Cannot Bury",
  "Our scouts disappear in the wasteland storms. Repair three signal beacons on safe ground along your crossing. The next scout may live because of you.",
  "We do not give trust easily. You have earned ours one light at a time.",
  {Work(0x5E,Prop::Beacon,"Raise three Haunted Wasteland signal beacons"),Visit(0x5C,"Reach the Desert Colossus")},1400,65,35,3},
 {36,9,35,Contact::Gerudo,Age::Adult,0,"Sisters of the Stolen Sun",
  "The witches hold power in our temple, and Nabooru has not been herself. Find the truth behind those walls. Our people deserve to choose their own future.",
  "The desert belongs to its people again. Walk here as our friend.",
  {Story(Spirit,"Defeat Twinrova / awaken the Spirit Sage")},2000,80,38,0},
 {37,10,20,Contact::Kakariko,Age::Adult,AllSages,"Five Lights, One Promise",
  "You have brought hope to so many places. Carry our promise to the forest, Gorons, and Zoras: when this ends, nobody rebuilds alone.",
  "Different people. The same promise. That is a kingdom worth saving.",
  {Visit(0x55,"Carry the promise to Kokiri Forest"),Visit(0x62,"Carry the promise to Goron City"),Visit(0x58,"Carry the promise to Zora's Domain")},1600,70,39,2},
 {38,10,37,Contact::Kakariko,Age::Adult,0,"Before the Last March",
  "Defeat eight monsters in Ganon's Castle and recover their stolen relief seals. Our final supply line must hold while you face the king.",
  "The families are ready to leave the shelters. All they need now is dawn.",
  {Hunt(0x0D,Foe::Any,8,"Defeat monsters inside Ganon's Castle"),Recover(0x0D,Foe::Any,8,"Recover relief seals from castle monsters")},1900,75,42,1},
 {39,10,38,Contact::Kakariko,Age::Adult,0,"The Sixfold Lock",
  "You say six barriers stand between you and the tower? Clear the way inside the castle, then return. We will have your last supplies ready.",
  "Take these. Every person you helped has put something into this journey.",
  {Story(Trials,"Clear all six Ganon's Castle trials")},2400,90,44,0},
 {40,10,39,Contact::Kakariko,Age::Adult,0,"A Dawn Worth Coming Home To",
  "One last promise before the tower: rebuild the three dawn beacons in Hyrule Field. Whatever happens in the castle, those lights will guide our people home.",
  "Look at what you have made possible. Go with all our hopes. Hyrule is waiting for its sunrise.",
  {Work(0x51,Prop::Beacon,"Build three dawn beacons in Hyrule Field")},3000,99,46,2},
 {41,0,4,Contact::Kokiri,Age::Child,0,"The Forest Has an Echo",
  "Try playing Saria's melody in the Lost Woods. Then look for three sunlit dewleaf patches back here. We want to learn which little plants the forest is keeping safe.",
  "Three little patches, and a song to find our way back. That is a good day's exploring!",
  {Song(0x5B,Tune::Saria,"Play Saria's Song in the Lost Woods"),Survey(0x55,Prop::Herbs,Period::Day,"Examine three forest dewleaf patches by day")},420,30,8,3},
 {42,0,41,Contact::Kokiri,Age::Child,0,"Special Delivery: Do Not Eat",
  "Gather three bundles of leaves from our marked patches and take them to the Gorons. They asked for wrapping for their rock lunches. Wrapping! Please make that part very clear.",
  "They only tasted ONE leaf? I suppose that is progress.",
  {Gather(0x55,Prop::Herbs,"Gather three bundles of lunch-wrapping leaves"),Delivery(0x62,Contact::Goron,"Hand the leaf bundles to a Goron in Goron City")},500,35,11,1},
 {43,1,5,Contact::Ranch,Age::Either,0,"Lullaby for a Light Sleeper",
  "The horses startle at every sound after dark. Play Epona's Song here, then keep a quiet twenty-second watch on the ranch at night. Let us see what is upsetting them.",
  "A flapping shutter! I will tie it down. You gave them a peaceful night, and me a good laugh.",
  {Song(0x63,Tune::Epona,"Play Epona's Song at Lon Lon Ranch"),Vigil(0x63,20,Period::Night,"Remain still and unharmed at the ranch for 20 seconds at night")},450,35,9,2},
 {44,1,43,Contact::Ranch,Age::Either,0,"Feathers, Not Sneezes",
  "Kakariko's cucco keeper cares for her birds even though their feathers make her ill. Gather three bundles of clean stable bedding, then take them to her. Tell her the horses send their regards.",
  "She says the cuccos approve? High praise. They are terribly demanding customers.",
  {Gather(0x63,Prop::Timber,"Gather clean bedding bundles at the ranch"),Delivery(0x52,Contact::Kakariko,"Give the bedding to Kakariko's cucco keeper")},520,40,12,3},
 {45,4,18,Contact::Kakariko,Age::Either,0,"A Rubbing, Not a Robbery",
  "Some old memorial stones have marks under the moss. Examine three marked stones in the graveyard by daylight, then take your rubbings to the lakeside scientist. Leave the stones where they belong!",
  "Water marks, not a treasure map. Good. I do not need treasure hunters digging up the cucco pen.",
  {Survey(0x53,Prop::Memorial,Period::Day,"Take rubbings at three graveyard survey rings by day"),Delivery(0x38,Contact::Scientist,"Show the rubbings to the lakeside scientist")},550,40,14,1},
 {46,4,45,Contact::Kakariko,Age::Either,0,"The Gravest Night Shift",
  "Now the scientist wants to know why those marks gleam after sunset. Examine three marked stones at night. If you know the Sun's Song, play it afterward; you will have earned a little daylight.",
  "Dew on pale stone. I shall tell the birds it was a very small, very damp ghost.",
  {Survey(0x53,Prop::Memorial,Period::Night,"Examine three gleaming memorial stones at night"),Song(0x53,Tune::Sun,"After the survey, play the Sun's Song in the graveyard")},650,45,16,2},
 {47,2,12,Contact::Goron,Age::Child,0,"A Rock With Rhythm",
  "Bring that forest tune to Goron City, then examine three marked red-ore seams on the trail in daylight. We are choosing a stone for a drum. Dinner can wait. Probably.",
  "Hear that lovely ring? A perfect drumstone! Now somebody put the mallet down before trying a bite.",
  {Song(0x62,Tune::Saria,"Play Saria's Song in Goron City"),Survey(0x60,Prop::Ore,Period::Day,"Test three marked drumstone seams by day")},620,45,14,0},
 {48,6,26,Contact::Goron,Age::Adult,Fire,"The Lunch Bell Rings Again",
  "We saved the old lunch bell, but its stand cracked in the heat. Inspect three marked stone piles on the trail, then shore up three bell-and-supper stands here in the city.",
  "That sound used to mean supper. Now it means everyone has a place to return to.",
  {Survey(0x60,Prop::Ore,Period::Any,"Inspect three stone piles on Death Mountain Trail"),Work(0x62,Prop::Supplies,"Rebuild three lunch-bell supper stands in Goron City")},1300,65,31,1},
 {49,3,16,Contact::Zora,Age::Child,0,"The River's Two Faces",
  "A spring can look clear at noon and cloud over after dark. Examine three marked river filters by day and three by night, then bring your notes to the scientist at Lake Hylia.",
  "He found the drifting silt! Our water bearers can clean the filters before the trouble reaches the lake.",
  {Survey(0x54,Prop::Spring,Period::Day,"Examine three river filters by day"),Survey(0x54,Prop::Spring,Period::Night,"Examine three river filters at night"),Delivery(0x38,Contact::Scientist,"Deliver the river observations to the lakeside scientist")},720,45,16,3},
 {50,7,0,Contact::Scientist,Age::Either,0,"Strictly Not for Soup",
  "I need two opinions on a water test. Take my bottle to Malon at the ranch for a clean stopper, then show it to the Zoras in their domain. No tasting! Observation comes before indigestion.",
  "A sound stopper and a Zora's report. Remarkable what research can achieve when nobody drinks the experiment.",
  {Delivery(0x63,Contact::Ranch,"Ask Malon at the ranch to seal the sample bottle"),Delivery(0x58,Contact::Zora,"Hand the sealed sample to a Zora in Zora's Domain")},700,45,17,2},
 {51,7,29,Contact::Scientist,Age::Adult,Water,"Rain Check",
  "If you know a melody that calls rain, play it beside Lake Hylia. Then inspect three marked shore filters in daylight. I want to see what the first runoff carries into our recovering lake.",
  "Silt, leaves, and one very surprised beetle. The filters caught the lot. That is a result worth getting wet for.",
  {Song(0x57,Tune::Storms,"Play the Song of Storms at Lake Hylia"),Survey(0x57,Prop::Spring,Period::Day,"Inspect three lakeshore runoff filters by day")},1400,65,34,3},
 {52,5,23,Contact::Kokiri,Age::Adult,Forest,"A Reply Without Words",
  "Could you play Saria's song in the Sacred Forest Meadow? Then listen quietly there for thirty seconds in daylight. We miss the way she made even a quiet place feel like company.",
  "Only leaves and birds? That sounds like a meadow she would want to come home to. Thank you, mister.",
  {Song(0x56,Tune::Saria,"Play Saria's Song in the Sacred Forest Meadow"),Vigil(0x56,30,Period::Day,"Listen still and unharmed in the meadow for 30 seconds by day")},1200,60,26,2},
 {53,9,34,Contact::Carpenter,Age::Adult,Carpenters,"The Bridge Inspector's Bad Day",
  "A bridge is not finished just because you can cross it! Inspect three marked timber stacks in the valley by day, get a Gerudo guard's crossing report, then repair three approach lights in the valley.",
  "Straight boards, clear signals, and a proper report. Now THAT is a finished job. Mostly. I shall inspect it once more.",
  {Survey(0x5A,Prop::Timber,Period::Day,"Inspect three valley timber stacks by day"),Delivery(0x5D,Contact::Gerudo,"Get a crossing report from a Gerudo fortress guard"),Work(0x5A,Prop::Beacon,"Repair three Gerudo Valley approach lights")},1550,70,36,0},
 {54,9,35,Contact::Gerudo,Age::Adult,Carpenters,"Reading the Dunes",
  "Our beacons tell you where to go. The sand tells you when to stop. Examine three marked signal posts in the wasteland, then hold still and unharmed for twenty seconds to watch the wind.",
  "You waited for the sand to settle instead of racing it. Good. Pride is a poor compass.",
  {Survey(0x5E,Prop::Beacon,Period::Any,"Read three wind-worn wasteland signal posts"),Vigil(0x5E,20,Period::Any,"Observe the wasteland wind still and unharmed for 20 seconds")},1550,70,37,3},
 {55,9,36,Contact::Gerudo,Age::Adult,Spirit,"Water Without a War",
  "We need clean water for our scouts. Ask the lakeside scientist which plants will protect a traveling supply, gather three bundles at the lake, and bring them to a guard here. We can ask for help without surrendering our pride.",
  "His instructions are fussy. His plants are useful. Tell him the desert remembers a fair exchange.",
  {Delivery(0x38,Contact::Scientist,"Ask the lakeside scientist for water-storage advice"),GatherAfter(0x57,Prop::Herbs,"Gather three lakeshore water-storage herb bundles"),Delivery(0x5D,Contact::Gerudo,"Deliver the water-storage bundles to a Gerudo fortress guard")},1700,75,40,1},
 {56,8,32,Contact::Kakariko,Age::Adult,Shadow,"The Quiet Watch",
  "The village is safer, but people still wake at every creak. Check three marked village lanterns at night, then keep a quiet thirty-second watch. Afterward, mend three little remembrance lights here.",
  "No alarm tonight. Just shutters, footsteps, and birds dreaming noisy dreams. I had forgotten how lovely ordinary sounds could be.",
  {Survey(0x52,Prop::Beacon,Period::Night,"Check three Kakariko lanterns at night"),Vigil(0x52,30,Period::Night,"Keep still and unharmed in Kakariko for 30 seconds at night"),Work(0x52,Prop::Memorial,"Mend three village remembrance lights")},1600,70,38,2},
 {57,1,44,Contact::Ranch,Age::Adult,Forest|Fire,"No Rocks in the Milk",
  "I want to send a welcome to the forest and the mountain. Take this invitation to a Kokiri at home, then to the Gorons. Tell them we will make room for everyone's supper. The rocks get their own table.",
  "The Kokiri sent their good wishes, and the Gorons offered to bring dinner? I had better reinforce that table.",
  {Delivery(0x55,Contact::Kokiri,"Give Malon's greeting to a Kokiri in Kokiri Forest"),Delivery(0x62,Contact::Goron,"Give Malon's supper invitation to a Goron in Goron City")},1450,65,33,1},
 {58,10,40,Contact::Kakariko,Age::Adult,AllSages,"A Table With Room for Everyone",
  "Before you leave for the tower, will you carry one last request? Ask the head carpenter in the valley for benches, the fortress guards for water jars, and Malon for supper. A gathering to hope for can keep a village going.",
  "Benches, water, supper. We will keep a place for you. You need not promise when you will return; just know you are wanted here.",
  {Delivery(0x5A,Contact::Carpenter,"Ask the head carpenter in Gerudo Valley for benches"),Delivery(0x5D,Contact::Gerudo,"Ask a Gerudo fortress guard for water jars"),Delivery(0x63,Contact::Ranch,"Ask Malon at Lon Lon Ranch about supper")},3200,99,47,2},
};
static_assert(std::size(Catalog)==ARPG_QUEST_COUNT);
inline constexpr const Definition* Find(int id) { return id>0 && id<=ARPG_QUEST_COUNT?&Catalog[id-1]:nullptr; }
inline constexpr bool Ordered(const Objective& o) { return o.ordered || o.kind==Kind::Work || o.kind==Kind::Song || o.kind==Kind::Delivery || o.kind==Kind::Survey || o.kind==Kind::Vigil; }
inline constexpr bool PeriodMatches(const Objective& o,bool night) { return (o.kind!=Kind::Survey && o.kind!=Kind::Vigil) || o.detail==uint8_t(Period::Any) || (o.detail==uint8_t(Period::Night))==night; }
inline constexpr bool Physical(const Objective& o) { return o.kind==Kind::Gather || o.kind==Kind::Work || o.kind==Kind::Survey; }
}
