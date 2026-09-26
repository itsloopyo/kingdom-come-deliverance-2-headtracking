#include "headtracking_mod.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <cameraunlock/diagnostics/crash_handler.h>
#include <cameraunlock/hooks/hook_manager.h>
#include <cameraunlock/math/angle_utils.h>

#include "builds/build_registry.h"
#include "config.h"
#include "cursor_hook.h"
#include "exe_paths.h"
#include "hotkeys.h"
#include "logging.h"
#include "runtime_state.h"
#include "view_hook.h"

namespace kcd2_ht
{
    namespace
    {
        namespace hooks = cameraunlock::hooks;

        // The engine and the whole game live in this one module; KingdomCome.exe
        // is a launcher stub with no camera code in it at all.
        constexpr wchar_t kGameModule[] = L"WHGame.dll";

        // The ASI loads while WHGame.dll's imports are being resolved, so the
        // module is mapped but its entry point has not run. Poll rather than
        // assume: a hook installed before the loader finished with the module
        // would be written over.
        constexpr int kModuleWaitMs = 20000;
        constexpr int kModulePollMs = 50;

        // Only the keepalive cadence. A state change is reported the moment it
        // happens, so this interval is how stale a quiet log is allowed to look,
        // not how long a player waits to see that a toggle took effect.
        constexpr std::uint64_t kHeartbeatIntervalMs = 300000;

        HANDLE g_bootstrapThread = nullptr;

        // A reference on our own image, taken in Initialize and given back by the
        // bootstrap thread on its way out. Without it an explicit FreeLibrary
        // unmaps this DLL while that thread is still parked in WaitForGameModule
        // or halfway through installing a trampoline, and the thread returns into
        // an address that is no longer mapped.
        HMODULE g_selfReference = nullptr;

        Config g_config;
        // Built before anything reads CameraUnlock.ini and never destroyed while
        // the hotkey thread that saves through it runs.
        std::unique_ptr<cameraunlock::config::ConfigOwner<Config>> g_owner;

        std::unique_ptr<cameraunlock::UdpReceiver> g_receiver;
        std::unique_ptr<Session> g_session;
        std::unique_ptr<cameraunlock::input::HotkeyPoller> g_hotkeys;

        // Everything on the heartbeat line that a reader triages from, sampled
        // once so the printed line and the change test cannot disagree. The pose
        // numbers stay out of it: they move every frame while a player is
        // wearing the tracker, so folding them into the change test would put a
        // line in the log per frame.
        struct HeartbeatState
        {
            bool enabled;
            bool haveData;
            bool positionActive;
            bool worldYaw;
            bool remote;
            bool portRetrying;
            bool portBound;

            std::uint32_t Bits() const
            {
                return (enabled        ? 1u << 0 : 0u)
                     | (haveData       ? 1u << 1 : 0u)
                     | (positionActive ? 1u << 2 : 0u)
                     | (worldYaw       ? 1u << 3 : 0u)
                     | (remote         ? 1u << 4 : 0u)
                     | (portRetrying   ? 1u << 5 : 0u)
                     | (portBound      ? 1u << 6 : 0u);
            }

            // "no data" has two causes that look identical from the game: the
            // tracker is not sending, or the port is held by something else and
            // the receiver is still waiting for it. Only the second is fixable by
            // closing the other app, so name it rather than making the player
            // guess. The receiver reclaims the port on its own once it frees up;
            // the heartbeat is how they see that happen.
            const char* PortState() const
            {
                if (portRetrying) return "WAITING (held by another app)";
                return portBound ? "bound" : "down";
            }
        };

