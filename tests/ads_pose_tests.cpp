// The entry-relative pose the tracked ADS mode feeds the camera.
//
// The seam and the capture timing are invisible from a settings or a gate test
// and both are easy to regress, so they get their own suite. Every case here is
// a frame the player either sees their head in or does not, and none of it is
// reachable from a test with a game in the loop.

#include <cameraunlock/ads/ads_blend.h>
#include <cameraunlock/ads/ads_fade.h>
#include <cameraunlock/ads/entry_pose.h>

#include "test_support.h"

namespace {

using kcd_tests::Check;
using cameraunlock::ads::AdsEntryPose;
using cameraunlock::ads::AdsFade;
using cameraunlock::ads::AdsMode;

AdsEntryPose::Pose MakePose(float pitch, float yaw, float roll,
                            float x = 0.0f, float y = 0.0f, float z = 0.0f)
{
    AdsEntryPose::Pose p;
    p.pitch = pitch;
    p.yaw = yaw;
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

}  // namespace

int RunAdsPoseTests()
{
    int failures = 0;
    std::cout << "ADS entry-pose tests\n";

    // Hip fire is the absolute pose, untouched. Anything else would mean the
    // whole of normal play ran through a relative frame.
    {
        AdsEntryPose entry;
        const AdsEntryPose::Pose absolute = MakePose(10.0f, 20.0f, 5.0f, 0.1f, 0.2f, 0.3f);
        const AdsEntryPose::Pose out = entry.Relative(/*aiming=*/false, /*live=*/true, absolute);
        Check(failures, Near(out.pitch, 10.0f) && Near(out.yaw, 20.0f) && Near(out.roll, 5.0f)
                     && Near(out.x, 0.1f) && Near(out.y, 0.2f) && Near(out.z, 0.3f),
              "hip fire passes the absolute pose straight through");
        Check(failures, !entry.HasEntry(), "hip fire holds no entry pose");
    }

    // The entry frame is identity, which is what makes the tracked modes arrive
    // at the same place the paused fade does.
    {
        AdsEntryPose entry;
        const AdsEntryPose::Pose at = MakePose(10.0f, 20.0f, 5.0f, 0.1f, 0.2f, 0.3f);
        const AdsEntryPose::Pose out = entry.Relative(/*aiming=*/true, /*live=*/true, at);
        Check(failures, Near(out.pitch, 0.0f) && Near(out.yaw, 0.0f)
                     && Near(out.x, 0.0f) && Near(out.y, 0.0f) && Near(out.z, 0.0f),
              "the frame the sights come up on is identity on every relative axis");
        Check(failures, Near(out.roll, 5.0f),
              "roll is NOT made relative - a tilt the player is holding stays held");
    }

    // Capturing off an interpolator that has been reset freezes a
    // pre-suppression pose and holds the whole aim at that offset.
    {
        AdsEntryPose entry;
        const AdsEntryPose::Pose stale = MakePose(1.0f, 2.0f, 0.0f);
        const AdsEntryPose::Pose out = entry.Relative(/*aiming=*/true, /*live=*/false, stale);
        Check(failures, !entry.HasEntry(),
              "a dead rotation does not capture an entry pose");
        Check(failures, Near(out.pitch, 1.0f) && Near(out.yaw, 2.0f),
              "and passes the pose through until a live one arrives");

        const AdsEntryPose::Pose fresh = MakePose(30.0f, 40.0f, 0.0f);
        entry.Relative(/*aiming=*/true, /*live=*/true, fresh);
        Check(failures, entry.HasEntry(), "the first live frame is what captures");
        const AdsEntryPose::Pose after =
            entry.Relative(/*aiming=*/true, /*live=*/true, MakePose(35.0f, 40.0f, 0.0f));
        Check(failures, Near(after.pitch, 5.0f),
              "and the aim is measured from the live frame, not the dead one");
    }

    // A plain subtraction reads a 10 degree move across the seam as -350 and
    // whips the view a full turn the wrong way.
    {
        AdsEntryPose entry;
        entry.Relative(/*aiming=*/true, /*live=*/true, MakePose(0.0f, 175.0f, 0.0f));
        const AdsEntryPose::Pose out =
            entry.Relative(/*aiming=*/true, /*live=*/true, MakePose(0.0f, -175.0f, 0.0f));
        Check(failures, Near(out.yaw, 10.0f),
              "yaw crosses the -180/180 seam the short way");
    }
    {
        Check(failures, Near(AdsEntryPose::ShortestDeltaDegrees(-179.0f, 179.0f), 2.0f),
              "and the other way round too");
        Check(failures, Near(AdsEntryPose::ShortestDeltaDegrees(10.0f, 350.0f), 20.0f),
              "and for an unwrapped pair");
    }

    // Position goes relative with the aim axes: the sights sit on the muzzle
    // line, so an eye offset from it moves the sight picture off the target.
    {
        AdsEntryPose entry;
        entry.Relative(/*aiming=*/true, /*live=*/true, MakePose(0.0f, 0.0f, 0.0f, 0.10f, 0.20f, 0.30f));
        const AdsEntryPose::Pose out =
            entry.Relative(/*aiming=*/true, /*live=*/true,
                           MakePose(0.0f, 0.0f, 0.0f, 0.15f, 0.25f, 0.35f));
        Check(failures, Near(out.x, 0.05f) && Near(out.y, 0.05f) && Near(out.z, 0.05f),
              "position is measured from the entry frame on all three axes");
    }

    // Dropping it the moment aiming ends would make the relative pose the
    // absolute pose, so the return ramp interpolates a value with itself and the
    // view steps by the whole entry offset in one frame. The entry pose has to
    // outlive the aim by the length of the ride back, which is what the caller's
    // suppression contract provides; what IS required here is that lowering the
    // weapon stops producing a relative pose at all.
    {
        AdsEntryPose entry;
        entry.Relative(/*aiming=*/true, /*live=*/true, MakePose(0.0f, 90.0f, 0.0f));
        Check(failures, entry.HasEntry(), "the aim holds an entry pose");
        const AdsEntryPose::Pose out =
            entry.Relative(/*aiming=*/false, /*live=*/true, MakePose(0.0f, 90.0f, 0.0f));
        Check(failures, !entry.HasEntry() && Near(out.yaw, 90.0f),
              "lowering the weapon drops the entry pose and hands back the absolute one");
    }

    // Suppression drops it, so the next aim re-enters cleanly rather than
    // resuming against a pose from before the menu.
    {
        AdsEntryPose entry;
        entry.Relative(/*aiming=*/true, /*live=*/true, MakePose(0.0f, 90.0f, 0.0f));
        entry.Reset();
        Check(failures, !entry.HasEntry(), "an explicit reset drops the entry pose");
        const AdsEntryPose::Pose out =
            entry.Relative(/*aiming=*/true, /*live=*/true, MakePose(0.0f, 100.0f, 0.0f));
        Check(failures, Near(out.yaw, 0.0f),
              "and the aim recaptures rather than resuming at the old offset");
    }

    // The blend is what makes paused and tracked start the same way and diverge
    // afterwards. Roll is in neither fade.
    {
        const AdsEntryPose::Pose absolute = MakePose(10.0f, 20.0f, 7.0f, 0.1f, 0.0f, 0.0f);
        const AdsEntryPose::Pose relative = MakePose(2.0f, 3.0f, 7.0f, 0.02f, 0.0f, 0.0f);

        const AdsEntryPose::Pose hip =
            cameraunlock::ads::BlendAdsPose(AdsMode::Paused, 1.0f, absolute, relative);
        Check(failures, Near(hip.pitch, 10.0f) && Near(hip.yaw, 20.0f),
              "at the hip the paused blend is the full pose");

        const AdsEntryPose::Pose aimed =
            cameraunlock::ads::BlendAdsPose(AdsMode::Paused, 0.0f, absolute, relative);
        Check(failures, Near(aimed.pitch, 0.0f) && Near(aimed.yaw, 0.0f)
                     && Near(aimed.x, 0.0f),
              "with the sights up the paused blend is nothing at all");
        Check(failures, Near(aimed.roll, 7.0f),
              "but roll survives the paused fade");

        const AdsEntryPose::Pose tracked =
            cameraunlock::ads::BlendAdsPose(AdsMode::Tracked, 0.0f, absolute, relative);
        Check(failures, Near(tracked.pitch, 2.0f) && Near(tracked.yaw, 3.0f)
                     && Near(tracked.x, 0.02f),
              "with the sights up the tracked blend is the entry-relative pose");
        Check(failures, Near(tracked.roll, 7.0f),
              "and roll is absolute there too");
    }

    // A tap of the aim button is the most common input there is, and starting
    // each leg at its own endpoint makes it remove a fully applied pose in one
    // frame.
    {
        AdsFade fade;
        Check(failures, Near(fade.Update(false, 0), 1.0f), "the fade rests at the hip");
        fade.Update(true, 0);
        const float partial = fade.Update(true, AdsFade::kLowerMs / 2);
        Check(failures, partial > 0.0f && partial < 1.0f, "and eases rather than switching");
        const float reversed = fade.Update(false, AdsFade::kLowerMs / 2);
        Check(failures, Near(reversed, partial),
              "a reversal starts from where the transition is, not from where it was heading");
    }
    {
        AdsFade fade;
        fade.Update(true, 0);
        Check(failures, Near(fade.Update(true, AdsFade::kLowerMs), 0.0f),
              "the pose is fully off by the end of the lower");
        fade.Reset();
        Check(failures, Near(fade.Update(false, AdsFade::kLowerMs), 1.0f),
              "and a reset returns it to the hip immediately");
    }

    return kcd_tests::Report("ADS entry-pose tests", failures);
}
