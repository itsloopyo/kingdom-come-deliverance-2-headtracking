#include "ads.h"

#include <cameraunlock/ads/ads_fade.h>

#include "logging.h"

namespace kcd2_ht::ads
{
    namespace
    {
        // Render-thread only, so plain members rather than atomics.
        cameraunlock::ads::AdsFade g_fade;
        bool g_wasAiming = false;
    }

    void Apply(HeadPose& pose, std::uint64_t nowMs, bool aiming, bool trueFreeLook)
    {
        // The fade's input is the game's own aim state, never the gate's
        // verdict. Feeding a verdict back in makes the fade start raising the
        // instant it finishes lowering, several times a second.
        const float scale = g_fade.Update(aiming && !trueFreeLook, nowMs);
        pose.x *= scale;
        pose.y *= scale;
        pose.z *= scale;

        if (aiming != g_wasAiming)
        {
            g_wasAiming = aiming;
            Log::Line("ADS: sights %s.", aiming ? "up" : "down");
        }
    }

    void Suppress()
    {
        g_fade.Reset();
        g_wasAiming = false;
    }
}
