#pragma once

#include "cryengine_types.h"

// Where the player is actually aiming, expressed as a screen offset from the
// centre of the head-tracked view. Pure functions over the same two matrices the
// injection produces, so it is testable without a game process and cannot drift
// from the camera composition the way a re-derived Euler formula would.

namespace kcd2_ht
{
    struct AimProjection
    {
        // Tangent of the angle between the tracked view's centre and the aim
        // direction. Positive right is screen-right, positive up is screen-up.
        float tanRight = 0.0f;
        float tanUp = 0.0f;
        // False when the aim direction has passed behind the tracked view plane
        // (a head turn past 90 degrees). The reticle must be hidden, not clamped
        // to an edge, or it points the player away from where their shots go.
        bool inFront = false;
    };

    // @p clean is the camera matrix the engine composed from m_viewParams - the
    // direction the game will fire, raycast and swing along. @p tracked is what
    // ApplyHeadPose returned and what the player actually sees.
    //
    // Only the aim DIRECTION is projected, not a point at some assumed distance.
    // A distance would only matter for the 6DOF lean parallax, and there is no
    // hit distance to raycast for here: assuming one puts the error where it
    // hurts most. At a 3 m guess a 0.3 m lean throws a 30 m bow shot off by more
    // than 5 degrees, where ignoring the parallax costs about half a degree at
    // that range and nothing at all for pure rotation, which is the common case.
    AimProjection ProjectAim(const Matrix34f& clean, const Matrix34f& tracked);

    struct ScreenPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    // Maps a projection onto the back buffer. @p fovRadians is CCamera's VERTICAL
    // field of view and @p projectionRatio its width-to-height ratio, both read
    // straight off the live camera so an FOV slider or an ultrawide aspect needs
    // no calibration constant.
    ScreenPoint ToScreen(const AimProjection& aim, float screenWidth, float screenHeight,
                         float fovRadians, float projectionRatio);
}
