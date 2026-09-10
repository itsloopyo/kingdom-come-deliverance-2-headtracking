// Consistency checks on the shipped build profiles. The RVAs themselves can only
// be confirmed against the game, but the relationships between them are
// checkable here, and each one has a failure mode that reaches a player: a zero
// hook RVA leaves the mod permanently dormant, a camera offset that collides
// with m_viewParams would write over the values the game aims with, and two
// profiles sharing a fingerprint would route half the players to the wrong RVAs.

#include "builds/build_registry.h"

#include "test_support.h"

namespace
{
    using kcd_tests::Check;
    using kcd2_ht::builds::BuildProfile;

    // A CryEngine Matrix34 is twelve floats. m_viewParams starting inside that
    // span would mean the injection silently rewrote the values the game aims
    // with.
    constexpr std::uint32_t kMatrix34Bytes = 48;

    // Every store's profile for the build the mod was written against. The
    // registry array itself lives in build_registry.cpp, which only compiles
    // against a live game; these are the same objects it points at.
    const BuildProfile* const kProfiles[] = {
        &kcd2_ht::builds::kGdkProfile_20260622,
        &kcd2_ht::builds::kSteamProfile_20260619,
    };

    void CheckProfile(int& failures, const BuildProfile& profile)
    {
        const kcd2_ht::builds::OffsetTable& offsets = profile.Offsets;
        const std::string name(profile.Name);

        Check(failures, IsComplete(profile),
              name + ": the profile is complete, so the mod activates rather than "
                     "staying dormant");

        Check(failures, offsets.kCViewUpdateRva < profile.Fingerprint.SizeOfImage
                     && offsets.kCCameraUpdateFrustumRva < profile.Fingerprint.SizeOfImage
                     && offsets.kSetCursorPositionRva < profile.Fingerprint.SizeOfImage
                     && offsets.kRendererGlobalRva < profile.Fingerprint.SizeOfImage,
              name + ": every pinned RVA lands inside the module image");

        // m_viewParams is 12 floats of position + quaternion + more starting at
        // 0x14; the camera has to sit past it, and the mod must never write into it.
        Check(failures, offsets.kCViewCameraOffset > offsets.kCViewParamsOffset,
              name + ": the camera sits after m_viewParams in CView");

        Check(failures, offsets.kCViewParamsOffset + kMatrix34Bytes <= offsets.kCViewCameraOffset,
              name + ": the camera matrix cannot overlap m_viewParams");

        // A return address is the instruction after a five-byte call, so it can
        // never equal the callee's own entry point, and a duplicate would apply
        // the aim offset twice at one site.
        const std::uint32_t sites[] = {
            offsets.kCursorCentreReturnRva,
            offsets.kCombatCursorReturnRva,
            offsets.kAimedCursorReturnRva,
        };
        constexpr int kSiteCount = static_cast<int>(sizeof(sites) / sizeof(sites[0]));
        for (int i = 0; i < kSiteCount; ++i)
        {
            Check(failures, sites[i] != offsets.kSetCursorPositionRva
                         && sites[i] < profile.Fingerprint.SizeOfImage,
                  name + ": cursor return site " + std::to_string(i) + " is a distinct "
                         "in-image address");
            for (int j = 0; j < i; ++j)
            {
                Check(failures, sites[j] != sites[i],
                      name + ": cursor return site " + std::to_string(i) + " is not a "
                             "duplicate of " + std::to_string(j));
            }
        }
    }
}

int RunBuildProfileTests()
{
    int failures = 0;
    std::cout << "Build profile tests\n";

    // Read from the shipped WHGame.dll - not the exe, which is a launcher stub
    // with no camera code in it. Steam keeps both under
    // Bin\Win64MasterMasterSteamPGO; the Game Pass package puts them flat in
    // Content.
    Check(failures,
          kcd2_ht::builds::kSteamProfile_20260619.Fingerprint.TimeDateStamp == 0x6A350E20u
       && kcd2_ht::builds::kSteamProfile_20260619.Fingerprint.SizeOfImage == 0x05B2D000u
       && kcd2_ht::builds::kSteamProfile_20260619.Fingerprint.CheckSum == 0x00000000u,
          "the Steam 2026-06-19 WHGame.dll fingerprint is unchanged");

    Check(failures,
          kcd2_ht::builds::kGdkProfile_20260622.Fingerprint.TimeDateStamp == 0x6A391F7Bu
       && kcd2_ht::builds::kGdkProfile_20260622.Fingerprint.SizeOfImage == 0x05BF2000u
       && kcd2_ht::builds::kGdkProfile_20260622.Fingerprint.CheckSum == 0x00000000u,
          "the Game Pass 2026-06-22 WHGame.dll fingerprint is unchanged");

    constexpr int kProfileCount = static_cast<int>(sizeof(kProfiles) / sizeof(kProfiles[0]));
    for (int i = 0; i < kProfileCount; ++i)
    {
        CheckProfile(failures, *kProfiles[i]);
        auto missingPause = *kProfiles[i];
        missingPause.Offsets.kFrameworkIsPausedSlot = 0;
        Check(failures, !kcd2_ht::builds::IsComplete(missingPause),
              "a profile without game pause detection cannot activate");
        auto missingAim = *kProfiles[i];
        missingAim.Offsets.kShootingIsAimingRva = 0;
        Check(failures, !kcd2_ht::builds::IsComplete(missingAim),
              "a profile without weapon aim detection cannot activate");
        auto missingCharge = *kProfiles[i];
        missingCharge.Offsets.kShootingIsChargingRva = 0;
        Check(failures, !kcd2_ht::builds::IsComplete(missingCharge),
              "a profile without weapon charge detection cannot activate");
        for (int j = 0; j < i; ++j)
        {
            Check(failures, !kProfiles[i]->Fingerprint.Matches(kProfiles[j]->Fingerprint),
                  std::string(kProfiles[i]->Name) + " and " + kProfiles[j]->Name
                      + " are told apart by fingerprint");
        }
    }

    // The two stores ship separate links of the same engine, so the struct
    // layout has to agree even though every RVA differs. A future profile that
    // disagrees here is a layout change, not a relink, and the injection maths
    // would need revisiting rather than just the addresses.
    const kcd2_ht::builds::OffsetTable& steam = kcd2_ht::builds::kSteamProfile_20260619.Offsets;
    const kcd2_ht::builds::OffsetTable& gdk = kcd2_ht::builds::kGdkProfile_20260622.Offsets;
    Check(failures, steam.kCViewCameraOffset == gdk.kCViewCameraOffset
                 && steam.kCViewParamsOffset == gdk.kCViewParamsOffset
                 && steam.kCCameraFovOffset == gdk.kCCameraFovOffset
                 && steam.kCCameraProjectionRatioOffset == gdk.kCCameraProjectionRatioOffset,
          "the Steam and Game Pass builds agree on every struct offset");

    Check(failures, steam.kRendererWidthSlot == gdk.kRendererWidthSlot
                 && steam.kRendererHeightSlot == gdk.kRendererHeightSlot,
          "the Steam and Game Pass builds agree on every vtable slot");

    Check(failures, steam.kCViewUpdateRva != gdk.kCViewUpdateRva
                 && steam.kSetCursorPositionRva != gdk.kSetCursorPositionRva
                 && steam.kRendererGlobalRva != gdk.kRendererGlobalRva,
          "the two builds are separate links, so their RVAs differ");

    return kcd_tests::Report("Build profile tests", failures);
}
