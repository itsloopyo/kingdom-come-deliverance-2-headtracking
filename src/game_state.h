#pragma once

#include <cstdint>

namespace kcd2_ht::game_state
{
    bool IsPaused(std::uintptr_t moduleBase);
    bool IsAiming(std::uintptr_t moduleBase);
}
