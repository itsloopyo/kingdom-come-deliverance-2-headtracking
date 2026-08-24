// Consistency checks on the shipped build profile. The RVAs themselves can only
// be confirmed against the game, but the relationships between them are
// checkable here, and each one has a failure mode that reaches a player: a zero
// hook RVA leaves the mod permanently dormant, and a camera offset that collides
// with m_viewParams would write over the values the game aims with.

#include "builds/build_profile.h"

#include "test_support.h"

namespace kcd2_ht::builds {
    extern const BuildProfile kSteamProfile_20260619;
}

namespace {

using kcd_tests::Check;

}  // namespace

int RunBuildProfileTests()
{
    int failures = 0;
    std::cout << "Build profile tests\n";

    const kcd2_ht::builds::BuildProfile& profile = kcd2_ht::builds::kSteamProfile_20260619;
    const kcd2_ht::builds::OffsetTable& offsets = profile.Offsets;

    // Read from the shipped Bin\Win64MasterMasterSteamPGO\WHGame.dll - not the exe, which is a
    // launcher stub with no camera code in it.
    Check(failures, profile.Fingerprint.TimeDateStamp == 0x6A350E20u
                 && profile.Fingerprint.SizeOfImage == 0x05B2D000u
                 && profile.Fingerprint.CheckSum == 0x00000000u,
          "the Steam 2026-06-19 WHGame.dll fingerprint is unchanged");

    Check(failures, IsComplete(profile),
          "the profile is complete, so the mod activates rather than staying dormant");

    Check(failures, offsets.kCViewUpdateRva < profile.Fingerprint.SizeOfImage
                 && offsets.kCCameraUpdateFrustumRva < profile.Fingerprint.SizeOfImage,
          "both pinned RVAs land inside the module image");

    // m_viewParams is 12 floats of position + quaternion + more starting at
    // 0x14; the camera has to sit past it, and the mod must never write into it.
    Check(failures, offsets.kCViewCameraOffset > offsets.kCViewParamsOffset,
          "the camera sits after m_viewParams in CView");

    // The camera's Matrix34 is 12 floats. Overlapping m_viewParams would mean
    // the injection silently rewrote the values the game aims with.
    Check(failures, offsets.kCViewParamsOffset + 48u <= offsets.kCViewCameraOffset,
          "the camera matrix cannot overlap m_viewParams");

    return kcd_tests::Report("Build profile tests", failures);
}
