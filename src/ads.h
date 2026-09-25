#pragma once

#include <cstdint>

#include "view_injection.h"

// What aiming a bow or crossbow does to the head pose. Head tracking carries
// straight on through the aim: rotation is never faded, made relative or
// suspended, so the weapon stays on the aim and seen down its own sight line.
// By default the lean eases out while the sights are up, because translating the
// eye takes it off that line (sights locked). True free look leaves the lean in
// full.

namespace kcd2_ht::ads
{
    // Once per rendered frame, on a frame whose rotation is live. Scales the
    // lean in @p pose by the fade and leaves its rotation alone. @p aiming is
    // the game's own aim state for this frame, polled rather than latched. With
    // @p trueFreeLook the fade heads back to the full lean, so toggling it
    // mid-aim slides the lean in or out rather than stepping it.
    void Apply(HeadPose& pose, std::uint64_t nowMs, bool aiming, bool trueFreeLook);

    // Every frame tracking is suppressed - master toggle off, no tracker data,
    // a pose that is not a number. Drops the fade so the next frame starts from
    // the hip rather than resuming a transition from before the suppression.
    void Suppress();
}
