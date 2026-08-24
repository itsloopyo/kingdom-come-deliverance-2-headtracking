#pragma once

#include "cryengine_types.h"

// The whole camera modification, expressed as pure functions over a Matrix34 so
// it can be tested without a game process. The hook in headtracking_mod.cpp does
// nothing but read CView+0xE8, call ApplyHeadPose, write it back and re-run
// CCamera::UpdateFrustum.

namespace kcd2_ht
{
    struct HeadPose
    {
        // Degrees, as the tracking pipeline produces them. The engine boundary
        // in ApplyHeadPose negates yaw and roll.
        float yaw = 0.0f;
        float pitch = 0.0f;
        float roll = 0.0f;
        // Metres, in the core's convention: +x right, +y up, NEGATIVE z is the
        // forward lean. The engine boundary in ApplyHeadPose negates x and z.
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    // Rotation about the camera-local axes, in CryEngine's convention:
    //   yaw   -> +Z (up)      positive turns the view left
    //   pitch -> +X (right)   positive raises the view
    //   roll  -> +Y (forward) positive rolls the view clockwise on screen
    // Composed yaw * pitch * roll, matching the shared YPR order.
    Matrix33f HeadRotationLocal(float yawDeg, float pitchDeg, float rollDeg);

    // Yaw taken about the WORLD up axis instead of the camera's own, so the
    // horizon stays level when the game camera is already pitched. Pitch and
    // roll stay camera-local.
    Matrix33f WorldYaw(float yawDeg);

    // Applies @p pose to @p view, returning the camera matrix to hand back to the
    // engine. Rotation is composed onto the camera basis; the position offset is
    // applied in the ORIGINAL (pre-head-rotation) camera space, so leaning
    // follows the body's orientation rather than the head's.
    //
    // worldSpaceYaw selects the horizon-locked yaw above. positionEnabled gates
    // the translation independently of the rotation, which is what the position
    // hotkey toggles.
    Matrix34f ApplyHeadPose(const Matrix34f& view, const HeadPose& pose,
                            bool worldSpaceYaw, bool positionEnabled);
}
