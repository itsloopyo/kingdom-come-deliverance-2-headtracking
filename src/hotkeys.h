#pragma once

#include <memory>

#include <cameraunlock/input/hotkey_poller.h>

#include "config.h"
#include "runtime_state.h"

namespace kcd2_ht
{
    // Binds the nav-cluster keys and their Ctrl+Shift chord alternatives, then
    // starts polling. The returned poller owns the polling thread; Stop() it
    // before the session goes away.
    std::unique_ptr<cameraunlock::input::HotkeyPoller> StartHotkeys(Session& session,
                                                                    const Config& config);
}
