#include "cursor_hook.h"

#include <atomic>
#include <intrin.h>
#include <windows.h>

#include <cameraunlock/hooks/hook_manager.h>

#include "builds/build_registry.h"
#include "hook_install.h"
#include "logging.h"

namespace kcd2_ht::cursor
{
    namespace
    {
        namespace hooks = cameraunlock::hooks;

        // (this, element, const float pos[2], int mode)
        using SetCursorPosition_t = void(__fastcall*)(void*, void*, const float*, int);
        using GetDimension_t = int(__fastcall*)(void*);

        SetCursorPosition_t g_orig = nullptr;
        std::uintptr_t g_moduleBase = 0;

        struct AimState
        {
            std::atomic<float> tanRight{0.0f};
            std::atomic<float> tanUp{0.0f};
            std::atomic<float> fovRadians{0.0f};
            std::atomic<float> projectionRatio{0.0f};
            std::atomic<bool> inFront{false};
            std::atomic<std::uint64_t> stampMs{0};
        };

        AimState g_aim;

        // One frame at 30 fps is 33 ms. Loose enough not to blink on a stutter,
        // tight enough that the cursor is back under the game's control by the
        // time a menu has finished opening.
        constexpr std::uint64_t kFreshnessMs = 250;

        // What a back buffer can plausibly be, from the smallest windowed mode
        // the game will open at to well past an 8K display. A pinned vtable slot
        // on a pinned global is exactly the kind of thing a patch moves, and a
        // wrong slot returns something that is not a resolution at all.
        constexpr int kMinBackBufferWidth = 320;
        constexpr int kMinBackBufferHeight = 240;
        constexpr int kMaxBackBufferExtent = 16384;

        bool IsPlausibleResolution(int width, int height)
        {
            return width >= kMinBackBufferWidth && width <= kMaxBackBufferExtent
                && height >= kMinBackBufferHeight && height <= kMaxBackBufferExtent;
        }

        // The same reasoning one field over. The field of view and the projection
        // ratio are read out of the live CCamera through pinned offsets, and
        // ToScreen divides by tan(fov/2) - so a slot a patch has moved can leave a
        // float that is still positive but nowhere near a frustum, and a fov of
        // 1e-20 rad turns a one-degree head turn into a cursor thousands of
        // screens away. The band is far wider than any FOV slider reaches; it
        // exists to catch a wrong offset, not to second-guess a setting.
        constexpr float kMinFovRadians = 0.087f;  // 5 degrees
        constexpr float kMaxFovRadians = 3.05f;   // 175 degrees
        constexpr float kMinProjectionRatio = 0.1f;
        constexpr float kMaxProjectionRatio = 10.0f;

        bool IsPlausibleFrustum(float fovRadians, float projectionRatio)
        {
            return fovRadians >= kMinFovRadians && fovRadians <= kMaxFovRadians
                && projectionRatio >= kMinProjectionRatio
                && projectionRatio <= kMaxProjectionRatio;
        }

        bool CallSiteIsCursor(std::uintptr_t returnRva)
        {
            const auto& offsets = builds::Offsets();
            return returnRva == offsets.kCursorCentreReturnRva
                || returnRva == offsets.kCombatCursorReturnRva
                || returnRva == offsets.kAimedCursorReturnRva;
        }

        bool ScreenSize(float& width, float& height)
        {
            const auto& offsets = builds::Offsets();
            auto* renderer = *reinterpret_cast<void**>(g_moduleBase + offsets.kRendererGlobalRva);
            if (renderer == nullptr) return false;

            auto** vtable = *reinterpret_cast<void***>(renderer);
            const int w = reinterpret_cast<GetDimension_t>(
                vtable[offsets.kRendererWidthSlot / sizeof(void*)])(renderer);
            const int h = reinterpret_cast<GetDimension_t>(
                vtable[offsets.kRendererHeightSlot / sizeof(void*)])(renderer);

            // An implausible size means the profile is wrong, so say so once and
            // stop touching the cursor rather than flinging it off screen.
            if (!IsPlausibleResolution(w, h))
            {
                static std::atomic<bool> complained{false};
                if (!complained.exchange(true))
                    Log::Line("The renderer reported a %dx%d back buffer, which is not a "
                              "resolution - leaving the game's crosshair alone.", w, h);
                return false;
            }

            static std::atomic<bool> logged{false};
            if (!logged.exchange(true))
                Log::Line("HUD cursor: back buffer is %dx%d, so the crosshair now follows your "
                          "aim instead of sitting at screen centre.", w, h);

            width = static_cast<float>(w);
            height = static_cast<float>(h);
            return true;
        }

