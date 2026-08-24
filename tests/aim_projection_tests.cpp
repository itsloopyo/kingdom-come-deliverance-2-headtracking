// The reticle litmus tests. Every one of these is a shipped-and-fixed bug
// somewhere in the fleet: a projection that agrees with the camera on a single
// axis and then drifts the moment two are combined looks right in every quick
// check and wrong in the only situation that matters.
//
// The projection here is derived from the injected matrices rather than from
// Euler angles, so these tests are really asserting that the reticle and the
// camera cannot disagree - and they run against ApplyHeadPose itself, not
// against a reconstruction of it.

#include "test_support.h"

#include "aim_projection.h"
#include "view_injection.h"

#include <cmath>

using kcd2_ht::AimProjection;
using kcd2_ht::ApplyHeadPose;
using kcd2_ht::HeadPose;
using kcd2_ht::Matrix34f;
using kcd2_ht::ProjectAim;
using kcd2_ht::ScreenPoint;
using kcd2_ht::ToScreen;
using kcd_tests::Check;
using kcd_tests::NearEqual;

namespace
{
    Matrix34f Identity()
    {
        Matrix34f m;
        m.m[0][0] = 1.0f; m.m[1][1] = 1.0f; m.m[2][2] = 1.0f;
        return m;
    }

    // A camera pitched down by @p degrees, so world-yaw mode has something to
    // conjugate through. Pitch is about the camera's right axis (+X).
    Matrix34f PitchedDown(float degrees)
    {
        const float r = degrees * 0.01745329252f;
        const float s = std::sin(-r), c = std::cos(-r);
        Matrix34f m;
        m.m[0][0] = 1.0f;
        m.m[1][1] = c; m.m[1][2] = -s;
        m.m[2][1] = s; m.m[2][2] = c;
        return m;
    }

    AimProjection Project(const Matrix34f& clean, const HeadPose& pose, bool worldYaw)
    {
        return ProjectAim(clean, ApplyHeadPose(clean, pose, worldYaw, false));
    }
}

