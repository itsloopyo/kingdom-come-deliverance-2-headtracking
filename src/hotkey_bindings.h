#pragma once

#include <vector>

#include <cameraunlock/input/key_bindings.h>

#include "config.h"

namespace kcd2_ht
{
    // The keys each hotkey action fires on, as the poller registers them.
    struct HotkeyBindings
    {
        std::vector<cameraunlock::input::KeyBinding> toggle;
        std::vector<cameraunlock::input::KeyBinding> cycle_tracking_mode;
        std::vector<cameraunlock::input::KeyBinding> yaw_mode;
    };

    HotkeyBindings BindingsFor(const Config& config);
}
