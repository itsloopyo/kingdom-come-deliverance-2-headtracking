// The guard between the tracking pipeline and the game's live camera matrix.
//
// The failure it exists for is reachable from one datagram: the socket binds
// INADDR_ANY, cameraunlock-core only rejects a pose whose values are not finite,
// and PoseInterpolator extrapolates pitch as a plain unwrapped lerp - so a pitch
// near FLT_MAX overflows that lerp to infinity and every later frame's smoothed
// pitch is NaN. ApplyHeadPose turns a NaN angle into a camera basis of NaNs, and
// that basis would be written straight into CView+0xE8 and handed to
// CCamera::UpdateFrustum.

#include "pose_guard.h"

#include <cmath>
#include <limits>

#include "test_support.h"

namespace {

using kcd_tests::Check;

kcd2_ht::HeadPose UsablePose()
{
    kcd2_ht::HeadPose pose;
    pose.yaw = 12.0f;
    pose.pitch = -8.0f;
    pose.roll = 3.0f;
    pose.x = 0.05f;
    pose.y = -0.02f;
    pose.z = -0.10f;
    return pose;
}

void AcceptsRealPoses(int& failures)
{
    Check(failures, kcd2_ht::IsFinitePose(kcd2_ht::HeadPose{}),
          "a zeroed pose passes, so a player with no tracker still gets a clean camera");
    Check(failures, kcd2_ht::IsFinitePose(UsablePose()),
          "an ordinary 6DOF pose passes");

    kcd2_ht::HeadPose extreme;
    extreme.yaw = 179.0f;
    extreme.pitch = -89.0f;
    extreme.roll = 180.0f;
    extreme.x = 0.5f;
    extreme.y = -0.5f;
    extreme.z = 0.5f;
    Check(failures, kcd2_ht::IsFinitePose(extreme),
          "a pose at the far end of every axis still passes - the guard rejects "
          "non-numbers, it does not clamp the player's range");
}

void RejectsEveryPoisonedAxis(int& failures)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    float kcd2_ht::HeadPose::*const axes[] = {
        &kcd2_ht::HeadPose::yaw,   &kcd2_ht::HeadPose::pitch, &kcd2_ht::HeadPose::roll,
        &kcd2_ht::HeadPose::x,     &kcd2_ht::HeadPose::y,     &kcd2_ht::HeadPose::z,
    };
    const char* names[] = { "yaw", "pitch", "roll", "x", "y", "z" };

    for (int i = 0; i < 6; ++i) {
        kcd2_ht::HeadPose poisoned = UsablePose();
        poisoned.*axes[i] = nan;
        Check(failures, !kcd2_ht::IsFinitePose(poisoned),
              std::string("a NaN ") + names[i] + " is rejected, so no NaN reaches the camera matrix");

        poisoned = UsablePose();
        poisoned.*axes[i] = inf;
        Check(failures, !kcd2_ht::IsFinitePose(poisoned),
              std::string("an infinite ") + names[i] + " is rejected");

        poisoned = UsablePose();
        poisoned.*axes[i] = -inf;
        Check(failures, !kcd2_ht::IsFinitePose(poisoned),
              std::string("a negative-infinite ") + names[i] + " is rejected");
    }
}

// Documents WHY the guard is needed rather than assuming it: run the pose the
// interpolator overflow produces through the real injection and confirm the
// matrix it hands back is unusable. Without the guard those twelve floats are
// what CView+0xE8 receives.
void PoisonedPoseProducesAPoisonedMatrix(int& failures)
{
    kcd2_ht::Matrix34f view;
    view.m[0][0] = 1.0f; view.m[1][1] = 1.0f; view.m[2][2] = 1.0f;

    kcd2_ht::HeadPose poisoned = UsablePose();
    poisoned.pitch = std::numeric_limits<float>::quiet_NaN();

    const kcd2_ht::Matrix34f out = kcd2_ht::ApplyHeadPose(view, poisoned, true, true);

    bool anyNonFinite = false;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            if (!std::isfinite(out.m[r][c])) anyNonFinite = true;

    Check(failures, anyNonFinite,
          "a non-finite pitch really does poison the camera matrix, which is what the "
          "guard exists to keep out of CView+0xE8");
}

}  // namespace

int RunPoseGuardTests()
{
    int failures = 0;
    std::cout << "Pose guard tests\n";

    AcceptsRealPoses(failures);
    RejectsEveryPoisonedAxis(failures);
    PoisonedPoseProducesAPoisonedMatrix(failures);

    return kcd_tests::Report("Pose guard tests", failures);
}
