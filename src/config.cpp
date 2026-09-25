#include "config.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <windows.h>

#include "legacy_config/legacy_config.h"
#include "logging.h"

namespace kcd2_ht {

namespace {

const char* kIniName = "HeadTracking.ini";

std::string IniPath(const std::string& exeDir) {
    return exeDir + "\\" + kIniName;
}

}  // namespace

void LoadConfig(const std::string& exeDir, Config& out) {
    legacy::Config read;
    legacy::LoadConfig(exeDir, read);

    out.udp_port = read.udp_port;
    out.enable_on_startup = read.enable_on_startup;
    out.world_space_yaw = read.world_space_yaw;
    out.toggle_key = read.toggle_key;
    out.position_key = read.position_key;
    out.yaw_mode_key = read.yaw_mode_key;
    out.local_smoothing = read.local_smoothing;
    out.remote_smoothing = read.remote_smoothing;
    out.max_extrapolation_fraction = read.max_extrapolation_fraction;
    out.move_crosshair = read.move_crosshair;
    out.position_enabled = read.position_enabled;
    out.limit_x = read.limit_x;
    out.limit_y = read.limit_y;
    out.limit_y_down = read.limit_y_down;
    out.limit_z = read.limit_z;
    out.limit_z_back = read.limit_z_back;
}

cameraunlock::TrackingMode StartupMode(const Config& config) {
    return config.position_enabled ? cameraunlock::TrackingMode::RotationAndPosition
                                   : cameraunlock::TrackingMode::RotationOnly;
}

void WriteDefaultConfigIfMissing(const std::string& exeDir) {
    const std::string path = IniPath(exeDir);
    if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) return;

    // Comments for bool keys go on their OWN line: the INI reader is built on
    // GetPrivateProfileStringA, which does not treat ';' as an inline comment
    // introducer, so "Enabled=true ; note" matches no known bool spelling and
    // silently falls back to the default.
    //
    // LimitYDown is formatted from cameraunlock::PositionSettings{}.limit_y_down
    // rather than written as a literal, so a change to core's default cannot
    // silently disagree with the file this mod ships.
    char limitYDown[16] = {};
    std::snprintf(limitYDown, sizeof(limitYDown), "%.2f",
                  static_cast<double>(cameraunlock::PositionSettings{}.limit_y_down));

    std::string kDefaults =
        "[HeadTracking]\r\n"
        "UdpPort=4242\r\n"
        "; Start with head tracking already on.\r\n"
        "EnableOnStartup=true\r\n"
        "; Yaw about the world up-axis so the horizon stays level. Off yaws about\r\n"
        "; the camera's own up-axis, which leans the view on pitched turns.\r\n"
        "WorldSpaceYaw=true\r\n"
        "; Move the game's own crosshair to where the shot actually goes. It is\r\n"
        "; pinned to screen centre, which stops being the aim point as soon as you\r\n"
        "; turn your head. Off leaves the HUD untouched.\r\n"
        "MoveCrosshair=true\r\n"
        "; Smoothing for a tracker running on this machine (loopback). 0 = none.\r\n"
        "LocalSmoothing=0.0\r\n"
        "; Smoothing for a tracker reaching this machine over the network. A tracker\r\n"
        "; sending to this PC's LAN address instead of 127.0.0.1 counts as remote -\r\n"
        "; the classifier sees a transport, not a machine.\r\n"
        "RemoteSmoothing=0.15\r\n"
        "MaxExtrapolationFraction=0.5\r\n"
        "\r\n"
        "[Position]\r\n"
        "; 6DOF lean. Limits are metres.\r\n"
        "Enabled=true\r\n"
        "LimitX=0.30\r\n"
        "LimitY=0.20\r\n"
        "LimitYDown=";
    kDefaults += limitYDown;
    kDefaults +=
        "\r\n"
        "LimitZ=0.40\r\n"
        "LimitZBack=0.10\r\n"
        "\r\n"
        "[Hotkeys]\r\n"
        "; Windows virtual-key codes. Ctrl+Shift+Y / G / H work as alternatives.\r\n"
        "ToggleKey=0x23\r\n"
        "PositionKey=0x21\r\n"
        "YawModeKey=0x22\r\n";

    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        Log::Line("Could not create %s (error %lu) - running on defaults.",
                  kIniName, GetLastError());
        return;
    }
    // A short write leaves a half-written INI that LoadConfig will happily parse
    // on the next launch, so the player gets whatever keys landed above the cut
    // and defaults for the rest, with nothing anywhere saying so. GetLastError is
    // read before CloseHandle, which overwrites it.
    const DWORD size = static_cast<DWORD>(kDefaults.size());
    DWORD written = 0;
    const BOOL wrote = WriteFile(file, kDefaults.data(), size, &written, nullptr);
    const DWORD writeError = GetLastError();
    CloseHandle(file);
    if (!wrote || written != size) {
        Log::Line("WARNING: only %lu of %lu bytes of %s reached disk (error %lu). The file is "
                  "truncated - delete it and restart the game to get a clean one.",
                  written, size, kIniName, writeError);
        return;
    }
    Log::Line("Wrote default %s beside the game exe.", kIniName);
}

}  // namespace kcd2_ht
