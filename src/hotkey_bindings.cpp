#include "hotkey_bindings.h"

namespace kcd2_ht
{
    namespace
    {
        using cameraunlock::input::KeyBinding;
        using cameraunlock::input::KeyModifiers;

        constexpr KeyModifiers kChord = KeyModifiers::kCtrl | KeyModifiers::kShift;

        // Ctrl+Shift+<letter> from the T/Y/U/G/H/J block, in the order AGENTS.md
        // fixes so the same action lands on the same chord in every mod.
        // Ctrl+Shift+T is left free: it was the recenter chord before mods
        // stopped keeping a centre, so reusing it would fire on muscle memory.
        constexpr int kVkY = 0x59;
        constexpr int kVkG = 0x47;
        constexpr int kVkH = 0x48;
    }

    HotkeyBindings BindingsFor(const Config& config)
    {
        HotkeyBindings bindings;
        bindings.toggle = {KeyBinding{KeyModifiers::kNone, config.toggle_key},
                           KeyBinding{kChord, kVkY}};
        bindings.cycle_tracking_mode = {KeyBinding{KeyModifiers::kNone, config.position_key},
                                        KeyBinding{kChord, kVkG}};
        bindings.yaw_mode = {KeyBinding{KeyModifiers::kNone, config.yaw_mode_key},
                             KeyBinding{kChord, kVkH}};
        return bindings;
    }
}
