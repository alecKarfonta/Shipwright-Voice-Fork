# Unified NPC conversations

Talk to an NPC using the normal game controls. After the original conversation finishes, supported speakers continue into local topics in the **same native textbox**. **F7** uses the NPC's offered campaign interaction when available, or opens local topics within speaking distance. Campaign choices, item exchanges, shops, cutscenes, ocarina messages and story events remain owned by their original scripts; custom topics wait until the player is free.

Use the normal **A / C-Up** confirmation and stick selection for pages and choices. **More topics** cycles through additional options. **B / Escape** leaves custom dialogue; **R** stops voice; B cancels a pending generated reply and returns to topics. **F7** or **Ask a question** opens a small keyboard composer above the textbox; Enter submits and Escape returns to topics. Finishing an ordinary final campaign page with B returns directly to play; confirming with A continues into local topics. Custom conversations hold Link and other actors with the native cutscene action, and only text entry captures game input. Nearby speakers must be in the current room, within 220 units and visible with a clear line of sight.

Open **Dev Tools → ARPG Mod → Open ARPG Mod → NPC dialogue** for the enable switch, voices, scripted dialogue narration, volume, service endpoints, model, and per-character saved voices. This feature also works with ARPG gameplay disabled. English dynamic conversations are supported; original English, French and German NPC text can be voiced. Japanese native text uses a different encoding and is excluded. Existing accessibility narration takes priority over this feature's native-text narration.

Profiles cover Saria, Mido, Kokiri villagers, Malon, Talon, Ingo, Darunia, Gorons, Zoras, Ruto, the cucco keeper, Hyrule guards, Hylian villagers, Gerudo guards and shopkeepers. Unknown actors, signs and item messages are not automatically given voices. The same actor family can share a voice; generic villagers do not acquire invented names. Each profile supplies a delivery instruction. A blank voice name uses the model's built-in voice; set a saved reference name to clone it.

Original story dialogue, authored quest replies and generated replies use the game's renderer, glyphs, typewriter, choice arrows and page controls. The separate scrolling chat window has been removed. **Local quests** supports explanations, acceptance, objective advice, explicit deliveries, tracking and both gear rewards directly in conversation. The journal remains available for reviewing the campaign. **Conversation history**, **Hear that again**, saved character voices and voice cancellation remain available; questions and replies are remembered for the current speaker.

All custom text passes through a bounded encoder: four lines and at most 120 glyphs per native page, with width-aware wrapping. Provider and player text cannot introduce native message commands. Custom pages use a private message ID and no talk actor, so their choices cannot trigger an original NPC's story or reward callbacks. The engine's message tables and campaign renderer are unchanged.

## Services

`./soh.sh run`, `test` and `debug` load the ignored `npc-services.local.json` through `scripts/npc_services.py`. Environment variables take precedence over that file; nonempty in-game endpoint/model CVars take precedence over environment defaults. Blank fields fall back to saved defaults. API keys are read from the environment or the private service file and never saved in game CVars, transcripts, logs or the repository.

Copy `npc-services.example.json` to `npc-services.local.json` and edit its values, or import the endpoint, model and credential already used by Lastwall:

```sh
python3 scripts/npc_services.py --import-lastwall '/path/to/Lastwall/.env'
```

The importer creates a mode-0600 file and refuses to overwrite an existing configuration. It parses the selected values without executing shell code. Supported environment overrides: `SOH_NPC_LLM_ENDPOINT`, `SOH_NPC_LLM_MODEL`, `SOH_NPC_LLM_API_KEY`, `SOH_NPC_TTS_ENDPOINT`. `CHAT_API_URL`, `CHAT_MODEL` and `CHAT_API_KEY` are also accepted by the native client when no SoH-specific value is supplied. Direct launches also read `npc-services.local.json` from the active profile or working directory (or the path in `SOH_NPC_CONFIG`). The profile launcher seeds this private mode-0600 file from the repository-local configuration when absent. Environment variables override file settings. Restart after editing the file.

The LLM endpoint is a full OpenAI-compatible chat-completions URL, with a model identifier. The default TTS URL is `http://192.168.50.64:8013/tts`. Requests send `text`, `language`, and, when present, `voice_name` and `instruction`. TTS receives no LLM credential. WAV replies are decoded with SDL and played on a separate audio device, mixed by the OS with normal game audio. Named voices must already exist on the voice service; a missing voice leaves readable captions and a service error.

