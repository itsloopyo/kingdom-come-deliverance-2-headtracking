#pragma once

#include <cstdint>

#include <cameraunlock/ads/ads_mode.h>

#include "view_injection.h"

// What head tracking does while the sights are up.
//
// The cycle, the value strings, the fade shape, the entry-relative pose and the
// blend all live in cameraunlock-core (cameraunlock/ads/). Nothing here spells
// any of them again; this file is the KCD2-shaped wiring around them: where the
// aim state is read from, and where the blended pose is handed back.
//
// TWO SLOTS, not three. KCD2 draws its own aim reticle at the point the shot
// lands - the HUD positions `CursorCross` from a computed point whenever the
// ranged aim state is up, and hides it otherwise - and this mod already moves
// that reticle onto the clean aim point through cursor_hook.cpp. That is the
// shared spec's two-slot case exactly, so `marker` does not exist here: not in
// the cycle, not in the config validation, not in the README. There are no
// optics in 1403 either, so the scope-reticle carve-out cannot apply.

namespace kcd2_ht::ads
{
    using cameraunlock::ads::AdsMode;

    AdsMode Mode();

    // Seeded from the INI at bootstrap.
    void SetMode(AdsMode mode);

    // Two-slot advance: paused -> tracked -> paused. Returns the new mode.
    AdsMode Cycle();

    // Called from the cursor hook every time the game positions its ranged aim
    // reticle at a computed point, which is the game's own statement that the
    // sights are up. Observed rather than latched: the HUD makes this decision
    // again every frame, and an observation that stops arriving expires.
    void NoteAimReticle(std::uint64_t nowMs);

    // The ADS state for this frame. Polled, never latched, and an absent
    // observation reads as NOT aiming - failing toward stock is the safe
    // direction, and it is also what a player who has turned the game's fire
    // cursor off gets.
    bool IsAiming(std::uint64_t nowMs);

    // Once per rendered frame, on a frame whose rotation is live. Rewrites
    // @p pose into what the camera should actually be given this frame, and
    // returns whether the sights are up so the caller can report it.
    //
    // In `paused` the pose eases to nothing over the fade and stays there, so
    // the sight picture is the game's own; in `tracked` it eases into the pose
    // measured from the frame the sights came up on. Both make the same swing
    // onto the aim point.
    bool Apply(HeadPose& pose, std::uint64_t nowMs);

    // Every frame tracking is suppressed - master toggle off, no tracker data,
    // a pose that is not a number. Drops the entry pose and the fade so the next
    // aim re-enters cleanly rather than resuming against a pre-suppression pose.
    void Suppress();
}
