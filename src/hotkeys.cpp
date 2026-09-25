#include "hotkeys.h"

#include <functional>
#include <vector>

#include <cameraunlock/input/chord_hotkeys.h>

#include "hotkey_bindings.h"
#include "logging.h"

namespace kcd2_ht
{
    namespace
    {
        using cameraunlock::TrackingMode;
        using cameraunlock::input::ChordGuarded;
        using cameraunlock::input::HotkeyPoller;
        using cameraunlock::input::KeyBinding;
        using cameraunlock::input::KeyModifiers;
        using cameraunlock::input::NavGuarded;

        constexpr int kPollIntervalMs = 16;

        // A nav-cluster binding is suppressed while Ctrl+Shift is held so the
        // chord path is the sole trigger for a Ctrl+Shift+<nav> press.
        void Register(HotkeyPoller& poller, const std::vector<KeyBinding>& bindings,
                      const std::function<void()>& action)
        {
            for (const KeyBinding& binding : bindings)
                poller.AddHotkey(binding.vk, binding.modifiers == KeyModifiers::kNone
                                                 ? NavGuarded(action)
                                                 : ChordGuarded(action));
        }

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
    }

    std::unique_ptr<cameraunlock::input::HotkeyPoller> StartHotkeys(Session& session,
                                                                    const Config& config)
    {
        auto poller = std::make_unique<HotkeyPoller>();
        const HotkeyBindings bindings = BindingsFor(config);
        Register(*poller, bindings.toggle, [] { ToggleTracking(); });
        Register(*poller, bindings.cycle_tracking_mode, [&session] { CycleTrackingMode(session); });
        Register(*poller, bindings.yaw_mode, [] { ToggleYawMode(); });

        poller->Start(kPollIntervalMs);
        return poller;
    }
}
