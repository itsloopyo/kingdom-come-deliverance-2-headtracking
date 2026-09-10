#include "view_hook.h"

#include <atomic>
#include <windows.h>

#include <cameraunlock/hooks/hook_manager.h>
#include <cameraunlock/time/frame_clock.h>

#include "ads.h"
#include "aim_projection.h"
#include "builds/build_registry.h"
#include "cryengine_types.h"
#include "cursor_hook.h"
#include "hook_install.h"
#include "logging.h"
#include "pose_guard.h"
#include "view_injection.h"
#include "game_state.h"

namespace kcd2_ht::view_hook
{
    namespace
    {
        namespace hooks = cameraunlock::hooks;
        using cameraunlock::time::FrameClock;

        using CViewUpdate_t = void(__fastcall*)(void* self, float frameTime, bool isActive);
        using UpdateFrustum_t = void(__fastcall*)(void* camera);

        CViewUpdate_t g_origCViewUpdate = nullptr;
        UpdateFrustum_t g_updateFrustum = nullptr;
        std::uintptr_t g_moduleBase = 0;

        Session* g_session = nullptr;
        UpdateObserver g_onUpdate = nullptr;

        std::atomic<std::uint64_t> g_updateCount{0};

        // The two frustum fields the reticle projection needs, captured off the
        // live camera every frame. Kept here rather than read at the point of
        // use so the heartbeat can report them even on frames where nothing was
        // injected: an aim offset computed from the wrong field of view is the
        // one reticle fault a log otherwise cannot distinguish from a bad hook.
        std::atomic<float> g_fovRadians{0.0f};
        std::atomic<float> g_projectionRatio{0.0f};

        // Ticked only by the active view's update, so the session sees exactly
        // one dt per rendered frame.
        FrameClock g_frameClock;

        // Whether the sights were up on the last frame that reached the ADS
        // stage. Reported even in the paused mode, where the pose has been faded
        // to nothing: the mode says what tracking does, this says what the game
        // is doing, and the heartbeat needs both to be readable.
        std::atomic<bool> g_aiming{false};

        // Every path that declines to apply a pose is a real suppression, so the
        // ADS fade and the entry pose are dropped with it. Returning early
        // without this leaves the next aim resuming against a pose captured
        // before the suppression.
        bool Suppressed()
        {
            ads::Suppress();
            cursor::HideAimMarker();
            g_aiming.store(false, std::memory_order_relaxed);
            return false;
        }

        // The whole injection. Runs after the engine has composed m_viewParams
        // into the camera matrix, and writes ONLY that matrix.
        // Returns false when nothing was injected this frame - tracking toggled
        // off, or no tracker data. The cursor hook reads that from the aim going
        // stale rather than from a flag, so the crosshair returns to wherever the
        // game wanted it with nothing to put back.
        bool InjectHeadPose(void* self)
        {
            const bool paused = game_state::IsPaused(g_moduleBase);
            static bool wasPaused = false;
            if (paused != wasPaused)
            {
                Log::Line("Game pause: %s. Head tracking %s.",
                          paused ? "active" : "inactive",
                          paused ? "suppressed" : "allowed");
                wasPaused = paused;
            }
            if (!Runtime().trackingEnabled.load()) return Suppressed();

            const float dt = g_frameClock.Tick();
            if (!g_session->Update(dt)) return Suppressed();
            if (paused) return Suppressed();

            HeadPose pose;
            if (!g_session->GetRotation(pose.yaw, pose.pitch, pose.roll)) return Suppressed();
            const bool positionActive = g_session->GetPositionOffset(pose.x, pose.y, pose.z);

            // Said once, not per frame: the processor's smoothed state keeps the
            // NaN, so this fires on every frame for the rest of the session.
            if (!IsFinitePose(pose))
            {
                static std::atomic<bool> complained{false};
                if (!complained.exchange(true))
                    Log::Line("The tracking pipeline produced a pose that is not a number "
                              "(Y=%g P=%g R=%g pos=%g %g %g) - the camera is left alone from "
                              "here. A tracker sending an absurd value poisons the filter for "
                              "the rest of the session; fix the tracker and restart the game.",
                              static_cast<double>(pose.yaw), static_cast<double>(pose.pitch),
                              static_cast<double>(pose.roll), static_cast<double>(pose.x),
                              static_cast<double>(pose.y), static_cast<double>(pose.z));
                return Suppressed();
            }

            // What the sights are doing to the pose, decided before it is
            // composed onto the camera so everything downstream - the write,
            // the frustum rebuild and the reticle projection - agrees on one
            // pose. In the paused mode this is what fades the head off the
            // camera and holds it off for the length of the aim.
            g_aiming.store(ads::Apply(pose, GetTickCount64(), game_state::IsAiming(g_moduleBase)),
                           std::memory_order_relaxed);

            const auto& offsets = builds::Offsets();
            auto* cameraBytes = reinterpret_cast<std::uint8_t*>(self) + offsets.kCViewCameraOffset;
            auto* camera = reinterpret_cast<Matrix34f*>(cameraBytes);

            // Kept for the reticle: this is the direction the game will fire,
            // raycast and swing along, and the whole job of the reticle is to
            // show where it lands in the picture the player is actually shown.
            const Matrix34f clean = *camera;

            *camera = ApplyHeadPose(clean, pose, Runtime().worldSpaceYaw.load(), positionActive);

            // Culling, shadow cascades and the render camera all derive from the
            // frustum planes, which the engine built from the pre-injection
            // matrix. Rebuild them or the world is culled against a view the
            // player is no longer looking through.
            g_updateFrustum(camera);

            cursor::SubmitAim(ProjectAim(clean, *camera),
                              g_fovRadians.load(std::memory_order_relaxed),
                              g_projectionRatio.load(std::memory_order_relaxed),
                              g_aiming.load(std::memory_order_relaxed));
            return true;
        }

