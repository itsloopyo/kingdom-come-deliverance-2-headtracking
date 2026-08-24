#pragma once

#include <cstdint>

#include <cameraunlock/memory/pe_fingerprint.h>

namespace kcd2_ht::builds
{
    // Every address this mod pins to a specific game build, in one place. Call
    // sites read builds::ActiveProfile().Offsets.<name> and never see a literal.
    //
    // The addresses are RVAs into WHGame.dll, not into KingdomCome.exe: the exe
    // is a 1.3 MB launcher stub and the entire engine, including the view system,
    // lives in WHGame.dll. So the fingerprint below is WHGame.dll's too.
    struct OffsetTable
    {
        // CView::Update(CView* this, float frameTime, bool isActive). Slot 2 of
        // CView::vftable. Composes m_viewParams into the camera matrix and ends
        // by calling CCamera::UpdateFrustum on it.
        std::uint32_t kCViewUpdateRva;

        // CCamera::UpdateFrustum(CCamera* this). Rebuilds the frustum planes from
        // the camera matrix; must be re-run after the matrix is modified or
        // culling is computed against the pre-injection view.
        std::uint32_t kCCameraUpdateFrustumRva;

        // Byte offset of CView::m_camera. Its first member is the Matrix34 that
        // CView::Update writes, so this offset IS the matrix address.
        std::uint32_t kCViewCameraOffset;

        // Byte offset of CView::m_viewParams. Never written by this mod - it is
        // what the game reads for aim, raycasts and weapon direction, and leaving
        // it alone is what decouples look from aim. Recorded so the log can prove
        // the layout matched at runtime.
        std::uint32_t kCViewParamsOffset;

        // Byte offsets into CCamera of the two frustum fields the reticle needs:
        // the VERTICAL field of view in radians, and the width-to-height ratio.
        // Both are written by CCamera::SetFrustum, so reading them after
        // CView::Update gives the live values the frame is rendered with.
        std::uint32_t kCCameraFovOffset;
        std::uint32_t kCCameraProjectionRatioOffset;

        // The one helper every HUD cursor goes through:
        //   (this, element, const float posPixels[2], int mode)
        // It subtracts the screen centre from posPixels and scales the remainder
        // into Flash units, so adding the aim offset to posPixels moves the
        // cursor exactly as far as the reticle projection asks.
        std::uint32_t kSetCursorPositionRva;

        // RETURN addresses (call site + 5) of the three calls that position a
        // crosshair, used to tell those apart from the helper's other callers.
        // kCursorCentreReturnRva is the exploration cursor being parked at screen
        // centre; kAimedCursorReturnRva is the same element positioned from a
        // computed point; kCombatCursorReturnRva is CombatCursorCross.
        std::uint32_t kCursorCentreReturnRva;
        std::uint32_t kCombatCursorReturnRva;
        std::uint32_t kAimedCursorReturnRva;

        // The renderer singleton pointer, and the vtable BYTE offsets of the two
        // getters returning the back buffer width and height. Needed because the
        // cursor position is in pixels and the aim projection is an angle.
        std::uint32_t kRendererGlobalRva;
        std::uint32_t kRendererWidthSlot;
        std::uint32_t kRendererHeightSlot;
    };

    struct BuildProfile
    {
        const char* Name;
        cameraunlock::memory::PeFingerprint Fingerprint;
        OffsetTable Offsets;
    };

    // A profile whose hook target is still zero is a placeholder landed ahead of
    // the rederive; it matches by fingerprint but must never activate.
    inline bool IsComplete(const BuildProfile& profile)
    {
        return profile.Offsets.kCViewUpdateRva != 0
            && profile.Offsets.kCCameraUpdateFrustumRva != 0;
    }
}
