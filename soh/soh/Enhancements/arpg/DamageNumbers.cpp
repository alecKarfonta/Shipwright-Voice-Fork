#include "DamageNumbers.h"

// [TIME] | [INFO] | DamageNumbers.cpp | Floating combat text over struck enemies
//
// Data flow: OnDamageActor (post-multiplier) -> queue {pos, value, kind} ->
// OnGameFrameUpdate ages/rises the numbers -> OnPlayDrawEnd billboards them
// over the world using the same font-quads technique as nametags.
// World position is captured at hit time so the number stays where the hit
// landed even if the actor moves or dies mid-flight.

#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdlib>

#include <libultraship/bridge/consolevariablebridge.h>

#include "ARPG.h"
#include "AffixSystem.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/ShipUtils.h"
#include "soh/frame_interpolation.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "z64.h"
#include "variables.h"
#include "functions.h"
#include "macros.h"
extern PlayState* gPlayState;
}

namespace ARPG::DamageNumbers {

namespace {

constexpr int LIFE_FRAMES = 50;
constexpr float RISE_PER_FRAME = 1.4f;
constexpr int MAX_ACTIVE = 24;

enum class Kind : uint8_t { Normal, Crit, Resist };

struct Number {
    Vec3f pos;
    float value;
    Kind kind;
    int age;
    Vtx* vtx;   // built at spawn: digits + optional textbox backdrop
    Mtx* mtx;
    int16_t width;
};

std::vector<Number> sNumbers;

// [TIME] | [INFO] | DamageNumbers.BuildVertices | Char quads for the digits at hit time (16-char gSPVertex limit respected by <=10 digits).
Vtx* BuildVertices(const std::string& text) {
    const size_t numChar = text.size();
    Vtx* vtx = (Vtx*)calloc(sizeof(Vtx[4]), numChar + 1);
    int16_t offsetX = 0;
    for (size_t i = 0; i < numChar; ++i) {
        int16_t charWidth = (int16_t)Ship_GetCharFontWidth(text[i]);
        Ship_CreateQuadVertexGroup(&vtx[(i + 1) * 4], offsetX, 0, charWidth, 16, 0);
        offsetX += charWidth;
    }
    return vtx;
}

void FreeNumber(Number& n) {
    if (n.vtx != nullptr) { free(n.vtx); n.vtx = nullptr; }
    if (n.mtx != nullptr) { delete n.mtx; n.mtx = nullptr; }
}

} // namespace

bool Enabled() {
    return ARPG::Enabled() && CVarGetInteger(CVAR_ENHANCEMENT("ARPG.DamageNumbers"), 1) != 0;
}

void Spawn(Actor* target, float damage, bool crit, bool resisted) {
    if (!Enabled() || target == nullptr || gPlayState == nullptr || damage <= 0.0f) {
        return;
    }
    Number n{};
    n.pos = target->world.pos;
    n.pos.y = std::max(n.pos.y, target->focus.pos.y);
    n.value = damage;
    n.kind = resisted ? Kind::Resist : crit ? Kind::Crit : Kind::Normal;
    n.age = 0;
    n.vtx = nullptr;
    n.mtx = new Mtx();

    if (sNumbers.size() >= MAX_ACTIVE) {
        FreeNumber(sNumbers.front());
        sNumbers.erase(sNumbers.begin());
    }
    sNumbers.push_back(std::move(n));
}

namespace {

// [TIME] | [INFO] | DamageNumbers.FrameInterp | OPEN_DISPS declares these inside its enclosing
// namespace; define them here so calls inside DrawAll (also anon-namespace) resolve to real globals.
void FrameInterpolation_RecordOpenChild(const void* a, int b) { ::FrameInterpolation_RecordOpenChild(a, b); }
void FrameInterpolation_RecordCloseChild() { ::FrameInterpolation_RecordCloseChild(); }

// [TIME] | [INFO] | DamageNumbers.Format | D2-style rounded numbers; crit gets an exclamation tail.
std::string Format(const Number& n) {
    long v = std::lround(n.value);
    if (v <= 0) { v = 1; }
    std::string s = std::to_string(v);
    if (n.kind == Kind::Crit) { s += "!"; }
    return s;
}

void DrawAll(PlayState* play) {
    if (!Enabled() || gPlayState == nullptr || sNumbers.empty()) {
        return;
    }

    static std::vector<Gfx> sDl;
    sDl.clear();

    OPEN_DISPS(play->state.gfxCtx);

    POLY_XLU_DISP = Gfx_SetupDL_39(POLY_XLU_DISP);
    sDl.push_back(gsDPSetAlphaCompare(G_AC_NONE));
    sDl.push_back(gsDPSetCombineLERP(0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0,
                                     0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0));

    sDl.push_back(gsSPClearGeometryMode(G_ZBUFFER | G_CULL_BOTH | G_LIGHTING));

    for (auto& n : sNumbers) {
        std::string text = Format(n);
        size_t numChar = text.length();
        if (numChar == 0) { continue; }

        if (n.vtx == nullptr) {
            n.vtx = BuildVertices(text);
            int16_t w = 0;
            for (size_t i = 0; i < numChar; ++i) { w = static_cast<int16_t>(w + Ship_GetCharFontWidth(text[i])); }
            n.width = w;
        }

        // [TIME] | [INFO] | DamageNumbers.Alpha | Pop in fast, hold, then fade over the last third.
        float alpha = n.age > LIFE_FRAMES - 14 ? float(LIFE_FRAMES - n.age) / 14.0f : 1.0f;
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        // Keep digits legible at normal combat distance instead of shrinking
        // into a few blurry pixels. Clamp growth for far-away targets.
        float distance = Math_Vec3f_DistXYZ(&n.pos, &GET_ACTIVE_CAM(play)->eye);
        float scale = std::clamp(distance / 260.0f, 0.85f, 3.0f);
        if (n.kind == Kind::Crit) { scale *= 1.25f; }

        // colors: white normal, yellow crit, grey resist
        uint8_t r = n.kind == Kind::Crit ? 255 : n.kind == Kind::Resist ? 150 : 255;
        uint8_t g = n.kind == Kind::Crit ? 230 : n.kind == Kind::Resist ? 150 : 255;
        uint8_t b = n.kind == Kind::Crit ? 60  : n.kind == Kind::Resist ? 150 : 255;


        FrameInterpolation_RecordOpenChild(&n, 0);

        float y = n.pos.y + 26.0f + n.age * RISE_PER_FRAME;

        Matrix_Translate(n.pos.x, y, n.pos.z, MTXMODE_NEW);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        float mirror = CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0) != 0 ? -1.0f : 1.0f;
        Matrix_Scale(scale * mirror, -scale, 1.0f, MTXMODE_APPLY);
        Matrix_Translate(-(n.width / 2.0f), 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_ToMtx(n.mtx, (char*)__FILE__, __LINE__);

        sDl.push_back(gsSPMatrix(n.mtx, G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW));
        sDl.push_back(gsDPSetPrimColor(0, 0, r, g, b, uint8_t(255 * alpha)));

        for (size_t i = 0, vtxGroup = 0; i < numChar; ++i) {
            if (i % 16 == 0) {
                size_t numVtxToLoad = std::min<size_t>(numChar - i, 16) * 4;
                sDl.push_back(gsSPVertex(&(n.vtx)[4 + (vtxGroup * 16 * 4)], int(numVtxToLoad), 0));
                vtxGroup++;
            }
            uintptr_t texture = (uintptr_t)Ship_GetCharFontTexture(text[i]);
            int16_t vertexStart = 4 * (i % 16);
            Gfx charTexture[] = { gsDPLoadTextureBlock_4b(texture, G_IM_FMT_I, FONT_CHAR_TEX_WIDTH, FONT_CHAR_TEX_HEIGHT, 0,
                                                          G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                                                          G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD) };
            sDl.insert(sDl.end(), std::begin(charTexture), std::end(charTexture));
            sDl.push_back(gsSP1Quadrangle(vertexStart, vertexStart + 2, vertexStart + 3, vertexStart + 1, 0));
        }

        sDl.push_back(gsSPPopMatrix(G_MTX_MODELVIEW));
        FrameInterpolation_RecordCloseChild();
    }

    sDl.push_back(gsSPEndDisplayList());
    gSPDisplayList(POLY_XLU_DISP++, sDl.data());

    CLOSE_DISPS(play->state.gfxCtx);
}

void Update() {
    if (GameInteractor::IsGameplayPaused()) { return; }
    for (auto it = sNumbers.begin(); it != sNumbers.end();) {
        it->age++;
        if (it->age >= LIFE_FRAMES) {
            FreeNumber(*it);
            it = sNumbers.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace

// SECTION: registration

static void RegisterDamageNumberHooks() {
    COND_HOOK(OnGameFrameUpdate, true, Update);
    COND_HOOK(OnPlayDrawEnd, true, []() { DrawAll(gPlayState); });
    COND_HOOK(OnPlayDestroy, true, []() {
        for (auto& n : sNumbers) { FreeNumber(n); }
        sNumbers.clear();
    });
}

static RegisterShipInitFunc initDamageNumbers(RegisterDamageNumberHooks);

} // namespace ARPG::DamageNumbers
