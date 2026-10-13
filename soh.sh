#!/usr/bin/env bash
# [TIME] | [INFO] | soh.sh | Native Ship of Harkinian build/run/mod loop
#
# Usage:
#   ./soh.sh build       - Incremental native rebuild after C/C++ edits
#   ./soh.sh full        - Clean configure + full rebuild (~9 min)
#   ./soh.sh assets      - Re-extract oot.o2r from roms/*.z64
#   ./soh.sh run         - Launch the native executable
# [TIME] | [INFO] | soh.Usage | ./soh.sh test starts a separate fully equipped ARPG testing adventure.
#   ./soh.sh debug       - Launch with log tail (console output visible)
#
# Paths:
#   Source edits:  soh/soh/**/*.cpp  (game logic - same names as decomp!)
#   Mods (.otr):   drop into <SHIP_HOME>/mods/
#   Config/saves:  <SHIP_HOME>/shipofharkinian.json

set -euo pipefail

readonly SOH_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly BUILD_DIR="${SOH_ROOT}/build-cmake"
readonly EXE="${BUILD_DIR}/soh/soh-macos"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "${CYAN}[INFO]${NC} $*"; }
ok()    { echo -e "${GREEN}[ OK ]${NC} $*"; }
fail()  { echo -e "${RED}[FAIL]${NC} $*"; exit 1; }

case "${1:-help}" in
    build)
        info "Incremental native rebuild..."
        ninja -C "${BUILD_DIR}" || fail "build failed"
        ok "Executable: ${EXE}"
        ;;

    full)
        info "Clean configure + rebuild..."
        rm -rf "${BUILD_DIR}"
        cmake -H"${SOH_ROOT}" -B"${BUILD_DIR}" -GNinja -DCMAKE_BUILD_TYPE:STRING=Release
        ninja -C "${BUILD_DIR}" || fail "build failed"
        ok "Executable: ${EXE}"
        ;;

    assets)
        info "Re-extracting oot.o2r from roms/..."
        ninja -C "${BUILD_DIR}" ExtractAssets
        ok "Assets: ${BUILD_DIR}/soh/oot.o2r"
        ;;

    run|test|debug)
        [ -x "${EXE}" ] || fail "No executable - run './soh.sh build' first"
        if [ "$1" = test ]; then
            SHIP_HOME="$(python3 "${SOH_ROOT}/scripts/soh_profile.py" test)"
            export SHIP_HOME
            export SOH_ARPG_SANDBOX=1
        else
            if [ -z "${SHIP_HOME:-}" ]; then
                SHIP_HOME="$(python3 "${SOH_ROOT}/scripts/soh_profile.py" normal)"
                export SHIP_HOME
            fi
            unset SOH_ARPG_SANDBOX
        fi
        info "Profile: ${SHIP_HOME}"
        info "Launching Ship of Harkinian (native ARM64)..."
        cd "${BUILD_DIR}/soh"
        if [ "$1" = debug ]; then
            python3 "${SOH_ROOT}/scripts/npc_services.py" -- ./soh-macos 2>&1 | tee /tmp/soh_console.log
        else
            exec python3 "${SOH_ROOT}/scripts/npc_services.py" -- ./soh-macos
        fi
        ;;

    *)  grep -E "^#   " "${BASH_SOURCE[0]}" | sed 's/^#   //';;
esac
