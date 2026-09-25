// What aiming a bow or crossbow does to the pose the camera is given: rotation
// passes through untouched, and the lean eases out while the sights are up
// (sights locked) and back in when they come down, unless true free look is on.

#include "ads.h"

#include <cameraunlock/ads/ads_fade.h>

#include "test_support.h"

namespace {

using kcd_tests::Check;
using cameraunlock::ads::AdsFade;
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

bool RotationIs(const HeadPose& pose, float yaw, float pitch, float roll)
{
    return pose.yaw == yaw && pose.pitch == pitch && pose.roll == roll;
}

}  // namespace

int RunAdsGateTests()
{
    namespace ads = kcd2_ht::ads;
    int failures = 0;
    std::cout << "ADS gate tests\n";

    // Hip fire is untouched. Anything else would mean the whole of normal play
    // ran through the ADS path.
    {
        ads::Suppress();
        HeadPose pose = Head(20.0f, 10.0f, 7.0f, 0.1f, 0.2f, 0.3f);
        ads::Apply(pose, 1000, false, false);
        Check(failures, RotationIs(pose, 20.0f, 10.0f, 7.0f)
                     && pose.x == 0.1f && pose.y == 0.2f && pose.z == 0.3f,
              "hip fire passes the pose through untouched");
    }

    // Sights up: the lean is gone once the fade has run, and rotation, roll
    // included, is the absolute pose, unscaled and not made relative.
    {
        ads::Suppress();
        HeadPose first = Head(20.0f, 10.0f, 7.0f, 0.1f, 0.2f, 0.3f);
        ads::Apply(first, 0, true, false);
        Check(failures, RotationIs(first, 20.0f, 10.0f, 7.0f),
              "raising the sights does not move the view");

        HeadPose settled = Head(25.0f, -5.0f, 9.0f, 0.1f, 0.2f, 0.3f);
        ads::Apply(settled, AdsFade::kLowerMs, true, false);
        Check(failures, RotationIs(settled, 25.0f, -5.0f, 9.0f),
              "rotation carries on through the aim, absolute and unscaled");
        Check(failures, Near(settled.x, 0.0f) && Near(settled.y, 0.0f) && Near(settled.z, 0.0f),
              "and the lean is fully eased out once the fade has run");
    }

    // Mid-transition the lean is scaled by the fade, never cut in one frame, and
    // rotation is still untouched.
    {
        ads::Suppress();
        HeadPose entry = Head(20.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(entry, 0, true, false);

        HeadPose mid = Head(20.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(mid, AdsFade::kLowerMs / 2, true, false);
        Check(failures, mid.x > 0.0f && mid.x < 0.2f,
              "the lean eases out rather than being cut in one frame");
        Check(failures, RotationIs(mid, 20.0f, 0.0f, 0.0f),
              "rotation is untouched mid-transition");
    }

    // A reversal continues from where the transition is. A tap of the aim
    // button is the common case: the lean must not step back to full.
    {
        ads::Suppress();
        HeadPose down = Head(0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(down, 0, true, false);
        HeadPose partway = Head(0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(partway, AdsFade::kLowerMs / 2, true, false);

        HeadPose reversed = Head(0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(reversed, AdsFade::kLowerMs / 2, false, false);
        Check(failures, Near(reversed.x, partway.x),
              "releasing mid-transition starts back from where the lean was");

        HeadPose back = Head(0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(back, AdsFade::kLowerMs / 2 + AdsFade::kRaiseMs, false, false);
        Check(failures, Near(back.x, 0.2f), "and the lean is fully back after the raise");
    }

    // True free look: the lean stays in full through the aim.
    {
        ads::Suppress();
        HeadPose first = Head(20.0f, 10.0f, 7.0f, 0.1f, 0.2f, 0.3f);
        ads::Apply(first, 0, true, true);
        HeadPose settled = Head(20.0f, 10.0f, 7.0f, 0.1f, 0.2f, 0.3f);
        ads::Apply(settled, AdsFade::kLowerMs * 2, true, true);
        Check(failures, RotationIs(settled, 20.0f, 10.0f, 7.0f)
                     && settled.x == 0.1f && settled.y == 0.2f && settled.z == 0.3f,
              "true free look passes the whole pose through while aiming");
    }

    // Toggling true free look mid-aim rides the same fade: the lean slides back
    // in from where it was rather than stepping.
    {
        ads::Suppress();
        HeadPose down = Head(0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(down, 0, true, false);
        HeadPose partway = Head(0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(partway, AdsFade::kLowerMs / 2, true, false);

        HeadPose toggled = Head(0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(toggled, AdsFade::kLowerMs / 2, true, true);
        Check(failures, Near(toggled.x, partway.x),
              "switching to true free look mid-transition starts from where the lean was");

        HeadPose full = Head(0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(full, AdsFade::kLowerMs / 2 + AdsFade::kRaiseMs, true, true);
        Check(failures, Near(full.x, 0.2f), "and the full lean is back once the fade has run");
    }

    // Suppression - a menu, the master toggle, a tracker dropout - drops the
    // fade, so the next frame starts from the hip.
    {
        ads::Suppress();
        HeadPose aimed = Head(0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(aimed, 0, true, false);
        ads::Apply(aimed, AdsFade::kLowerMs, true, false);

        ads::Suppress();

        HeadPose after = Head(0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f);
        ads::Apply(after, AdsFade::kLowerMs * 4, false, false);
        Check(failures, Near(after.x, 0.2f),
              "the frame after a suppression carries the full lean at the hip");
    }

    ads::Suppress();
    return kcd_tests::Report("ADS gate tests", failures);
}
