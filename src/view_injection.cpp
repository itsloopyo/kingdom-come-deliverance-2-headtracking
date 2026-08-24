#include "view_injection.h"

#include <cmath>

namespace kcd2_ht
{
    namespace
    {
        constexpr float kDegToRad = 0.01745329252f;

        Matrix33f Multiply(const Matrix33f& a, const Matrix33f& b)
        {
            Matrix33f out;
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c)
                    out.m[r][c] = a.m[r][0] * b.m[0][c]
                                + a.m[r][1] * b.m[1][c]
                                + a.m[r][2] * b.m[2][c];
            return out;
        }

        Matrix33f RotationX(float rad)
        {
            const float s = std::sin(rad), c = std::cos(rad);
            Matrix33f r;
            r.m[1][1] = c; r.m[1][2] = -s;
            r.m[2][1] = s; r.m[2][2] = c;
            return r;
        }

        Matrix33f RotationY(float rad)
        {
            const float s = std::sin(rad), c = std::cos(rad);
            Matrix33f r;
            r.m[0][0] = c;  r.m[0][2] = s;
            r.m[2][0] = -s; r.m[2][2] = c;
            return r;
        }

        Matrix33f RotationZ(float rad)
        {
            const float s = std::sin(rad), c = std::cos(rad);
            Matrix33f r;
            r.m[0][0] = c; r.m[0][1] = -s;
            r.m[1][0] = s; r.m[1][1] = c;
            return r;
        }

        Matrix33f RotationOf(const Matrix34f& view)
        {
            Matrix33f r;
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col)
                    r.m[row][col] = view.m[row][col];
            return r;
        }

        Vec3f Transform(const Matrix33f& r, const Vec3f& v)
        {
            return {
                r.m[0][0] * v.x + r.m[0][1] * v.y + r.m[0][2] * v.z,
                r.m[1][0] * v.x + r.m[1][1] * v.y + r.m[1][2] * v.z,
                r.m[2][0] * v.x + r.m[2][1] * v.y + r.m[2][2] * v.z,
            };
        }
    }

    Matrix33f HeadRotationLocal(float yawDeg, float pitchDeg, float rollDeg)
    {
        return Multiply(Multiply(RotationZ(yawDeg * kDegToRad),
                                 RotationX(pitchDeg * kDegToRad)),
                        RotationY(rollDeg * kDegToRad));
    }

    Matrix33f WorldYaw(float yawDeg)
    {
        return RotationZ(yawDeg * kDegToRad);
    }

    Matrix34f ApplyHeadPose(const Matrix34f& view, const HeadPose& pose,
                            bool worldSpaceYaw, bool positionEnabled)
    {
        const Matrix33f base = RotationOf(view);

        // The protocol's yaw and roll run opposite to CryEngine's camera-local
        // +Z and +Y rotations. Converted once, here at the engine boundary, the
        // same way the position z is - never with a user-facing inversion.
        const float yaw = -pose.yaw;
        const float roll = -pose.roll;

        Matrix33f rotated;
        if (worldSpaceYaw)
        {
            // Head yaw about world up, pitch and roll still camera-local, so a
            // yaw while the game camera is pitched sweeps the horizon instead of
            // tipping it.
            rotated = Multiply(WorldYaw(yaw),
                               Multiply(base, HeadRotationLocal(0.0f, pose.pitch, roll)));
        }
        else
        {
            rotated = Multiply(base, HeadRotationLocal(yaw, pose.pitch, roll));
        }

        Matrix34f out;
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                out.m[r][c] = rotated.m[r][c];

        Vec3f position = view.Translation();
        if (positionEnabled)
        {
            // The core's z is negative-forward; CryEngine's camera-local forward
            // is +Y. Its x runs opposite to the protocol's, the same way yaw and
            // roll do above. Convert once, here, at the engine boundary - never
            // with a user-facing inversion setting, which would put the generous
            // forward limit on the backward lean.
            const Vec3f local{ -pose.x, -pose.z, pose.y };
            // Applied through the ORIGINAL basis: the lean follows where the body
            // faces, not where the head is currently looking.
            const Vec3f world = Transform(base, local);
            position.x += world.x;
            position.y += world.y;
            position.z += world.z;
        }
        out.SetTranslation(position);
        return out;
    }
}
