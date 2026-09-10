#include "game_state.h"

#include "builds/build_registry.h"

namespace kcd2_ht::game_state
{
    namespace
    {
        std::uintptr_t GetFramework(std::uintptr_t moduleBase)
        {
            const auto& offsets = builds::Offsets();
            const auto game = *reinterpret_cast<std::uintptr_t*>(
                moduleBase + offsets.kGameInterfaceGlobalRva);
            if (!game) return 0;

            return *reinterpret_cast<std::uintptr_t*>(game + offsets.kGameFrameworkOffset);
        }
    }

    bool IsPaused(std::uintptr_t moduleBase)
    {
        const auto framework = GetFramework(moduleBase);
        if (!framework) return true;

        using Query = bool(__fastcall*)(std::uintptr_t);
        const auto table = *reinterpret_cast<std::uintptr_t*>(framework);
        const auto isPaused = *reinterpret_cast<Query*>(
            table + builds::Offsets().kFrameworkIsPausedSlot);
        return isPaused(framework);
    }

    bool IsAiming(std::uintptr_t moduleBase)
    {
        const auto& offsets = builds::Offsets();
        const auto framework = GetFramework(moduleBase);
        if (!framework) return false;

        using GetClientActor = std::uintptr_t(__fastcall*)(std::uintptr_t);
        const auto frameworkTable = *reinterpret_cast<std::uintptr_t*>(framework);
        const auto getClientActor = *reinterpret_cast<GetClientActor*>(
            frameworkTable + offsets.kFrameworkClientActorSlot);
        const auto player = getClientActor(framework);
        if (!player) return false;

        const auto actor = *reinterpret_cast<std::uintptr_t*>(
            player + offsets.kPlayerActionActorOffset);
        if (!actor) return false;

        using GetExpansion = std::uintptr_t(__fastcall*)(std::uintptr_t, std::uint8_t);
        const auto actorTable = *reinterpret_cast<std::uintptr_t*>(actor);
        const auto getExpansion = *reinterpret_cast<GetExpansion*>(
            actorTable + offsets.kActionActorExpansionSlot);
        const auto shooting = getExpansion(actor, 4);
        if (!shooting) return false;

        using Query = bool(__fastcall*)(std::uintptr_t);
        const auto isAiming = reinterpret_cast<Query>(moduleBase + offsets.kShootingIsAimingRva);
        const auto isCharging = reinterpret_cast<Query>(moduleBase + offsets.kShootingIsChargingRva);
        return isAiming(shooting) || isCharging(shooting);
    }
}