The server's `/tts/design` and `/voices?voice_name=...` endpoints can create reusable references outside the game. Once saved, enter that name in the character's voice field. Voice generation/upload is not performed on every NPC interaction.

## Behavior and limits

This adapts Lastwall's profile/context/request/voice lifecycle, rather than its Unreal transport or social simulation. Prompts include identity, scene/region, age, time of day, ARPG level when enabled, the last authored page from this NPC, and up to eight recent conversation turns. Memory is temporary and scoped to the current speaker; changing speaker, scene or save clears it. The model is asked for concise JSON dialogue. Responses are validated before display/speech. Generated prose may still improvise or get lore wrong; it has no engine command interface and cannot change quests, rewards, inventory, relationships or save data. Native story text and choices remain authoritative.

One worker owns HTTP calls; game objects and SDL playback stay on the main thread. Requests have bounded sizes and timeouts (3-second connection, 45-second LLM, 90-second TTS). The latest request supersedes the previous one; no background backlog accumulates. Closing, scene/save changes, actor destruction, death, and disabling the feature invalidate late responses and clear audio. Captions appear before synthesis finishes, and remain if synthesis fails. Exact speech requests are cached in memory, bounded to 24 lines / 32 MiB. Cache keys include endpoint, text, voice, delivery and language.

This is local presentation, including in co-op: generated dialogue and voice are not broadcast to other clients. It does not replace Lastwall's long-term social memory, relationship system, spatial voice, pre-rendered voice imports or NPC actions. Original dialogue, local topics, authored quests, history and reward choices support the normal controller controls. Free-text entry requires a keyboard.

## Build and tests

Desktop builds enable `SOH_LOCAL_NPC` and require libcurl. Configure with `-DSOH_LOCAL_NPC=OFF` to omit it; console targets default off. The new sources are discovered by the existing source glob.

```sh
cmake --build build-cmake --target soh
cmake -S tests/local-npc -B /tmp/soh-npc-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/soh-npc-tests
ctest --test-dir /tmp/soh-npc-tests --output-on-failure
```

The dialogue encoder tests cover glyph/page capacity, pixel width, long unbroken text, typography and message-command injection. The local HTTP fixture tests prompt/history bounds, valid and malformed replies, truncated and oversized responses, HTTP failure, caption-first delivery, TTS failure, cache isolation by voice, cancellation, stale-result rejection, shutdown and credential separation. It does not contact real services. Optional live smoke test:

```sh
SOH_NPC_LIVE_TTS=http://192.168.50.64:8013/tts SOH_NPC_LIVE_LLM=1 \
  python3 scripts/npc_services.py -- python3 tests/local-npc/fixture.py /tmp/soh-npc-tests/npc_client_test
```

That test uses the configured provider, prints its generated test reply, and writes `/tmp/soh-npc-live.wav`. Real-service latency and lore quality remain model-dependent.

Set `SOH_NPC_LIVE_VOICE` to exercise a saved reference instead of the built-in voice. [macOS verification](../../../../tests/local-npc/verification-macos.json) records the native cucco-keeper conversation, queued voice playback, Escape behavior, service smoke tests and remaining acceptance coverage.

## Native integration check

Build with `-DSOH_ARPG_RUNTIME_TESTS=ON` and launch with `SOH_DIALOGUE_QA=1` and a fresh `SHIP_HOME` directory whose name starts with `soh-dialogue-qa-`. Seed only the normal assets and window configuration; no user save is required. The opt-in harness creates a debug adventure in memory and writes `dialogue-results.json` inside that profile. It exercises a real Kokiri campaign choice and continuation, automatic topic handoff, native quest acceptance and rewards, duplicate transaction rejection, cancellation, actor invalidation, disabled ARPG gameplay and the feature enable switch. It never writes an ordinary save.

[Unified dialogue verification on macOS](../../../../tests/local-npc/unified-dialogue-verification-macos.json) records the native build, regression tests, 18 in-game checks and visual review of campaign handoff, question entry, generated reply pagination, native topics and HUD restoration. The [runtime report](../../../../tests/local-npc/unified-dialogue-runtime-results-macos.json) lists every in-game assertion.
