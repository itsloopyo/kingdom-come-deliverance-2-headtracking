#include "build_registry.h"

// Every Microsoft Store / Xbox Game Pass build of Kingdom Come: Deliverance II
// this mod knows about, oldest at the bottom. APPEND ONLY, exactly as in
// steam_offsets.cpp: a new game build gets a NEW profile, never an edit to an
// existing one.
//
// The GDK build is a separate link of the same engine as the Steam build. Every
// struct offset and vtable slot below came out identical to the Steam profile's,
// and every RVA moved, which is why this is a second profile rather than a
// shared table: the two binaries agree on what the engine IS and disagree on
// where all of it lives.
//
// The Game Pass layout is flat. KingdomCome.exe and WHGame.dll sit directly in
// <XboxGames>\Kingdom Come- Deliverance II\Content, with no
// Bin\Win64MasterMasterSteamPGO, so the loader, the ASI, CameraUnlock.ini and
// HeadTracking.log all sit there instead. Nothing in the mod depends on that -
// the log and INI follow the running exe - but it is the first thing to check
// when a Game Pass player reports no log file.

namespace kcd2_ht::builds
{
    // Microsoft Store package DeepSilver.77536C3FE941 1.5.6.0
    // (MicrosoftGame.Config). WHGame.dll built 2026-06-22 09:11:23 UTC.
    extern const BuildProfile kGdkProfile_20260622 = {
        /* Name        */ "gdk-win64-20260622",
        /* Fingerprint */ { 0x6A391F7Bu, 0x05BF2000u, 0x00000000u },
        /* Offsets     */ {
            /* kCViewUpdateRva               */ 0x008A07A0u,
            /* kCCameraUpdateFrustumRva      */ 0x003D4234u,
            /* kCViewCameraOffset            */ 0x000000E8u,
            /* kCViewParamsOffset            */ 0x00000014u,
            /* kCCameraFovOffset             */ 0x00000030u,
            /* kCCameraProjectionRatioOffset */ 0x00000040u,
            /* kSetCursorPositionRva         */ 0x00907154u,
            /* kCursorCentreReturnRva        */ 0x008A3D8Cu,
            /* kCombatCursorReturnRva        */ 0x00CA0F05u,
            /* kAimedCursorReturnRva         */ 0x00CA1188u,
            /* kRendererGlobalRva            */ 0x049D7008u,
            /* kRendererWidthSlot            */ 0x00000238u,
            /* kRendererHeightSlot           */ 0x00000230u,
            /* kGameInterfaceGlobalRva       */ 0x0555BBB8u,
            /* kGameFrameworkOffset          */ 0x00000008u,
            /* kFrameworkClientActorSlot     */ 0x00000200u,
            /* kFrameworkIsPausedSlot        */ 0x00000078u,
            /* kPlayerActionActorOffset      */ 0x00000280u,
            /* kActionActorExpansionSlot     */ 0x00000070u,
            /* kShootingIsAimingRva           */ 0x014BA1F4u,
            /* kShootingIsChargingRva         */ 0x013361CCu,
        },
    };
}