int RunAimProjectionTests()
{
    int failures = 0;
    std::cout << "Aim projection tests\n";

    const Matrix34f level = Identity();

    {
        HeadPose pose;
        const AimProjection aim = Project(level, pose, false);
        Check(failures, aim.inFront && NearEqual(aim.tanRight, 0.0) && NearEqual(aim.tanUp, 0.0),
              "a still head leaves the reticle at screen centre");
    }

    {
        // Litmus 1: pure roll must not move the reticle. Roll turns about the
        // view axis, so the aim point it turns around is the centre itself.
        HeadPose pose; pose.roll = 25.0f;
        const AimProjection aim = Project(level, pose, false);
        Check(failures, aim.inFront && NearEqual(aim.tanRight, 0.0) && NearEqual(aim.tanUp, 0.0),
              "pure roll keeps the reticle at centre");
    }

    {
        // Litmus 2: pure pitch moves it vertically and only vertically. Looking
        // up puts the aim point lower in the picture.
        HeadPose pose; pose.pitch = 20.0f;
        const AimProjection aim = Project(level, pose, false);
        Check(failures, aim.inFront && NearEqual(aim.tanRight, 0.0),
              "pure pitch does not move the reticle sideways");
        Check(failures, aim.tanUp < 0.0f,
              "pitching the head up drops the reticle below centre");
        Check(failures, NearEqual(aim.tanUp, -std::tan(20.0 * 0.01745329252)),
              "the vertical offset is the tangent of the head pitch");
    }

    {
        // Pure yaw is the mirror of the above. The boundary negates tracker yaw,
        // so a positive yaw turns the view right and the aim point falls to the
        // left of centre.
        HeadPose pose; pose.yaw = 20.0f;
        const AimProjection aim = Project(level, pose, false);
        Check(failures, aim.inFront && NearEqual(aim.tanUp, 0.0),
              "pure yaw does not move the reticle vertically");
        Check(failures, aim.tanRight < 0.0f,
              "turning the head right leaves the reticle left of centre");
    }

    {
        // Litmus 3: pitch combined with roll. The camera rolls the screen frame
        // around the view axis, so the offset must turn with it and keep its
        // length - a horizontal wander that grows with roll is the classic
        // symptom of a projection derived from per-axis tangents instead.
        HeadPose pitchOnly; pitchOnly.pitch = 20.0f;
        const AimProjection a = Project(level, pitchOnly, false);

        HeadPose pitchAndRoll = pitchOnly; pitchAndRoll.roll = 35.0f;
        const AimProjection b = Project(level, pitchAndRoll, false);

        const double lenA = std::sqrt(static_cast<double>(a.tanRight * a.tanRight + a.tanUp * a.tanUp));
        const double lenB = std::sqrt(static_cast<double>(b.tanRight * b.tanRight + b.tanUp * b.tanUp));
        Check(failures, NearEqual(lenA, lenB),
              "roll turns the pitch offset about centre without changing its distance");

        // Turned by exactly the angle the picture rolled, which is the NEGATED
        // tracker roll - the engine boundary in ApplyHeadPose flips it. Asserting
        // the raw tracker sign here would put the reticle 180 degrees out of
        // phase with the camera, which is the failure this test exists to catch.
        const double roll = -35.0 * 0.01745329252;
        const double expectedRight = a.tanRight * std::cos(roll) - a.tanUp * std::sin(roll);
        const double expectedUp = a.tanRight * std::sin(roll) + a.tanUp * std::cos(roll);
        Check(failures, NearEqual(b.tanRight, expectedRight) && NearEqual(b.tanUp, expectedUp),
              "the combined pose turns the offset by exactly the roll angle");
    }

    {
        // Litmus 4: world-space yaw while the camera looks straight down. Head
        // yaw is then a pure spin about the view axis, the world turns, and the
        // reticle must sit dead centre. A projection that treats head yaw as
        // camera-local sweeps it through an arc instead.
        const Matrix34f down = PitchedDown(90.0f);
        HeadPose pose; pose.yaw = 40.0f;
        const AimProjection aim = Project(down, pose, true);
        Check(failures, aim.inFront && NearEqual(aim.tanRight, 0.0) && NearEqual(aim.tanUp, 0.0),
              "world-space yaw looking straight down spins the world, not the reticle");
    }

    {
        // World yaw at the horizon has to behave like local yaw, or the mode
        // switch itself would move the reticle.
        HeadPose pose; pose.yaw = 20.0f;
        const AimProjection world = Project(level, pose, true);
        const AimProjection local = Project(level, pose, false);
        Check(failures, NearEqual(world.tanRight, local.tanRight)
                     && NearEqual(world.tanUp, local.tanUp),
              "at the horizon the two yaw modes project identically");
    }

    {
        // Past a right angle the aim direction is behind the picture. Hiding the
        // reticle is the only honest answer; clamping it to an edge points the
        // player away from where the shot goes.
        HeadPose pose; pose.yaw = 100.0f;
        const AimProjection aim = Project(level, pose, false);
        Check(failures, !aim.inFront, "an aim point behind the view is reported as not drawable");
    }

    {
        // Screen mapping. A vertical FOV of 90 degrees makes tan(half) exactly
        // 1, so an offset of 1 lands on the top edge and the horizontal offset
        // is divided by the projection ratio.
        AimProjection aim; aim.inFront = true; aim.tanRight = 0.0f; aim.tanUp = 1.0f;
        const ScreenPoint top = ToScreen(aim, 1920.0f, 1080.0f, 1.57079632679f, 16.0f / 9.0f);
        Check(failures, NearEqual(top.x, 960.0) && NearEqual(top.y, 0.0),
              "a full up-offset maps to the top edge, horizontally centred");

        aim.tanUp = 0.0f;
        aim.tanRight = 16.0f / 9.0f;
        const ScreenPoint rightEdge = ToScreen(aim, 1920.0f, 1080.0f, 1.57079632679f, 16.0f / 9.0f);
        Check(failures, NearEqual(rightEdge.x, 1920.0) && NearEqual(rightEdge.y, 540.0),
              "the projection ratio widens the horizontal mapping, not the vertical");

        // Same head pose on an ultrawide: the reticle sits closer to centre
        // horizontally because the same angle covers fewer degrees of the
        // wider picture. Read from the live camera, so no calibration constant.
        aim.tanRight = 0.5f;
        const ScreenPoint wide = ToScreen(aim, 3840.0f, 1080.0f, 1.57079632679f, 32.0f / 9.0f);
        const ScreenPoint normal = ToScreen(aim, 1920.0f, 1080.0f, 1.57079632679f, 16.0f / 9.0f);
        const double wideFraction = (wide.x - 1920.0) / 1920.0;
        const double normalFraction = (normal.x - 960.0) / 960.0;
        Check(failures, NearEqual(wideFraction, normalFraction * 0.5),
              "doubling the aspect halves the reticle's fraction of the half-width");
    }

    return kcd_tests::Report("Aim projection", failures);
}
