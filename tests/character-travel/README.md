# Portable characters

Each native save slot is a character. The public server browser always uses portable characters. Its character line shows the loaded name, save slot, level and rupees. In the title menu, select a party, choose **Choose Character**, and load an existing save (or create one through the original file menu).

During play, open **Network → Anchor → Public Games / Server Browser → Save & Switch Server**, then join another party or change the relay under Connection. **Save & Play Solo** returns to the same adventure offline. Both actions wait for the native save worker to finish writing the current snapshot; a write failure reports an error and leaves the connection intact. Joining from gameplay also checkpoints first. Temporary debug saves, cutscenes, death and other unsafe save states require returning to normal gameplay first.

Gear rolls, backpack and sockets, XP, skills, materials, currency, story, campaign objectives and one-time reward history belong to the local character. Other players cannot import legacy inventory/story snapshots into portable characters, even when a room enables legacy synchronization. Legacy shared-save play remains an explicit opt-out in the advanced Network → Anchor settings.

An unfinished quest retains its work, physical world placements and largest party difficulty when moving to a different server, room or team. It becomes personal and can be shared with the new party from the journal. Returning to the same party after a disconnect resumes sharing. Claimed rewards cannot be claimed again. Returning to file selection disconnects the old character before another save is loaded.

This is local save portability between game sessions, not an account service or cloud/cross-device save synchronization. The relay does not own character inventories.

## Verification

- `tests/arpg/character_travel_test.cpp`: all 58 quests, reward replay, saved progress, party boundaries and packet protection policy.
- `tests/arpg/character_travel_runtime_test.cpp`: opt-in native save and two-relay fixture. Build with `SOH_CHARACTER_TRAVEL_TESTS=ON`; requires `SOH_CHARACTER_TRAVEL_TEST=1` and an isolated `SHIP_HOME` named `soh-arpg-qa-travel-*` containing a vanilla save in slot 1. Local relay ports are 43396 and 43397. Result is `travel-results.json` inside that profile.
- Production builds leave `SOH_CHARACTER_TRAVEL_TESTS=OFF`.
