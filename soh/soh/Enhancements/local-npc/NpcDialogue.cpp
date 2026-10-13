#include "NpcDialogue.h"
#ifdef SOH_LOCAL_NPC
#include "NpcClient.h"
#include "DialogueText.h"
#include <functional>
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/arpg/ARPG.h"
#include "soh/Enhancements/arpg/QuestSystem.h"
#include "soh/Enhancements/arpg/ARPGTheme.h"
#include "soh/Enhancements/arpg/ARPGSave.h"
#include "soh/Enhancements/arpg/EnemyRegions.h"
#include "soh/SohGui/UIWidgets.hpp"
#include <libultraship/libultraship.h>
#include <SDL.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <spdlog/spdlog.h>

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
extern PlayState* gPlayState;
extern float sFontWidths[144];
void Player_StartTalking(PlayState*, Actor*);
}
std::string Message_TTS_Decode(uint8_t*, uint16_t, uint16_t);

namespace LocalNpc {
namespace {
struct Profile { int actor; const char *id, *name, *persona, *delivery; };
// Keep these identities grounded in the actor table. Generic residents do not acquire a made-up name or quest.
const Profile profiles[] = {
    {ACTOR_EN_SA, "saria", "Saria", "Saria, Link's kind Kokiri friend. You love the forest and music.", "gentle, bright, friendly"},
    {ACTOR_EN_MD, "mido", "Mido", "Mido, a proud Kokiri boy. Blunt and competitive, but protective of the forest.", "youthful, brisk, a little stubborn"},
    {ACTOR_EN_KO, "kokiri", "Kokiri villager", "A Kokiri forest villager. Curious, playful and attached to home.", "light, friendly, curious"},
    {ACTOR_EN_MA1, "malon", "Malon", "Malon, Talon's daughter at Lon Lon Ranch. Warm, practical, fond of horses and singing.", "warm, clear, cheerful"},
    {ACTOR_EN_MA2, "malon", "Malon", "Malon of Lon Lon Ranch. Warm, practical and protective of the horses.", "warm, clear, gentle"},
    {ACTOR_EN_MA3, "malon", "Malon", "Malon of Lon Lon Ranch. Warm, practical and protective of the horses.", "warm, clear, gentle"},
    {ACTOR_EN_TA, "talon", "Talon", "Talon, Malon's sleepy, good-natured father, a rancher who knows horses and cuccos.", "relaxed, rounded, sleepy, kindly"},
    {ACTOR_EN_IN, "ingo", "Ingo", "Ingo, a prickly worker at Lon Lon Ranch. Proud of your hard work; brusque but practical.", "nasal, gruff, impatient"},
    {ACTOR_EN_DU, "darunia", "Darunia", "Darunia, proud leader of the Gorons. Hearty, direct, protective of your people.", "deep, resonant, hearty"},
    {ACTOR_EN_GO2, "goron", "Goron", "A Goron of Death Mountain. Friendly and sturdy; you know rocks and mountain life.", "deep, slow, good-natured"},
    {ACTOR_EN_ZO, "zora", "Zora", "A Zora resident. Courteous and concerned for the waters and your people.", "smooth, measured, courteous"},
    {ACTOR_EN_RU1, "ruto", "Princess Ruto", "Ruto, princess of the Zoras. Proud, outspoken and brave.", "clear, proud, assertive"},
    {ACTOR_EN_RU2, "ruto", "Princess Ruto", "Ruto, princess of the Zoras. Proud, outspoken and brave.", "clear, proud, assertive"},
    {ACTOR_EN_NIW_LADY, "anju", "Cucco keeper", "The cucco keeper in Kakariko Village. Kindly, worried about your birds, practical.", "soft, friendly, slightly worried"},
    {ACTOR_EN_HEISHI2, "guard", "Hyrule guard", "A Hyrule guard on duty. Direct and alert; speak only about things you know here.", "firm, clear, measured"},
    {ACTOR_EN_HEISHI3, "guard", "Hyrule guard", "A Hyrule guard on duty. Direct and alert; speak only about things you know here.", "firm, clear, measured"},
    {ACTOR_EN_HEISHI4, "guard", "Hyrule guard", "A Hyrule guard on duty. Direct and alert; speak only about things you know here.", "firm, clear, measured"},
    {ACTOR_EN_HY, "hylian", "Hylian villager", "A Hylian villager. Everyday concerns, local knowledge, no secret royal information.", "natural, conversational, friendly"},
    {ACTOR_EN_KZ, "king_zora", "King Zora", "Dignified ruler of the Zoras and concerned father of Princess Ruto. Knows his people and the waters, not hidden temple secrets.", "Speak with measured warmth and stately concern."},
    {ACTOR_EN_MK, "lakeside_scientist", "Lakeside scientist", "The elderly scientist at Lake Hylia's laboratory. Curious, practical, and fascinated by water and wildlife.", "thoughtful, lively, scholarly"},
    {ACTOR_EN_TORYO, "carpenter", "Head carpenter", "The head carpenter of Gerudo Valley. Gruff, loyal to your crew, and proud of building things that last.", "gruff, hearty, direct"},
    {ACTOR_EN_GE1, "gerudo", "Gerudo guard", "A Gerudo guard. Proud, watchful and concise. Protect your people's privacy.", "confident, steady, direct"},
    {ACTOR_EN_OSSAN, "shopkeeper", "Shopkeeper", "A Hyrule shopkeeper. Friendly and businesslike. Actual prices and purchases use the shop menu.", "lively, welcoming, clear"},
};
const Profile* FindProfile(Actor* actor) {
    if (!actor) return nullptr;
    for (const auto& profile : profiles) if (actor->id == profile.actor) return &profile;
    return nullptr;
}
std::unique_ptr<Client> client;
SDL_AudioDeviceID device = 0;
bool audioInitialized = false;
SDL_AudioSpec outputSpec{};
uint64_t activeSerial = 0;
bool open = false, busy = false, nativePagePending = false, focusInput = false;
bool composing = false, mouseCaptured = false, ownsPlayer = false;
Actor* followupSpeaker = nullptr;
int followupFrames = 0;
constexpr uint16_t conversationTextId = 0xF700;
enum class Screen { Menu, Reply, Waiting, Composer };
Screen screen = Screen::Menu;
struct Option { std::string label; std::function<void()> select; };
std::vector<Option> options, visibleOptions;
std::vector<std::string> replyPages;
size_t replyPage = 0, menuPage = 0;
std::string encodedMessage, menuTitle, lastReply;
std::function<void()> afterReply;
void Hub();
void ShowReply(const std::string&, std::function<void()> = Hub, bool = true);
void ShowMenu(const std::string&, std::vector<Option>);
void ShowMenuPage();
void Compose();
void Ask(const std::string&);
void QuestList();
void QuestMenu(int);
bool OwnsTextbox() { return open && gPlayState && gPlayState->msgCtx.textId == conversationTextId; }
float GlyphWidth(unsigned char c) { return int(sFontWidths[c - ' '] * .75f); }
Actor *speaker = nullptr, *nativeSpeaker = nullptr;
std::string status, authoredText;
std::vector<Turn> transcript;
char input[601]{};
enum class Mode { None, Conversation, Native, Preview };
Mode mode = Mode::None;
constexpr int32_t inputBlock = 0x4E504344;

bool Enabled() { return CVarGetInteger(CVAR_ENHANCEMENT("LocalNpc.Enabled"), 1); }
bool Loaded() { return gPlayState && GameInteractor::IsSaveLoaded(true); }
const char* Env(const char* name, const char* fallback = "") {
    const char* value = std::getenv(name); return value && *value ? value : fallback;
}
Provider Settings(const Profile* profile) {
    static const Provider defaults = LoadProviderDefaults({
        Env("SOH_NPC_CONFIG"),
        Ship::Context::GetPathRelativeToAppDirectory("npc-services.local.json"),
        "npc-services.local.json",
    });
    Provider p = defaults;
    auto setting = [](const char* cvar, const char* fallback) {
        const char* value = CVarGetString(cvar, "");
        return std::string(value && *value ? value : fallback);
    };
    p.endpoint = setting(CVAR_ENHANCEMENT("LocalNpc.Endpoint"), Env("SOH_NPC_LLM_ENDPOINT", Env("CHAT_API_URL", defaults.endpoint.c_str())));
    p.model = setting(CVAR_ENHANCEMENT("LocalNpc.Model"), Env("SOH_NPC_LLM_MODEL", Env("CHAT_MODEL", defaults.model.c_str())));
    p.key = Env("SOH_NPC_LLM_API_KEY", Env("CHAT_API_KEY", defaults.key.c_str()));
    p.ttsEndpoint = setting(CVAR_ENHANCEMENT("LocalNpc.TtsEndpoint"), Env("SOH_NPC_TTS_ENDPOINT", defaults.ttsEndpoint.c_str()));
    p.voice = CVarGetString(CVAR_ENHANCEMENT("LocalNpc.DefaultVoice"), "");
    if (profile) {
        const auto key = std::string(CVAR_ENHANCEMENT("LocalNpc.Voice.")) + profile->id;
        p.voice = CVarGetString(key.c_str(), p.voice.c_str());
        p.instruction = profile->delivery;
    }
    p.speak = CVarGetInteger(CVAR_ENHANCEMENT("LocalNpc.Voices"), 1);
    return p;
}
void StopAudio() { if (device) SDL_ClearQueuedAudio(device); }
void Cancel() {
    if (client) activeSerial = client->Cancel();
    StopAudio(); busy = false; mode = Mode::None;
}
void EndComposer() {
    if (!composing) return;
    composing = false;
    auto* context = Ship::Context::GetRawInstance();
    context->GetControlDeck()->UnblockGameInput(inputBlock);
    context->GetWindow()->SetMouseCapture(mouseCaptured);
    context->GetWindow()->SetCursorVisibility(!mouseCaptured);
}
void Close() {
    EndComposer();
    Cancel();
    if (OwnsTextbox() && gPlayState->msgCtx.msgMode != MSGMODE_NONE) Message_CloseTextbox(gPlayState);
    // Never release a cutscene started by the campaign after our conversation.
    if (ownsPlayer && Loaded() && GET_PLAYER(gPlayState)->csAction == PLAYER_CSACTION_8 &&
        GET_PLAYER(gPlayState)->csActor == speaker)
        Player_SetCsActionWithHaltedActors(gPlayState, nullptr, PLAYER_CSACTION_7);
    ownsPlayer = false; open = false; followupSpeaker = nullptr;
    options.clear(); visibleOptions.clear(); replyPages.clear(); afterReply = {};
}
void Reset() {
    Close(); speaker = nativeSpeaker = nullptr; nativePagePending = false;
    authoredText.clear(); transcript.clear(); lastReply.clear(); status.clear(); input[0] = 0;
}
void Submit(Request request, Mode nextMode) {
    StopAudio();
    try {
        if (!client) client = std::make_unique<Client>();
        activeSerial = client->Submit(std::move(request));
        mode = nextMode; busy = true;
    } catch (const std::exception&) { status = "Could not start the dialogue service."; busy = false; }
}
bool Nearby(Actor* actor, float range = 300.0f) {
    if (!Loaded() || !actor || !actor->update || !actor->draw || actor->freezeTimer > 0) return false;
    auto* player = GET_PLAYER(gPlayState);
    if (!player || (actor->room != -1 && actor->room != gPlayState->roomCtx.curRoom.num)) return false;
    const auto& a = actor->world.pos; const auto& b = player->actor.world.pos;
    return std::hypot(a.x - b.x, a.z - b.z) < range && std::abs(a.y - b.y) < 130.0f;
}
bool InSight(Actor* actor) {
    Vec3f a = GET_PLAYER(gPlayState)->actor.world.pos, b = actor->world.pos, hit{};
    a.y += 40; b.y += 40;
    CollisionPoly* poly = nullptr; int bg = BGCHECK_SCENE;
    return !BgCheck_EntityLineTest1(&gPlayState->colCtx, &a, &b, &hit, &poly, true, true, true, true, &bg);
}
Actor* Nearest() {
    if (!Loaded() || gPlayState->pauseCtx.state || !GameInteractor::IsPlayerInControl()) return nullptr;
    Actor* best = nullptr; float distance = 220;
    auto* player = GET_PLAYER(gPlayState);
    if (!player) return nullptr;
    for (int category = 0; category < ACTORCAT_MAX; ++category) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor; actor = actor->next) {
            if (!FindProfile(actor) || !Nearby(actor, distance)) continue;
            if (!InSight(actor)) continue;
            distance = std::hypot(player->actor.world.pos.x - actor->world.pos.x,
                                  player->actor.world.pos.z - actor->world.pos.z); best = actor;
        }
    }
    return best;
}
std::string Context() {
    std::string place;
    switch (gPlayState->sceneNum) {
        case SCENE_KOKIRI_FOREST: place = "Kokiri Forest"; break;
        case SCENE_KAKARIKO_VILLAGE: place = "Kakariko Village"; break;
        case SCENE_LON_LON_RANCH: case SCENE_LON_LON_BUILDINGS: place = "Lon Lon Ranch"; break;
        case SCENE_ZORAS_DOMAIN: place = "Zora's Domain"; break;
        case SCENE_GORON_CITY: place = "Goron City"; break;
        case SCENE_MARKET_DAY: case SCENE_MARKET_NIGHT: place = "Hyrule Castle Town market"; break;
        case SCENE_MARKET_RUINS: place = "ruins of Hyrule Castle Town"; break;
        default: {
            const auto region = ARPG::Enemies::RegionFor(gPlayState->sceneNum, LINK_IS_ADULT);
            place = std::string(region.name) == "Sanctuary" ? "an interior in Hyrule; exact location unspecified" : region.name;
        }
    }
    std::ostringstream out;
    out << "Place: " << place << ". Link is " << (LINK_IS_ADULT ? "an adult" : "a child")
        << ". It is " << (gSaveContext.nightFlag ? "night" : "day") << ".";
    if (ARPG::Enabled()) out << " Adventure level: " << int(gARPGSave.level) << ".";
    if (!authoredText.empty()) out << "\nYour most recent authored dialogue page: " << authoredText;
    out << ARPG::Quests::DialogueContext(speaker);
    out << "\nNo quest progress or promises beyond these facts are known. Reply in English.";
    return out.str();
}
void Present(const std::string& encoded) {
    encodedMessage = encoded;
    const bool continuing = OwnsTextbox() && gPlayState->msgCtx.msgMode != MSGMODE_NONE;
    if (continuing) Message_ContinueTextbox(gPlayState, conversationTextId);
    else Message_StartTextbox(gPlayState, conversationTextId, nullptr);
    // A custom conversation must not drive an NPC's original event/choice state.
    gPlayState->msgCtx.talkActor = nullptr;
    gPlayState->msgCtx.choiceIndex = 0;
}
void Speak(const std::string& text) {
    Request request; request.provider = Settings(FindProfile(speaker)); request.text = text;
    if (request.provider.speak && !CVarGetInteger(CVAR_SETTING("A11yTTS"), 0)) {
        status = "Preparing voice..."; Submit(std::move(request), Mode::Conversation);
    }
}
void Remember(const std::string& role, const std::string& text) {
    transcript.push_back({role, text});
    if (transcript.size() > 16) transcript.erase(transcript.begin(), transcript.end() - 16);
}
void ShowReply(const std::string& text, std::function<void()> next, bool voice) {
    EndComposer();
    screen = Screen::Reply; replyPage = 0; afterReply = std::move(next); lastReply = text;
    replyPages = DialogueText::Pages(FindProfile(speaker)->name, text, GlyphWidth);
    Present(replyPages.front());
    if (voice) { Cancel(); status.clear(); Speak(text); }
}
void ShowMenuPage() {
    visibleOptions.clear();
    const size_t count = options.size() <= 3 ? options.size() : 2;
    const size_t start = menuPage * 2;
    for (size_t i = start; i < std::min(start + count, options.size()); ++i) visibleOptions.push_back(options[i]);
    if (options.size() > 3) visibleOptions.push_back({"More topics...", [] {
        menuPage = (menuPage + 1) % ((options.size() + 1) / 2); ShowMenuPage();
    }});
    // The native renderer supports two or three choices.
    if (visibleOptions.size() == 1) visibleOptions.push_back({"Back", Hub});
    std::vector<std::string> labels;
    for (const auto& option : visibleOptions) labels.push_back(option.label);
    Present(DialogueText::Choices(menuTitle, labels, GlyphWidth));
}
void ShowMenu(const std::string& title, std::vector<Option> next) {
    EndComposer(); Cancel(); status.clear(); screen = Screen::Menu;
    menuTitle = title; options = std::move(next); menuPage = 0; ShowMenuPage();
}
void QuestMenu(int id) {
    std::vector<Option> choices;
    for (const auto& option : ARPG::Quests::ConversationOptions(speaker, id)) choices.push_back({option.label, [option, id] {
        Remember("user", option.label);
        auto text = ARPG::Quests::ChooseConversationOption(speaker, option);
        Remember("assistant", text);
        ShowReply(text, [id] { QuestMenu(id); });
    }});
    choices.push_back({"Other local quests", QuestList});
    choices.push_back({"Back to conversation", Hub});
    ShowMenu(ARPG::Quests::Find(id)->title, std::move(choices));
}
void QuestList() {
    auto quests = ARPG::Quests::ConversationQuests(speaker);
    if (quests.empty()) { ShowReply("There are no new requests for you here right now."); return; }
    std::vector<Option> choices;
    for (int id : quests) choices.push_back({ARPG::Quests::Find(id)->title, [id] {
        auto opts = ARPG::Quests::ConversationOptions(speaker, id);
        auto about = std::find_if(opts.begin(), opts.end(), [](const auto& o) { return o.action == ARPG::Quests::TalkAction::About; });
        if (about != opts.end()) ShowReply(ARPG::Quests::ChooseConversationOption(speaker, *about), [id] { QuestMenu(id); });
        else QuestMenu(id);
    }});
    choices.push_back({"Back to conversation", Hub});
    ShowMenu("Local quests", std::move(choices));
}
void Topic(bool advice) {
    const std::string question = advice ? "Any advice?" : "How are things here?";
    if (ARPG::Quests::HasQuests(speaker)) {
        Remember("user", question);
        auto text = ARPG::Quests::AuthoredReply(speaker, advice);
        Remember("assistant", text); ShowReply(text);
    } else Ask(question);
}
void History() {
    std::vector<Option> choices;
    for (const auto& turn : transcript) choices.push_back({(turn.role == "user" ? "Link: " : "") + turn.text, [turn] {
        // Reading history must preserve who said the line and the NPC's most
        // recent spoken reply for replay.
        screen = Screen::Reply; replyPage = 0; afterReply = History;
        replyPages = DialogueText::Pages(turn.role == "user" ? "Link" : FindProfile(speaker)->name, turn.text, GlyphWidth);
        Present(replyPages.front());
    }});
    choices.push_back({"Back", Hub});
    ShowMenu("Conversation history", std::move(choices));
}
void Hub() {
    std::vector<Option> choices = {{"How are things here?", [] { Topic(false); }}};
    if (ARPG::Quests::HasQuests(speaker)) choices.push_back({"Local quests", QuestList});
    choices.push_back({"Any advice?", [] { Topic(true); }});
    choices.push_back({"Ask a question [F7]", Compose});
    if (!transcript.empty()) choices.push_back({"Conversation history", History});
    if (!lastReply.empty()) choices.push_back({"Hear that again", [] { ShowReply(lastReply); }});
    choices.push_back({"Stop voice", [] { Cancel(); status.clear(); }});
    choices.push_back({"Goodbye", Close});
    ShowMenu(FindProfile(speaker)->name, std::move(choices));
}
void Compose() {
    if (composing) return;
    Cancel(); status.clear(); screen = Screen::Composer;
    Present(DialogueText::Pages(FindProfile(speaker)->name, "What would you like to ask?", GlyphWidth).front());
    composing = true; focusInput = true; input[0] = 0;
    auto* context = Ship::Context::GetRawInstance();
    mouseCaptured = context->GetWindow()->IsMouseCaptured();
    context->GetControlDeck()->BlockGameInput(inputBlock);
    if (auto menu = context->GetWindow()->GetGui()->GetMenu()) menu->Hide();
    context->GetWindow()->SetMouseCapture(false); context->GetWindow()->SetCursorVisibility(true);
}
void Ask(const std::string& text) {
    // Copy before clearing the input buffer (the caller may pass that buffer).
    const std::string question = text;
    if (question.find_first_not_of(" \t\r\n") == std::string::npos || !Nearby(speaker)) return;
    Cancel(); EndComposer();
    Request request;
    request.provider = Settings(FindProfile(speaker)); request.persona = FindProfile(speaker)->persona;
    request.provider.speak &= !CVarGetInteger(CVAR_SETTING("A11yTTS"), 0);
    request.context = Context(); request.history = transcript; request.playerText = question;
    if (request.provider.endpoint.empty() || request.provider.model.empty()) {
        ShowReply("Free questions need a dialogue service. You can still discuss local quests. Configure the endpoint and model in Settings & testing > NPC dialogue.", Hub, false); return;
    }
    Remember("user", question); input[0] = 0;
    screen = Screen::Waiting; status = "Thinking...";
    Present(DialogueText::Pages(FindProfile(speaker)->name, "Let me think...", GlyphWidth).front());
    Submit(std::move(request), Mode::Conversation);
    if (!busy) ShowReply(status, Hub, false);
}
bool Play(const std::vector<uint8_t>& wav) {
    SDL_AudioSpec source{}; Uint8* raw = nullptr; Uint32 length = 0;
    if (!SDL_LoadWAV_RW(SDL_RWFromConstMem(wav.data(), static_cast<int>(wav.size())), 1, &source, &raw, &length)) return false;
    std::unique_ptr<Uint8, decltype(&SDL_FreeWAV)> owned(raw, SDL_FreeWAV);
    if (source.freq < 8000 || source.freq > 96000 || source.channels < 1 || source.channels > 2 ||
        length == 0 || length > 16 * 1024 * 1024 ||
        length / (double(source.freq) * source.channels * (SDL_AUDIO_BITSIZE(source.format)/8)) > 90) return false;
    if (!device) {
        // CoreAudio/WASAPI game backends need not have initialized SDL's audio subsystem.
        if (!audioInitialized) {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
                SPDLOG_WARN("[LocalNpc] Audio initialization failed: {}", SDL_GetError()); return false;
            }
            audioInitialized = true;
        }
        SDL_AudioSpec wanted{}; wanted.freq = 48000; wanted.format = AUDIO_F32SYS; wanted.channels = 2; wanted.samples = 1024;
        device = SDL_OpenAudioDevice(nullptr, 0, &wanted, &outputSpec, 0);
        if (!device) { SPDLOG_WARN("[LocalNpc] Voice device unavailable: {}", SDL_GetError()); return false; }
    }
    SDL_AudioCVT conversion{};
    if (SDL_BuildAudioCVT(&conversion, source.format, source.channels, source.freq,
                         outputSpec.format, outputSpec.channels, outputSpec.freq) < 0) return false;
    const size_t allocation = size_t(length) * conversion.len_mult;
    if (allocation > 64 * 1024 * 1024) return false;
    std::vector<uint8_t> converted(allocation);
    std::copy(raw, raw + length, converted.begin());
    conversion.buf = converted.data(); conversion.len = static_cast<int>(length);
    if (SDL_ConvertAudio(&conversion) < 0) return false;
    const float volume = std::clamp(CVarGetFloat(CVAR_ENHANCEMENT("LocalNpc.Volume"), 0.8f), 0.0f, 1.0f);
    for (int i = 0; i + 4 <= conversion.len_cvt; i += 4) {
        float sample; std::memcpy(&sample, converted.data() + i, 4);
        sample = std::isfinite(sample) ? std::clamp(sample * volume, -1.0f, 1.0f) : 0;
        std::memcpy(converted.data() + i, &sample, 4);
    }
    StopAudio();
    if (SDL_QueueAudio(device, converted.data(), conversion.len_cvt)) return false;
    SDL_PauseAudioDevice(device, 0);
    SPDLOG_INFO("[LocalNpc] Queued voice: {} bytes at {} Hz, {} channels", conversion.len_cvt, outputSpec.freq, outputSpec.channels);
    return true;
}
void Update() {
    if (!Enabled()) { if (open || speaker || nativeSpeaker || followupSpeaker || mode != Mode::None) Reset(); return; }
    if (mode != Mode::Preview && (!Loaded() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF || gSaveContext.health <= 0)) {
        if (speaker || nativeSpeaker || open || busy) Reset(); return;
    }
    if (open && (!Nearby(speaker) || gPlayState->pauseCtx.state ||
                 (gPlayState->msgCtx.msgMode != MSGMODE_NONE && !OwnsTextbox()))) Close();
    if (open && gPlayState->msgCtx.msgMode == MSGMODE_NONE) Close();
    if (open) Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_A_HEARTS_MAGIC_FORCE);
    if (followupSpeaker && Loaded() && gPlayState->csCtx.state != 0) followupSpeaker = nullptr;
    if (followupSpeaker && Loaded() && gPlayState->msgCtx.msgMode == MSGMODE_NONE) {
        if (++followupFrames > 80 || !Nearby(followupSpeaker, 220)) followupSpeaker = nullptr;
        else if (GameInteractor::IsPlayerInControl()) {
            auto* actor = followupSpeaker; followupSpeaker = nullptr;
            OpenConversation(actor);
        }
    }
    if (mode == Mode::Native && (!Loaded() || gPlayState->msgCtx.msgMode == MSGMODE_NONE ||
                               gPlayState->msgCtx.msgMode == MSGMODE_TEXT_CLOSING)) Cancel();
    if (!CVarGetInteger(CVAR_ENHANCEMENT("LocalNpc.Voices"), 1)) StopAudio();
    if (!client) return;
    for (auto& result : client->Poll()) {
        if (result.serial != activeSerial) continue;
        if (!result.text.empty() && mode == Mode::Conversation && open) {
            Remember("assistant", result.text); ShowReply(result.text, Hub, false);
        }
        status = result.status;
        if (result.wav && CVarGetInteger(CVAR_ENHANCEMENT("LocalNpc.Voices"), 1))
            status = Play(*result.wav) ? "Speaking..." : "Voice playback unavailable. You can still read the dialogue.";
        if (result.done) {
            busy = false;
            if (open && screen == Screen::Waiting) ShowReply(status.empty() ? "I could not find the words. Please try again." : status, Hub, false);
        }
    }
    if (!busy && device && status == "Speaking..." && SDL_GetQueuedAudioSize(device) == 0) status.clear();
}
void NativeDialogue() {
    if (!Enabled() || !Loaded()) return;
    auto& message = gPlayState->msgCtx;
    if (OwnsTextbox()) {
        if (composing) return;
        auto& controls = gPlayState->state.input[0];
        if (controls.press.button & BTN_R) {
            Cancel(); status.clear();
            if (screen == Screen::Waiting) Hub();
            controls.press.button &= ~BTN_R;
        }
        const bool back = (controls.press.button & BTN_B) != 0;
        if (back) {
            if (screen == Screen::Waiting) { Cancel(); Hub(); }
            else Close();
        } else if (message.msgMode == MSGMODE_TEXT_DONE && (controls.press.button & (BTN_A | BTN_CUP))) {
            if (screen == Screen::Menu) {
                const auto choice = message.choiceIndex;
                // The callback may rebuild the menu and destroy its old entries.
                if (choice < visibleOptions.size()) { auto select = visibleOptions[choice].select; select(); }
            } else if (screen == Screen::Reply) {
                if (++replyPage < replyPages.size()) Present(replyPages[replyPage]);
                else { auto next = afterReply; if (next) next(); else Hub(); }
            }
        } else return;
        // Do not let the same press also close/advance the message just installed.
        controls.press.button &= ~(BTN_A | BTN_B | BTN_CUP);
        controls.cur.button &= ~(BTN_A | BTN_B | BTN_CUP);
        return;
    }
    if (message.msgMode == MSGMODE_TEXT_START) {
        if (open) Close();
        nativeSpeaker = message.talkActor;
        nativePagePending = true;
        auto* player = GET_PLAYER(gPlayState);
        // Only ordinary NPC conversations continue into topics. Cutscene, item,
        // sign and ocarina messages keep their complete original lifecycle.
        followupSpeaker = FindProfile(nativeSpeaker) && (player->stateFlags1 & PLAYER_STATE1_TALKING) &&
            gPlayState->csCtx.state == 0 ? nativeSpeaker : nullptr;
        followupFrames = 0;
    }
    if (message.msgMode == MSGMODE_NONE || message.msgMode == MSGMODE_TEXT_CLOSING) {
        nativePagePending = false;
        if (mode == Mode::Native) Cancel();
        return;
    }
    if (message.msgMode == MSGMODE_TEXT_DONE && message.textboxEndType == TEXTBOX_ENDTYPE_DEFAULT &&
        (gPlayState->state.input[0].press.button & BTN_B)) followupSpeaker = nullptr;
    if (message.msgMode == MSGMODE_TEXT_NEXT_MSG ||
        (message.msgMode == MSGMODE_TEXT_CONTINUING && message.stateTimer == 1)) {
        nativePagePending = true;
        if (mode == Mode::Native) Cancel();
    }
    if (!nativePagePending || !(message.msgMode == MSGMODE_TEXT_DISPLAYING ||
        message.msgMode == MSGMODE_TEXT_AWAIT_NEXT || message.msgMode == MSGMODE_TEXT_DONE)) return;
    nativePagePending = false;
    const auto* profile = FindProfile(message.talkActor);
    if (!profile || gSaveContext.language == LANGUAGE_JPN) return;
    if (speaker != message.talkActor) { transcript.clear(); authoredText.clear(); lastReply.clear(); }
    speaker = nativeSpeaker = message.talkActor;
    const auto length = static_cast<uint16_t>(std::min<size_t>(message.decodedTextLen, sizeof(message.msgBufDecoded)));
    authoredText = Message_TTS_Decode(message.msgBufDecoded, 0, length);
    if (authoredText.empty()) return;
    Remember("assistant", authoredText); lastReply = authoredText;
    if (!CVarGetInteger(CVAR_ENHANCEMENT("LocalNpc.ReadScripted"), 1) ||
        CVarGetInteger(CVAR_SETTING("A11yTTS"), 0)) return;
    Request request; request.provider = Settings(profile); request.text = authoredText;
    request.provider.language = gSaveContext.language == LANGUAGE_FRA ? "fr" : gSaveContext.language == LANGUAGE_GER ? "de" : "en";
    if (request.provider.speak) { status = "Preparing voice..."; Submit(std::move(request), Mode::Native); }
}
bool TextSetting(const char* label, const char* key, const std::string& value) {
    char buffer[512]; std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    if (!ImGui::InputText(label, buffer, sizeof(buffer))) return false;
    CVarSetString(key, buffer);
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    return true;
}
}

