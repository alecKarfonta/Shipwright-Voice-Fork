#!/usr/bin/env python3
"""Prepare persistent normal/testing profiles without replacing existing saves."""

import argparse
import copy
import json
import os
from pathlib import Path
import shutil


def prepare(repo: Path, root: Path, mode: str, import_from: Path | None = None) -> Path:
    profile = root / ("player1" if mode == "normal" else "arpg-testing")
    if import_from is not None and (profile / "Save").exists():
        raise SystemExit(f"Refusing to replace existing saves in {profile}")
    profile.mkdir(parents=True, exist_ok=True)
    # Direct app launches also need the machine-local provider, including its credential.
    services = repo / "npc-services.local.json"
    profile_services = profile / services.name
    if services.is_file() and not profile_services.exists():
        fd = os.open(profile_services, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        with os.fdopen(fd, "w") as output:
            output.write(services.read_text())
    if import_from is not None:
        source = import_from / "Save"
        if not source.is_dir():
            raise SystemExit(f"No Save directory in {import_from}")
        shutil.copytree(source, profile / "Save")
        for name in ("shipofharkinian.json", "imgui.ini", "SoHTimeSplitData.json"):
            if (import_from / name).is_file():
                shutil.copy2(import_from / name, profile / name)

    config_path = profile / "shipofharkinian.json"
    if not config_path.exists():
        config = {}
        normal = root / "player1" / "shipofharkinian.json"
        if mode == "test" and normal.exists():
            original = json.loads(normal.read_text())
            for key in ("Window", "ConfigVersion"):
                if key in original:
                    config[key] = copy.deepcopy(original[key])
            config["CVars"] = {
                key: copy.deepcopy(value)
                for key, value in original.get("CVars", {}).items()
                if key in ("gSettings", "gAudioEditor", "gControllers")
            }
        if mode == "test":
            cvars = config.setdefault("CVars", {})
            cvars.setdefault("gSettings", {}).update({"BootSequence": 4, "OpenMenuBar": 1})
            cvars["gDeveloperTools"] = {"DebugSaveFileMode": 2, "DebugEnabled": 1}
            cvars["gOpenWindows"] = {"ARPG": 0, "ARPGInventory": 1, "Notifications": 1}
            cvars["gEnhancements"] = {
                "ARPG": {"Enabled": 1, "Affixes": 1, "Items": 1, "Skills": 1, "PickupText": 0},
                "BetterDebugWarpScreen": 1,
                "TimeSavers": {"SkipCutscene": {
                    key: 1 for key in ("Intro", "Entrances", "Story", "LearnSong", "BossIntro", "QuickBossDeaths", "OnePoint")
                }},
            }
            config["WarpPoints"] = {"ARPG testing field": {
                "entranceId": 0xCD, "roomNum": 0,
                "pos": {"x": -1500, "y": 300, "z": 3500}, "rotY": 0, "bootToPoint": True,
            }}
    else:
        config = json.loads(config_path.read_text())

    # Living-world populations are part of the default adventure in both profiles.
    # Preserve explicit later opt-outs when preparing an existing profile.
    arpg = config.setdefault("CVars", {}).setdefault("gEnhancements", {}).setdefault("ARPG", {})
    for key in ("Enabled", "Affixes", "Enemies", "Packs", "Tactics"):
        arpg.setdefault(key, 1)
    config_path.write_text(json.dumps(config, indent=2) + "\n")

    # [TIME] | [INFO] | Profile.Assets | Use the current build's archives while keeping user data outside the build directory.
    for name in ("oot.o2r", "soh.o2r"):
        archive = repo / "build-cmake" / "soh" / name
        link = profile / name
        if not archive.is_file():
            raise SystemExit(f"Missing archive: {archive}")
        if link.is_symlink():
            if link.resolve() == archive.resolve():
                continue
            link.unlink()
        elif link.exists():
            continue
        link.symlink_to(archive)
    return profile


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("normal", "test"))
    parser.add_argument("--import-from", type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    root = Path(os.environ.get("SOH_PROFILE_ROOT", str(Path.home() / "Library/Application Support/shipofharkinian/profiles")))
    print(prepare(repo, root, args.mode, args.import_from))
