// The ADS half of the tracking gate: what the sights do to the pose the camera
// is given, and what the mode selects between.
//
// This mod's gate is the early-return walk in view_hook.cpp rather than a
// verdict object, and its ADS stage is ads::Apply. So the shape under test is
// "what pose comes out, and does the caller still know the sights are up" -
// which is the same contract a verdict-walk mod states as "the gate is closed
// with the ADS reason, and the ADS flag is still true".

#include "ads.h"

#include <cameraunlock/ads/ads_fade.h>

#include "test_support.h"

namespace {

using kcd_tests::Check;
using cameraunlock::ads::AdsFade;
using cameraunlock::ads::AdsMode;
using kcd2_ht::HeadPose;

HeadPose Head(float yaw, float pitch, float roll,
              float x = 0.0f, float y = 0.0f, float z = 0.0f)
{
    HeadPose p;
    p.yaw = yaw;
    p.pitch = pitch;
    p.roll = roll;
    p.x = x;
    p.y = y;
    p.z = z;
    return p;
}

bool Near(float a, float b, float tolerance = 1e-3f)
{
    const float d = a - b;
    return (d < 0.0f ? -d : d) <= tolerance;
}

// The module holds process-global state, so every case starts from a known one.
void Fresh(AdsMode mode)
{
    kcd2_ht::ads::Suppress();
    kcd2_ht::ads::SetMode(mode);
}

}  // namespace