        // CCamera::SetFrustum has just written both fields from the FOV the view
        // asked for, so this is what the frame is rendered with. Injecting the
        // head pose does not disturb them: UpdateFrustum rebuilds the planes from
        // the matrix and reads these rather than writing them.
        void CaptureFrustum(const void* self)
        {
            const auto& offsets = builds::Offsets();
            const auto* cameraBytes =
                reinterpret_cast<const std::uint8_t*>(self) + offsets.kCViewCameraOffset;
            g_fovRadians.store(
                *reinterpret_cast<const float*>(cameraBytes + offsets.kCCameraFovOffset),
                std::memory_order_relaxed);
            g_projectionRatio.store(
                *reinterpret_cast<const float*>(cameraBytes + offsets.kCCameraProjectionRatioOffset),
                std::memory_order_relaxed);
        }

        void __fastcall CViewUpdate_Detour(void* self, float frameTime, bool isActive)
        {
            g_origCViewUpdate(self, frameTime, isActive);

            if (!isActive || self == nullptr) return;

            g_updateCount.fetch_add(1, std::memory_order_relaxed);
            CaptureFrustum(self);
            g_onUpdate();

            InjectHeadPose(self);
        }
    }

    bool Install(std::uintptr_t moduleBase, Session& session, UpdateObserver onUpdate)
    {
        g_moduleBase = moduleBase;
        g_session = &session;
        g_onUpdate = onUpdate;

        const auto& offsets = builds::Offsets();
        void* target = reinterpret_cast<void*>(moduleBase + offsets.kCViewUpdateRva);
        g_updateFrustum = reinterpret_cast<UpdateFrustum_t>(
            moduleBase + offsets.kCCameraUpdateFrustumRva);

        const hooks::HookStatus status = CreateAndEnableHook(
            target, reinterpret_cast<void*>(&CViewUpdate_Detour),
            reinterpret_cast<void**>(&g_origCViewUpdate));
        if (status != hooks::HookStatus::Ok)
        {
            Log::Line("Could not hook CView::Update at RVA 0x%08X: %s - staying dormant.",
                      offsets.kCViewUpdateRva, hooks::HookStatusToString(status));
            return false;
        }

        Log::Line("CView::Update hooked at RVA 0x%08X (camera at CView+0x%X, viewParams at "
                  "CView+0x%X left untouched).",
                  offsets.kCViewUpdateRva, offsets.kCViewCameraOffset,
                  offsets.kCViewParamsOffset);
        return true;
    }

    std::uint64_t UpdateCount() { return g_updateCount.load(std::memory_order_relaxed); }
    float FovRadians() { return g_fovRadians.load(std::memory_order_relaxed); }
    float ProjectionRatio() { return g_projectionRatio.load(std::memory_order_relaxed); }
}
