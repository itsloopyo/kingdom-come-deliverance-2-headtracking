#include "ads.h"

#include <atomic>

#include <cameraunlock/ads/ads_blend.h>
#include <cameraunlock/ads/ads_fade.h>
#include <cameraunlock/ads/entry_pose.h>

namespace kcd2_ht::ads
{
    namespace
    {
        using cameraunlock::ads::AdsEntryPose;
        using cameraunlock::ads::AdsFade;

        // How long an aim-reticle observation stands for. The HUD positions the
        // cursor once per rendered frame, so this only has to outlast a frame:
        // at 30 fps that is 33 ms, and 200 ms rides out a stutter without
        // holding the aim on for long enough to see after the weapon drops.
        constexpr std::uint64_t kAimFreshnessMs = 200;

        // Written by the hotkey thread, read by the render thread.
        std::atomic<AdsMode> g_mode{cameraunlock::ads::kDefaultAdsMode};

        // Written by the HUD cursor detour, read by the view hook. Both are game
        // threads and neither blocks on the other.
        //
        // "Seen at all" is its own flag rather than a zero timestamp: the clock
        // is the caller's, a caller is free to start it at zero, and a sentinel
        // that collides with a real reading makes the very first aim of a
        // session read as no aim at all.
        std::atomic<bool> g_sawAimReticle{false};
        std::atomic<std::uint64_t> g_lastAimReticleMs{0};

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
        const AdsMode next = cameraunlock::ads::NextAdsModeTwoSlot(Mode());
        SetMode(next);
        return next;
    }

    void NoteAimReticle(std::uint64_t nowMs)
    {
        g_lastAimReticleMs.store(nowMs, std::memory_order_relaxed);
        g_sawAimReticle.store(true, std::memory_order_relaxed);
    }

    bool IsAiming(std::uint64_t nowMs)
    {
        if (!g_sawAimReticle.load(std::memory_order_relaxed)) return false;
        const std::uint64_t seen = g_lastAimReticleMs.load(std::memory_order_relaxed);
        // A clock that stepped backwards must not read as a fresh observation,
        // so the comparison is one-sided rather than a subtraction.
        return nowMs >= seen && (nowMs - seen) < kAimFreshnessMs;
    }

    bool Apply(HeadPose& pose, std::uint64_t nowMs)
    {
        const bool aiming = IsAiming(nowMs);

        // The fade's `aiming` input is the game's own state, never the gate's
        // verdict. Feeding a verdict back in makes the fade start raising the
        // instant it finishes lowering, several times a second.
        const float scale = g_fade.Update(aiming, nowMs);

        const AdsEntryPose::Pose absolute = ToCore(pose);
        // The caller only reaches here on a live rotation, which is exactly the
        // gate the entry capture needs: capturing off an interpolator that has
        // been reset would freeze a pre-suppression pose for the whole aim.
        const AdsEntryPose::Pose relative = g_entry.Relative(aiming, /*live=*/true, absolute);

        FromCore(cameraunlock::ads::BlendAdsPose(Mode(), scale, absolute, relative), pose);
        return aiming;
    }

    void Suppress()
    {
        g_fade.Reset();
        g_entry.Reset();
        // The observation goes too. A suppressed frame is one the HUD may not
        // have drawn at all, and holding the last one across it would report the
        // sights up on the frame tracking comes back.
        g_sawAimReticle.store(false, std::memory_order_relaxed);
    }
}