void DrawSettings() {
    if (!ImGui::CollapsingHeader("NPC dialogue")) return;
    bool changed = UIWidgets::CVarCheckbox("Unified NPC conversations", CVAR_ENHANCEMENT("LocalNpc.Enabled"), {.defaultValue = true});
    changed |= UIWidgets::CVarCheckbox("NPC voices", CVAR_ENHANCEMENT("LocalNpc.Voices"), {.defaultValue = true});
    changed |= UIWidgets::CVarCheckbox("Voice original NPC dialogue", CVAR_ENHANCEMENT("LocalNpc.ReadScripted"), {.defaultValue = true});
    float volume = CVarGetFloat(CVAR_ENHANCEMENT("LocalNpc.Volume"), 0.8f);
    if (ImGui::SliderFloat("Voice volume", &volume, 0, 1, "%.2f", ImGuiSliderFlags_None)) {
        CVarSetFloat(CVAR_ENHANCEMENT("LocalNpc.Volume"), volume);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    auto settings = Settings(nullptr);
    changed |= TextSetting("LLM endpoint", CVAR_ENHANCEMENT("LocalNpc.Endpoint"), settings.endpoint);
    changed |= TextSetting("LLM model", CVAR_ENHANCEMENT("LocalNpc.Model"), settings.model);
    changed |= TextSetting("TTS endpoint", CVAR_ENHANCEMENT("LocalNpc.TtsEndpoint"), settings.ttsEndpoint);
    changed |= TextSetting("Default saved voice", CVAR_ENHANCEMENT("LocalNpc.DefaultVoice"), settings.voice);
    ImGui::TextWrapped("Leave the voice blank for the model's built-in voice. Saved voice names come from your voice server.");
    static int selected = 0;
    if (ImGui::BeginCombo("Character voice", profiles[selected].name)) {
        for (int i = 0; i < static_cast<int>(std::size(profiles)); ++i) {
            if (i > 0 && std::string(profiles[i].id) == profiles[i-1].id) continue;
            if (ImGui::Selectable(profiles[i].name, selected == i)) selected = i;
        }
        ImGui::EndCombo();
    }
    const auto& profile = profiles[selected];
    const std::string voiceKey = std::string(CVAR_ENHANCEMENT("LocalNpc.Voice.")) + profile.id;
    changed |= TextSetting("Saved voice for character", voiceKey.c_str(), Settings(&profile).voice);
    if (changed) { Cancel(); status = "Settings updated."; }
    ImGui::BeginDisabled(!Enabled());
    if (ImGui::Button("Preview character voice")) {
        Request r; r.provider = Settings(&profile); r.text = "Welcome, traveler. Take a moment to rest before your journey.";
        if (r.provider.speak) { status = "Preparing voice..."; Submit(std::move(r), Mode::Preview); }
    }
    ImGui::SameLine(); if (ImGui::Button("Stop voice")) { Cancel(); status.clear(); }
    ImGui::EndDisabled();
    ImGui::TextWrapped("Talk normally to continue into local topics after the story dialogue. F7 talks nearby or opens question entry during a custom conversation. Use the normal game controls for pages and choices; B or Escape leaves custom dialogue.");
    if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
}

bool OpenConversation(Actor* actor) {
    if (!Enabled() || !Loaded() || !FindProfile(actor) || !Nearby(actor, 220) || !InSight(actor) ||
        !GameInteractor::IsPlayerInControl() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) return false;
    Cancel();
    if (speaker != actor) { transcript.clear(); authoredText.clear(); lastReply.clear(); }
    speaker = actor; followupSpeaker = nullptr; open = true; status.clear();
    nativePagePending = false;
    Player_SetCsActionWithHaltedActors(gPlayState, actor, PLAYER_CSACTION_8); ownsPlayer = true;
    if (auto menu = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetMenu()) menu->Hide();
    if (transcript.empty() && ARPG::Quests::HasQuests(actor)) {
        auto greeting = ARPG::Quests::AuthoredGreeting(actor);
        Remember("assistant", greeting); ShowReply(greeting);
    } else Hub();
    return true;
}
bool TalkTo(Actor* actor) {
    if (!Enabled() || !Loaded() || !FindProfile(actor) || !Nearby(actor, 220) || !GameInteractor::IsPlayerInControl()) return false;
    auto* player = GET_PLAYER(gPlayState);
    if (player->talkActor == actor && (player->stateFlags2 & PLAYER_STATE2_CAN_ACCEPT_TALK_OFFER) &&
        actor->textId != 0 && actor->textId != 0xFFFF) {
        Player_StartTalking(gPlayState, actor);
        return true;
    }
    return OpenConversation(actor);
}
bool IsOpen() { return open; }
void EndConversation() { Close(); }
void Draw() {
    if (!Enabled() || !Loaded()) return;
    if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F7, false)) {
        if (open) Compose();
        else if (gPlayState->msgCtx.msgMode == MSGMODE_NONE) { if (auto* actor = Nearest()) TalkTo(actor); }
        else if (FindProfile(gPlayState->msgCtx.talkActor)) {
            // A story event or choice must finish before opening custom topics.
            followupSpeaker = gPlayState->msgCtx.talkActor; followupFrames = 0;
        }
    }
    if (open && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (composing) Hub(); else Close();
        if (auto menu = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetMenu()) menu->Hide();
        return;
    }
    std::string hint;
    if (open) hint = composing ? "Enter: Speak   Esc: Back" :
        screen == Screen::Waiting ? "B: Cancel reply   F7: Ask something else" : "A: Continue / Choose   B: Leave   R: Quiet   F7: Ask";
    else if (followupSpeaker && gPlayState->msgCtx.msgMode != MSGMODE_NONE) hint = "Continue the conversation for local topics";
    else if (auto* actor = Nearest()) hint = std::string("F7: Talk to ") + FindProfile(actor)->name;
    if (hint.empty()) return;
    // The game draws every dialogue page and choice. Only keyboard composition
    // and service status need an ImGui accessory, below the native textbox.
    auto* viewport = ImGui::GetMainViewport();
    const float width = std::min(700.0f, viewport->WorkSize.x - 24);
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x * .5f,
                            viewport->WorkPos.y + (composing ? viewport->WorkSize.y * .55f : viewport->WorkSize.y - 8)},
                            ImGuiCond_Always, {.5f, 1});
    ImGui::SetNextWindowSize({width, 0});
    ImGui::SetNextWindowBgAlpha(.8f);
    const auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize | (composing ? 0 : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing);
    if (ImGui::Begin("##ConversationControls", nullptr, flags)) {
        if (composing) {
            ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - 80));
            if (focusInput) { ImGui::SetKeyboardFocusHere(); focusInput = false; }
            bool send = ImGui::InputTextWithHint("##NpcInput", "Ask a question...", input, sizeof(input), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine(); send |= ImGui::Button("Speak", {70, ImGui::GetFrameHeight()});
            if (send) Ask(input);
        }
        ImGui::TextWrapped("%s", hint.c_str());
        if (open && !status.empty()) ImGui::TextWrapped("%s", status.c_str());
    }
    ImGui::End();
}
void Shutdown() {
    Close();
    client.reset();
    if (device) { SDL_CloseAudioDevice(device); device = 0; }
    if (audioInitialized) { SDL_QuitSubSystem(SDL_INIT_AUDIO); audioInitialized = false; }
}
static void Register() {
    auto* hooks = GameInteractor::Instance;
    hooks->RegisterGameHookForID<GameInteractor::OnOpenText>(conversationTextId, [](uint16_t*, bool* load) {
        if (!open || encodedMessage.empty()) return;
        auto& message = gPlayState->msgCtx;
        if (encodedMessage.size() >= sizeof(message.font.msgBuf)) return;
        std::memcpy(message.font.msgBuf, encodedMessage.data(), encodedMessage.size());
        message.font.charTexBuf[0] = 3; // black textbox, bottom position
        message.msgLength = message.font.msgLength = encodedMessage.size();
        *load = false;
    });
    hooks->RegisterGameHook<GameInteractor::OnDialogMessage>(NativeDialogue);
    hooks->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Update);
    hooks->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) { Reset(); });
    hooks->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) { Reset(); });
    hooks->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) { Reset(); });
    const auto destroyed = [](void* actor) { if (actor == speaker || actor == nativeSpeaker || actor == followupSpeaker) Reset(); };
    hooks->RegisterGameHook<GameInteractor::OnActorDestroy>(destroyed);
    hooks->RegisterGameHook<GameInteractor::OnActorKill>(destroyed);
}
static RegisterShipInitFunc init(Register);
}
#else
namespace LocalNpc {
bool OpenConversation(Actor*) { return false; }
bool TalkTo(Actor*) { return false; }
bool IsOpen() { return false; }
void EndConversation() {}
void Draw() {}
void DrawSettings() {}
void Shutdown() {}
}
#endif