        // Called from the view hook once per active view update, so the receiver
        // and the session are both live by construction - the bootstrap builds
        // them before it installs the hook. Writes the first line immediately -
        // that is the line proving the hook fired at all - then on every change
        // to the state above, and otherwise once every kHeartbeatIntervalMs to
        // show the mod is still alive.
        void LogHeartbeat()
        {
            static std::atomic<std::uint64_t> lastTick{0};
            static std::atomic<std::uint32_t> lastBits{0};

            float yaw = 0, pitch = 0, roll = 0;
            const bool haveData = g_receiver->GetRotation(yaw, pitch, roll);
            float px = 0, py = 0, pz = 0;
            const bool havePos = g_receiver->GetPosition(px, py, pz);

            const HeartbeatState state{
                Runtime().trackingEnabled.load(),
                haveData,
                g_session->IsPositionActive(),
                Runtime().worldSpaceYaw.load(),
                g_session->IsRemoteConnection(),
                g_receiver->IsRetrying(),
                g_receiver->IsRunning(),
            };

            const std::uint64_t now = GetTickCount64();
            const std::uint64_t updates = view_hook::UpdateCount();
            const std::uint32_t bits = state.Bits();
            const bool changed = lastBits.exchange(bits, std::memory_order_relaxed) != bits;
            if (updates != 1 && !changed
                && (now - lastTick.load(std::memory_order_relaxed)) < kHeartbeatIntervalMs)
                return;
            lastTick.store(now, std::memory_order_relaxed);

            // rawPos is what the tracker actually sent, in metres, straight off
            // the wire. Since the mod maps the pose 1:1 it is also the answer to
            // "is this the mod or my tracker profile", which is nearly always the
            // question being asked.
            // vFov is the camera's own vertical field of view, in degrees, and
            // ratio its width-to-height - the two numbers the crosshair offset is
            // scaled by. Reported because a crosshair that tracks the head but
            // sits consistently short or long is a field-of-view fault, and it
            // looks exactly like a projection fault from the outside. Against
            // KCD2's default 95-degree horizontal setting at 16:9 these read
            // 63.1 and 1.778.
            Log::Line("heartbeat viewUpdates=%llu enabled=%s udpPort=%d/%s udpData=%s "
                      "raw=(Y=%.2f P=%.2f R=%.2f) "
                      "rawPos=(%.3f %.3f %.3f) pos=%s yawMode=%s smoothing=%s "
                      "vFov=%.1fdeg ratio=%.3f",
                      static_cast<unsigned long long>(updates),
                      state.enabled ? "ON" : "OFF",
                      g_config.udp_port, state.PortState(),
                      state.haveData ? "YES" : "NO",
                      static_cast<double>(yaw), static_cast<double>(pitch), static_cast<double>(roll),
                      havePos ? static_cast<double>(px) : 0.0,
                      havePos ? static_cast<double>(py) : 0.0,
                      havePos ? static_cast<double>(pz) : 0.0,
                      state.positionActive ? "ON" : "OFF",
                      state.worldYaw ? "world" : "local",
                      state.remote ? "remote" : "local",
                      cameraunlock::math::ToDegrees(static_cast<double>(view_hook::FovRadians())),
                      static_cast<double>(view_hook::ProjectionRatio()));
        }

        HMODULE WaitForGameModule()
        {
            for (int waited = 0; waited < kModuleWaitMs; waited += kModulePollMs)
            {
                if (HMODULE module = GetModuleHandleW(kGameModule)) return module;
                Sleep(kModulePollMs);
            }
            Log::Line("%ls never appeared after %d ms - staying dormant.",
                      kGameModule, kModuleWaitMs);
            return nullptr;
        }

        // Returns whether the port was free right now. False is not a failure:
        // the receiver's supervisor keeps retrying and picks the port up on its
        // own the moment whatever holds it exits, so the only thing the caller
        // does with this is tell the player which of the two states they are in.
        bool StartTracking()
        {
            g_receiver = std::make_unique<cameraunlock::UdpReceiver>();
            g_receiver->SetLog([](const std::string& line) { Log::Line("%s", line.c_str()); });
            const bool bound = g_receiver->Start(static_cast<std::uint16_t>(g_config.udp_port));

            g_session = std::make_unique<Session>(*g_receiver);
            g_session->SetLocalSmoothing(g_config.local_smoothing);
            g_session->SetRemoteSmoothing(g_config.remote_smoothing);
            g_session->SetMaxExtrapolationFraction(g_config.max_extrapolation_fraction);
            g_session->SetPositionSettings(cameraunlock::PositionSettings(
                1.0f, 1.0f, 1.0f,
                g_config.limit_x, g_config.limit_y, g_config.limit_y_down,
                g_config.limit_z, g_config.limit_z_back,
                g_config.local_smoothing, g_config.remote_smoothing));
            g_session->SetMode(StartupMode(g_config));

            Runtime().trackingEnabled.store(g_config.enable_on_startup);
            Runtime().worldSpaceYaw.store(g_config.world_space_yaw);
            Runtime().trueFreeLook.store(g_config.true_free_look);
            return bound;
        }

        // Truncates on every launch and keeps one previous generation as
        // HeadTracking.prev.log - both are Log::Open's own behaviour, including
        // the warning it writes when the rotation fails.
        void OpenLog()
        {
            Log::Open(ExeDirectory() + L"\\HeadTracking.log");
            Log::Line("=== Kingdom Come: Deliverance II Head Tracking (CryEngine) ===");
        }

        void LogConfig()
        {
            Log::Line("config: port=%d enabled=%s worldYaw=%s rotation=%s pos=%s trueFreeLook=%s "
                      "local=%.2f remote=%.2f "
                      "limits=(x %.2f, y +%.2f/-%.2f, z %.2f fwd/%.2f back)",
                      g_config.udp_port,
                      g_config.enable_on_startup ? "yes" : "no",
                      g_config.world_space_yaw ? "yes" : "no",
                      g_config.rotation_enabled ? "on" : "off",
                      g_config.position_enabled ? "on" : "off",
                      g_config.true_free_look ? "on" : "off",
                      static_cast<double>(g_config.local_smoothing),
                      static_cast<double>(g_config.remote_smoothing),
                      static_cast<double>(g_config.limit_x),
                      static_cast<double>(g_config.limit_y),
                      static_cast<double>(g_config.limit_y_down),
                      static_cast<double>(g_config.limit_z),
                      static_cast<double>(g_config.limit_z_back));
        }

