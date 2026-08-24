#include "build_registry.h"

// Every Steam build of Kingdom Come: Deliverance II this mod knows about, oldest
// at the bottom. APPEND ONLY - see AGENTS.md "Maintain compatibility across new
// patches". Editing an existing profile's RVAs strands every player who has not
// taken the patch yet.
//
// The fingerprint is WHGame.dll's, not KingdomCome.exe's. Warhorse ships the
// engine and the game in that one 89 MB module, and both binaries are relinked
// together, so either would route correctly - but the RVAs are WHGame.dll's, and
// fingerprinting the module the offsets belong to is the only version of this
// that cannot drift.
//
// CheckSum is 0 in the shipped headers (the linker was not asked to compute one).
// That is not a problem: the triple is still matched exactly, so a repacked
// binary that DOES carry a checksum fails the match rather than mis-routing.
//
// WHGameArm.dll ships beside it and is NOT a target: it is the ARM64 build of
// the same code, so its RVAs are unrelated. The mod only ever fingerprints
// WHGame.dll.

namespace kcd2_ht::builds
{
    // Steam buildid 23914554. WHGame.dll built 2026-06-19 09:38:40 UTC.
    extern const BuildProfile kSteamProfile_20260619 = {
        /* Name        */ "steam-win64-20260619",
        /* Fingerprint */ { 0x6A350E20u, 0x05B2D000u, 0x00000000u },
        /* Offsets     */ {
            /* kCViewUpdateRva               */ 0x007F0EC0u,
            /* kCCameraUpdateFrustumRva      */ 0x00537BA4u,
            /* kCViewCameraOffset            */ 0x000000E8u,
            /* kCViewParamsOffset            */ 0x00000014u,
            /* kCCameraFovOffset             */ 0x00000030u,
            /* kCCameraProjectionRatioOffset */ 0x00000040u,
            /* kSetCursorPositionRva         */ 0x009A85D4u,
            /* kCursorCentreReturnRva        */ 0x007F44F0u,
            /* kCombatCursorReturnRva        */ 0x0089B715u,
            /* kAimedCursorReturnRva         */ 0x0089B998u,
            /* kRendererGlobalRva            */ 0x0492D908u,
            /* kRendererWidthSlot            */ 0x00000238u,
            /* kRendererHeightSlot           */ 0x00000230u,
        },
    };
}
