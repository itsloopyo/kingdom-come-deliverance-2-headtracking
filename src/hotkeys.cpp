#include "hotkeys.h"

#include <cameraunlock/input/chord_hotkeys.h>

#include "ads.h"
#include "exe_paths.h"
#include "logging.h"

namespace kcd2_ht
{
    namespace
    {
        using cameraunlock::TrackingMode;
        using cameraunlock::input::ChordGuarded;
        using cameraunlock::input::NavGuarded;

        // Ctrl+Shift+<letter> from the T/Y/U/G/H/J block, in the order AGENTS.md
        // fixes so the same action lands on the same chord in every mod.
        // Ctrl+Shift+T is left free: it was the recenter chord before mods
        // stopped keeping a centre, so reusing it would fire on muscle memory.
        constexpr int kVkY = 0x59;
        constexpr int kVkG = 0x47;
        constexpr int kVkH = 0x48;
        constexpr int kVkU = 0x55;

        constexpr int kPollIntervalMs = 16;

        void ToggleTracking()
        {
            const bool enabled = !Runtime().trackingEnabled.load();
            Runtime().trackingEnabled.store(enabled);
            Log::Line("hotkey: tracking %s", enabled ? "ON" : "OFF");
        }

        // Cycles rather than toggling: 6DOF -> rotation only -> position only.
        // A plain on/off is what the rest of the fleet binds here, but it can
        // only reach two of the three modes the session actually has, and
        // position-only is the one you want when the head rotation is fighting a
        // scripted camera.
        void CycleTrackingMode(Session& session)
        {
            const char* name = "unknown";
            switch (session.CycleMode())
            {
            case TrackingMode::RotationAndPosition: name = "6DOF (rotation + lean)"; break;
            case TrackingMode::RotationOnly:        name = "rotation only"; break;
            case TrackingMode::PositionOnly:        name = "positional lean only"; break;
            }
            Log::Line("hotkey: tracking mode %s", name);
        }

        void ToggleYawMode()
        {
            const bool worldSpace = !Runtime().worldSpaceYaw.load();
            Runtime().worldSpaceYaw.store(worldSpace);
            Log::Line("hotkey: yaw mode %s", worldSpace ? "world" : "local");
        }

        // Two slots, not three: KCD2 has its own aim reticle at the impact
        // point and this mod already moves it, so there is no marker mode to
        // cycle through. The toast strings come from core so they read the
        // same in every mod in the fleet; this one has no on-screen text of
        // its own, so the log is where the mode is named.
        //
        // Nothing caches the tracking verdict here - the view hook recomputes
        // it from this mode on the next frame it draws - so a change made mid
        // aim takes effect on that aim rather than the next one.
        void CycleAdsMode()
        {
            const ads::AdsMode mode = ads::Cycle();
            PersistAdsMode(ExeDirectoryNarrow(), mode);
            Log::Line("hotkey: %s", cameraunlock::ads::AdsModeToast(mode));
        }
    }

    std::unique_ptr<cameraunlock::input::HotkeyPoller> StartHotkeys(Session& session,
                                                                    const Config& config)
    {
        auto poller = std::make_unique<cameraunlock::input::HotkeyPoller>();

        // Nav-cluster defaults. Suppressed while Ctrl+Shift is held so the chord
        // path is the sole trigger for a Ctrl+Shift+<nav> press.
        poller->AddHotkey(config.toggle_key, NavGuarded([] { ToggleTracking(); }));
        poller->AddHotkey(config.position_key, NavGuarded([&session] { CycleTrackingMode(session); }));
        poller->AddHotkey(config.yaw_mode_key, NavGuarded([] { ToggleYawMode(); }));
        poller->AddHotkey(config.ads_mode_key, NavGuarded([] { CycleAdsMode(); }));

        poller->AddHotkey(kVkY, ChordGuarded([] { ToggleTracking(); }));
        poller->AddHotkey(kVkG, ChordGuarded([&session] { CycleTrackingMode(session); }));
        poller->AddHotkey(kVkH, ChordGuarded([] { ToggleYawMode(); }));
        poller->AddHotkey(kVkU, ChordGuarded([] { CycleAdsMode(); }));

        poller->Start(kPollIntervalMs);
        return poller;
    }
}