        bool AimOffset(float& dx, float& dy)
        {
            if (!g_aim.inFront.load(std::memory_order_relaxed)) return false;
            const std::uint64_t stamp = g_aim.stampMs.load(std::memory_order_relaxed);
            if (stamp == 0 || (GetTickCount64() - stamp) >= kFreshnessMs) return false;

            const float fov = g_aim.fovRadians.load(std::memory_order_relaxed);
            const float ratio = g_aim.projectionRatio.load(std::memory_order_relaxed);
            if (!IsPlausibleFrustum(fov, ratio))
            {
                // Both read zero until the first active view update, which is the
                // normal startup state rather than a fault, so only a value that
                // is present and wrong is worth a line.
                static std::atomic<bool> complained{false};
                if (fov != 0.0f && !complained.exchange(true))
                    Log::Line("The camera reported a %.4f rad field of view at ratio %.3f, "
                              "which is not a frustum - leaving the game's crosshair alone.",
                              static_cast<double>(fov), static_cast<double>(ratio));
                return false;
            }

            float width = 0.0f, height = 0.0f;
            if (!ScreenSize(width, height)) return false;

            AimProjection aim;
            aim.tanRight = g_aim.tanRight.load(std::memory_order_relaxed);
            aim.tanUp = g_aim.tanUp.load(std::memory_order_relaxed);
            aim.inFront = true;

            const ScreenPoint p = ToScreen(aim, width, height, fov, ratio);
            dx = p.x - width * 0.5f;
            dy = p.y - height * 0.5f;

            return true;
        }

        void __fastcall SetCursorPosition_Detour(void* self, void* element, const float* pos, int mode)
        {
            const std::uintptr_t returnRva =
                reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - g_moduleBase;

            float dx = 0.0f, dy = 0.0f;
            if (pos != nullptr && CallSiteIsCursor(returnRva)
                    && AimOffset(dx, dy))
            {
                // For the site that hands in screen centre this is exact. For the
                // combat site, which hands in an already-aimed position, it is a
                // first-order shift of that point into the tracked view - right at
                // the centre and increasingly approximate towards the edges, which
                // is where that cursor never is.
                const float moved[2] = { pos[0] + dx, pos[1] + dy };
                g_orig(self, element, moved, mode);
                return;
            }

            g_orig(self, element, pos, mode);
        }
    }

    bool Install(std::uintptr_t moduleBase)
    {
        g_moduleBase = moduleBase;

        const auto& offsets = builds::Offsets();
        void* target = reinterpret_cast<void*>(moduleBase + offsets.kSetCursorPositionRva);

        const hooks::HookStatus status = CreateAndEnableHook(
            target, reinterpret_cast<void*>(&SetCursorPosition_Detour),
            reinterpret_cast<void**>(&g_orig));
        if (status != hooks::HookStatus::Ok)
        {
            Log::Line("Could not hook the HUD cursor setter at RVA 0x%08X: %s - the game's "
                      "crosshair will stay at screen centre.",
                      offsets.kSetCursorPositionRva, hooks::HookStatusToString(status));
            return false;
        }

        float width = 0.0f, height = 0.0f;
        if (ScreenSize(width, height))
            Log::Line("HUD cursor hooked at RVA 0x%08X (renderer reports %.0fx%.0f).",
                      offsets.kSetCursorPositionRva,
                      static_cast<double>(width), static_cast<double>(height));
        else
            Log::Line("HUD cursor hooked at RVA 0x%08X (renderer not up yet; size read on first "
                      "use).", offsets.kSetCursorPositionRva);
        return true;
    }

    void SubmitAim(const AimProjection& aim, float fovRadians, float projectionRatio)
    {
        g_aim.tanRight.store(aim.tanRight, std::memory_order_relaxed);
        g_aim.tanUp.store(aim.tanUp, std::memory_order_relaxed);
        g_aim.inFront.store(aim.inFront, std::memory_order_relaxed);
        g_aim.fovRadians.store(fovRadians, std::memory_order_relaxed);
        g_aim.projectionRatio.store(projectionRatio, std::memory_order_relaxed);
        g_aim.stampMs.store(GetTickCount64(), std::memory_order_relaxed);
    }
}
