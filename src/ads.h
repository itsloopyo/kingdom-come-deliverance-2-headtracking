#pragma once

#include <cstdint>

#include <cameraunlock/ads/ads_mode.h>

#include "view_injection.h"

namespace kcd2_ht::ads
{
    using cameraunlock::ads::AdsMode;

    AdsMode Mode();

    // Seeded from the INI at bootstrap.
    void SetMode(AdsMode mode);

    // paused -> marker -> tracked -> paused.
    AdsMode Cycle();

    // Once per rendered frame, on a frame whose rotation is live. Rewrites
    // @p pose into what the camera should actually be given this frame, and
    // returns whether the sights are up so the caller can report it.
    //
    // In `paused` the pose eases to nothing over the fade and stays there, so
    // the sight picture is the game's own; in `tracked` it eases into the pose
    // measured from the frame the sights came up on. Both make the same swing
    // onto the aim point.
    bool Apply(HeadPose& pose, std::uint64_t nowMs, bool aiming);

    // Every frame tracking is suppressed - master toggle off, no tracker data,
    // a pose that is not a number. Drops the entry pose and the fade so the next
    // aim re-enters cleanly rather than resuming against a pre-suppression pose.
    void Suppress();
}
