#pragma once

#include <cstdint>

#include "aim_projection.h"

// Moves the GAME's crosshair to where the shot actually goes, rather than
// hiding it and drawing our own. The stock cursor carries weapon, stamina and
// interaction state that a plain dot cannot, so shifting it is worth the extra
// pinned addresses.
//
// The HUD sets its cursor through one helper that takes a position in back
// buffer pixels, subtracts the screen centre and converts the remainder into
// Flash units for the `CursorCross` / `CombatCursorCross` elements. That
// conversion is linear, so adding the aim offset to the pixel position it is
// handed lands the cursor exactly where the reticle projection says it should
// be, with no knowledge of Flash needed.

namespace kcd2_ht::cursor
{
    // Installed whenever the mod is active, whatever MoveCrosshair says: the
    // ADS state is read from the call sites this hook sees, so a player who
    // turned the crosshair move off still gets their sights detected.
    // @p moveCrosshair decides only whether the aim offset is applied.
    bool Install(std::uintptr_t moduleBase, bool moveCrosshair);

    void PrepareAimMarker();
    void HideAimMarker();

    // Called from the view hook on every frame tracking is applied. Doubles as
    // the liveness signal: when these stop arriving the cursor goes back to
    // wherever the game wanted it, which is what makes menus and loading screens
    // look after themselves.
    void SubmitAim(const AimProjection& aim, float fovRadians, float projectionRatio, bool aiming);
}