        // Both hook sites create their trampolines through the one MinHook
        // instance, so it is brought up here rather than by whichever of them
        // happens to install first.
        bool InitializeHooking()
        {
            const hooks::HookStatus status = hooks::HookManager::Instance().Initialize();
            if (status == hooks::HookStatus::Ok) return true;
            Log::Line("MinHook init failed: %s - staying dormant.",
                      hooks::HookStatusToString(status));
            return false;
        }

        void LogReadyLine(bool portBound)
        {
            const std::string portLine =
                portBound ? "Waiting for a tracker on UDP " + std::to_string(g_config.udp_port)
                          : "UDP " + std::to_string(g_config.udp_port) +
                                " is held by another app (a game you left running, or OpenTrack"
                                " bound as a receiver) - close it and tracking starts within a"
                                " second, no restart needed";
            Log::Line("init complete. Toggle tracking: %s. Cycle mode (6DOF / rotation only / "
                      "lean only): %s. Yaw mode: %s. True free look: %s."
                      " %s. Centre in your tracker app - this mod keeps no centre of its own.",
                      g_config.toggle_key.c_str(), g_config.cycle_tracking_mode_key.c_str(),
                      g_config.yaw_mode_key.c_str(), g_config.true_free_look_key.c_str(),
                      portLine.c_str());
        }

        void Bootstrap()
        {
            OpenLog();
            cameraunlock::diagnostics::InstallCrashHandler();

            // Reads CameraUnlock.ini, or imports HeadTracking.ini into it or creates
            // it, on this thread rather than under the loader lock, and after the
            // log is open so its lines have somewhere to go.
            g_owner = std::make_unique<cameraunlock::config::ConfigOwner<Config>>(
                OwnerOptions(ExeDirectory(), cameraunlock::config::DefaultsFile::PerUser()));
            const cameraunlock::config::ConfigLoadResult<Config> loaded = g_owner->Load();
            for (const std::string& line : loaded.log) Log::Line("%s", line.c_str());
            g_config = loaded.config;
            LogConfig();

            HMODULE module = WaitForGameModule();
            if (!module) return;

            if (builds::SelectProfile(module) != builds::SelectResult::Matched) return;
            Log::Line("build profile %s active (WHGame.dll at 0x%p).",
                      builds::ActiveProfile().Name, module);

            if (!InitializeHooking()) return;

            const bool portBound = StartTracking();
            const auto moduleBase = reinterpret_cast<std::uintptr_t>(module);
            if (!view_hook::Install(moduleBase, *g_session, &LogHeartbeat)) return;
            cursor::Install(moduleBase);

            g_hotkeys = StartHotkeys(*g_session, g_config, *g_owner);
            LogReadyLine(portBound);
        }

        DWORD WINAPI BootstrapThread(LPVOID)
        {
            Bootstrap();

            // Releases the reference Initialize took and exits in one step, so
            // there is no window where the image can be unmapped while this
            // thread is still returning through it. Does not return.
            if (HMODULE self = g_selfReference)
            {
                g_selfReference = nullptr;
                FreeLibraryAndExitThread(self, 0);
            }
            return 0;
        }
    }

    void Initialize(HMODULE)
    {
        // FROM_ADDRESS on a function in this image, which increments the module's
        // reference count - the handle DllMain was handed is not a reference and
        // does not keep the image mapped. The loader lock is already held on this
        // thread and is recursive, so taking it again here is safe; loading a
        // library would not be, which is why the bootstrap runs on its own thread.
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                reinterpret_cast<LPCWSTR>(&Initialize), &g_selfReference))
            g_selfReference = nullptr;

        g_bootstrapThread = CreateThread(nullptr, 0, BootstrapThread, nullptr, 0, nullptr);
        if (g_bootstrapThread == nullptr && g_selfReference != nullptr)
        {
            FreeLibrary(g_selfReference);
            g_selfReference = nullptr;
        }
    }

    void Shutdown()
    {
        // Hooks first: both detours read the session and the receiver, and
        // MinHook's teardown is the only thing here that waits for a thread
        // parked inside one of them to leave.
        hooks::HookManager::Instance().Shutdown();
        if (g_hotkeys) g_hotkeys->Stop();
        if (g_receiver) g_receiver->Stop();
        Log::Line("shutdown");
        Log::Close();
        if (g_bootstrapThread)
        {
            CloseHandle(g_bootstrapThread);
            g_bootstrapThread = nullptr;
        }
    }
}