int RunAdsGateTests()
{
    namespace ads = kcd2_ht::ads;
    int failures = 0;
    std::cout << "ADS gate tests\n";

    {
        Fresh(AdsMode::Paused);
        for (int cycle = 0; cycle < 3; ++cycle) {
            Check(failures, ads::Cycle() == AdsMode::Marker, "paused cycles to marker");
            Check(failures, ads::Cycle() == AdsMode::Tracked, "marker cycles to tracked");
            Check(failures, ads::Cycle() == AdsMode::Paused, "tracked cycles to paused");
        }
    }

    // Paused: the pose fades off the camera and stays off, and the caller is
    // still told the sights are up. That second half is the part a gate alone
    // cannot express - the mode says what tracking does, the flag says what the
    // game is doing, and the per-frame code needs both.
    {
        Fresh(AdsMode::Paused);
        HeadPose pose = Head(20.0f, 10.0f, 7.0f, 0.1f, 0.0f, 0.0f);
        const bool aiming = ads::Apply(pose, 0, true);
        Check(failures, aiming, "paused still reports the sights up");

        HeadPose settled = Head(20.0f, 10.0f, 7.0f, 0.1f, 0.0f, 0.0f);
        ads::Apply(settled, AdsFade::kLowerMs, true);
        Check(failures, Near(settled.yaw, 0.0f) && Near(settled.pitch, 0.0f)
                     && Near(settled.x, 0.0f),
              "and the head is fully off the camera once the fade has run");
        Check(failures, Near(settled.roll, 7.0f),
              "except roll, which a paused aim keeps");
    }

    // The pose is not CUT on the first aiming frame - that jolt is the whole
    // reason the fade exists. A verdict-walk mod states this as "paused holds
    // the gate open until the fade says the pose has gone".
    {
        Fresh(AdsMode::Paused);
        HeadPose pose = Head(20.0f, 0.0f, 0.0f);
        ads::Apply(pose, 0, true);
        Check(failures, Near(pose.yaw, 20.0f),
              "the first aiming frame has not moved the pose yet");

        HeadPose mid = Head(20.0f, 0.0f, 0.0f);
        ads::Apply(mid, AdsFade::kLowerMs / 2, true);
        Check(failures, mid.yaw > 0.0f && mid.yaw < 20.0f,
              "and the pose eases off rather than being cut in one frame");
    }

    // Tracked: tracking stays live through the aim, measured from the frame the
    // sights came up on, so the swing onto the aim point is the same one paused
    // makes and the head moves the view again from there.
    {
        Fresh(AdsMode::Tracked);
        HeadPose entry = Head(20.0f, 10.0f, 0.0f);
        ads::Apply(entry, 0, true);

        HeadPose held = Head(20.0f, 10.0f, 0.0f);
        Check(failures, ads::Apply(held, AdsFade::kLowerMs, true), "tracked reports the sights up");
        Check(failures, Near(held.yaw, 0.0f) && Near(held.pitch, 0.0f),
              "a head that has not moved since the sights came up is identity");

        HeadPose moved = Head(35.0f, 10.0f, 0.0f);
        ads::Apply(moved, AdsFade::kLowerMs + 10, true);
        Check(failures, Near(moved.yaw, 15.0f),
              "and tracking carries on from there rather than from centre");
    }

    // Hip fire is untouched in both modes. Anything else would mean the whole of
    // normal play ran through the ADS path.
    {
        for (const AdsMode mode : { AdsMode::Paused, AdsMode::Marker, AdsMode::Tracked })
        {
            Fresh(mode);
            HeadPose pose = Head(20.0f, 10.0f, 7.0f, 0.1f, 0.2f, 0.3f);
            const bool aiming = ads::Apply(pose, 1000, false);
            Check(failures, !aiming, "hip fire does not report the sights up");
            Check(failures, Near(pose.yaw, 20.0f) && Near(pose.pitch, 10.0f)
                         && Near(pose.roll, 7.0f) && Near(pose.x, 0.1f)
                         && Near(pose.y, 0.2f) && Near(pose.z, 0.3f),
                  "and passes the pose through untouched");
        }
    }

    // Suppression - a menu, the master toggle, a tracker dropout - outranks the
    // sights: it clears the ADS state rather than leaving a stale flag behind,
    // and the next aim re-enters cleanly instead of resuming at the old offset.
    {
        Fresh(AdsMode::Tracked);
        HeadPose entry = Head(90.0f, 0.0f, 0.0f);
        ads::Apply(entry, 0, true);

        ads::Suppress();

        HeadPose after = Head(100.0f, 0.0f, 0.0f);
        ads::Apply(after, AdsFade::kLowerMs * 4, true);
        Check(failures, Near(after.yaw, 100.0f),
              "the frame after a suppression re-enters at the pose it is holding");

        HeadPose later = Head(110.0f, 0.0f, 0.0f);
        ads::Apply(later, AdsFade::kLowerMs * 5, true);
        Check(failures, Near(later.yaw, 10.0f),
              "and measures the rest of the aim from there, not from before the suppression");
    }

    {
        Fresh(AdsMode::Paused);
        HeadPose entry = Head(20.0f, 10.0f, 3.0f);
        ads::Apply(entry, 0, true);
        HeadPose held = Head(35.0f, 10.0f, 3.0f);
        ads::Apply(held, AdsFade::kLowerMs, true);
        Check(failures, Near(held.yaw, 0.0f), "paused holds aim despite head movement");

        ads::Cycle();
        ads::Cycle();
        HeadPose tracked = Head(35.0f, 10.0f, 3.0f);
        ads::Apply(tracked, AdsFade::kLowerMs + 1, true);
        Check(failures, Near(tracked.yaw, 15.0f), "cycling while aimed activates relative tracking");

        ads::Cycle();
        HeadPose paused = Head(35.0f, 10.0f, 3.0f);
        ads::Apply(paused, AdsFade::kLowerMs + 2, true);
        Check(failures, Near(paused.yaw, 0.0f), "cycling again pauses the same aim");

        HeadPose release = Head(35.0f, 10.0f, 3.0f);
        Check(failures, !ads::Apply(release, 1000, false), "weapon release immediately clears aim state");
        HeadPose raised = Head(35.0f, 10.0f, 3.0f);
        ads::Apply(raised, 1000 + AdsFade::kRaiseMs, false);
        Check(failures, Near(raised.yaw, 35.0f), "freelook returns after the release fade");
    }

    Fresh(AdsMode::Paused);
    return kcd_tests::Report("ADS gate tests", failures);
}
