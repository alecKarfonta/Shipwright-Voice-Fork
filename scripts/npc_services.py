#!/usr/bin/env python3
"""Launch SoH with machine-local NPC services; never evaluate configuration as shell code."""
import argparse
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "npc-services.local.json"
FIELDS = {
    "llm_endpoint": "SOH_NPC_LLM_ENDPOINT",
    "llm_model": "SOH_NPC_LLM_MODEL",
    "llm_api_key": "SOH_NPC_LLM_API_KEY",
    "tts_endpoint": "SOH_NPC_TTS_ENDPOINT",
}


def import_lastwall(path: Path, destination: Path = CONFIG):
    if destination.exists():
        raise SystemExit(f"Configuration already exists: {destination}; edit it to change providers.")
    values = {}
    for line in path.read_text().splitlines():
        key, separator, value = line.strip().removeprefix("export ").partition("=")
        if separator:
            value = value.strip()
            if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
                value = value[1:-1]
            values[key.strip()] = value
    config = {
        "llm_endpoint": values.get("HUIHUI_LLM_ENDPOINT") or values.get("CHAT_API_URL", ""),
        "llm_model": values.get("HUIHUI_LLM_MODEL") or values.get("CHAT_MODEL", ""),
        "llm_api_key": values.get("HUIHUI_LLM_API_KEY") or values.get("CHAT_API_KEY", ""),
        "tts_endpoint": "http://192.168.50.64:8013/tts",
    }
    if not config["llm_endpoint"] or not config["llm_model"]:
        raise SystemExit("Lastwall configuration has no LLM endpoint/model.")
    fd = os.open(destination, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    with os.fdopen(fd, "w") as output:
        json.dump(config, output, indent=2)
        output.write("\n")
    print(f"Saved NPC service settings to {destination} (API key not displayed).")


def environment(path: Path = CONFIG):
    env = os.environ.copy()
    env.setdefault("SOH_NPC_CONFIG", str(path))
    if path.exists():
        config = json.loads(path.read_text())
        if not isinstance(config, dict):
            raise SystemExit("NPC service configuration must be a JSON object.")
        for field, variable in FIELDS.items():
            value = config.get(field, "")
            if not isinstance(value, str) or "\x00" in value or "\n" in value or "\r" in value:
                raise SystemExit(f"Invalid NPC configuration field: {field}")
            if value:
                env.setdefault(variable, value)
    return env


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--import-lastwall", type=Path, metavar="ENV_FILE")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.import_lastwall:
        import_lastwall(args.import_lastwall)
    elif args.command:
        command = args.command[1:] if args.command[0] == "--" else args.command
        os.execvpe(command[0], command, environment())
    else:
        parser.error("provide --import-lastwall ENV_FILE or a command to launch")
