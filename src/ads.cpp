#include "ads.h"

#include <atomic>

#include <cameraunlock/ads/ads_blend.h>
#include <cameraunlock/ads/ads_fade.h>
#include <cameraunlock/ads/entry_pose.h>

#include "logging.h"

namespace kcd2_ht::ads
{
    namespace
    {
        using cameraunlock::ads::AdsEntryPose;
        using cameraunlock::ads::AdsFade;

        std::atomic<AdsMode> g_mode{cameraunlock::ads::kDefaultAdsMode};

        // Render-thread only, so plain members rather than atomics.
        AdsFade g_fade;
        AdsEntryPose g_entry;

        AdsEntryPose::Pose ToCore(const HeadPose& pose)
        {
            AdsEntryPose::Pose out;
            out.pitch = pose.pitch;
            out.yaw = pose.yaw;
            out.roll = pose.roll;
            out.x = pose.x;
            out.y = pose.y;
            out.z = pose.z;
            return out;
        }

        void FromCore(const AdsEntryPose::Pose& in, HeadPose& pose)
        {
            pose.pitch = in.pitch;
            pose.yaw = in.yaw;
            pose.roll = in.roll;
            pose.x = in.x;
            pose.y = in.y;
            pose.z = in.z;
        }
    }

    AdsMode Mode() { return g_mode.load(std::memory_order_relaxed); }

    void SetMode(AdsMode mode) { g_mode.store(mode, std::memory_order_relaxed); }

    AdsMode Cycle()
    {
        const AdsMode next = cameraunlock::ads::NextAdsMode(Mode());
        SetMode(next);
        return next;
    }

    bool Apply(HeadPose& pose, std::uint64_t nowMs, bool aiming)
    {
        // The fade's `aiming` input is the game's own state, never the gate's
        // verdict. Feeding a verdict back in makes the fade start raising the
        // instant it finishes lowering, several times a second.
        const float scale = g_fade.Update(aiming, nowMs);

        const AdsEntryPose::Pose absolute = ToCore(pose);
        // The caller only reaches here on a live rotation, which is exactly the
        // gate the entry capture needs: capturing off an interpolator that has
        // been reset would freeze a pre-suppression pose for the whole aim.
        const AdsEntryPose::Pose relative = g_entry.Relative(aiming, /*live=*/true, absolute);

        const AdsMode mode = Mode();
        FromCore(cameraunlock::ads::BlendAdsPose(mode, scale, absolute, relative), pose);
        static std::uint64_t lastLogMs = 0;
        static AdsMode lastMode = cameraunlock::ads::kDefaultAdsMode;
        static bool lastAiming = false;
        if (nowMs - lastLogMs >= 1000 || mode != lastMode || aiming != lastAiming) {
            lastLogMs = nowMs;
            lastMode = mode;
            lastAiming = aiming;
            Log::Line("ADS: mode=%s aiming=%s scale=%.3f "
                      "input=(%.2f %.2f %.2f) output=(%.2f %.2f %.2f)",
                      cameraunlock::ads::AdsModeValue(mode), aiming ? "yes" : "no",
                      static_cast<double>(scale),
                      static_cast<double>(absolute.yaw), static_cast<double>(absolute.pitch),
                      static_cast<double>(absolute.roll), static_cast<double>(pose.yaw),
                      static_cast<double>(pose.pitch), static_cast<double>(pose.roll));
        }
        return aiming;
    }

    void Suppress()
    {
        g_fade.Reset();
        g_entry.Reset();
    }
}
