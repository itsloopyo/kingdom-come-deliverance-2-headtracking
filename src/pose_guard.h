#pragma once

#include <cmath>

#include "view_injection.h"

// The last check between the tracking pipeline and the game's live camera.
//
// The pose reaches this mod from a UDP socket bound to INADDR_ANY, so anything
// on the network can choose its magnitude. cameraunlock-core rejects a datagram
// whose values are not finite, but a merely HUGE one passes: PoseInterpolator
// extrapolates pitch past the newest sample as a plain lerp with no wrap, so one
// packet carrying a pitch near FLT_MAX overflows that lerp to infinity, and
// TrackingProcessor's smoothed state is NaN from then on. Writing that into
// CView+0xE8 hands CCamera::UpdateFrustum a basis of NaNs and the frame is
// rendered through a frustum with no planes.
//
// So the pose is checked where it stops being ours and starts being the game's,
// rather than trusted because the packet parser upstream validated something
// weaker.

namespace kcd2_ht
{
    inline bool IsFinitePose(const HeadPose& pose)
    {
        return std::isfinite(pose.yaw) && std::isfinite(pose.pitch) && std::isfinite(pose.roll)
            && std::isfinite(pose.x) && std::isfinite(pose.y) && std::isfinite(pose.z);
    }
}
