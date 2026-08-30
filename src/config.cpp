#include "config.h"

#include <cstdint>
#include <windows.h>

#include <cameraunlock/config/ini_reader.h>
#include <cameraunlock/math/finite_utils.h>
#include <cameraunlock/protocol/port_utils.h>

#include "logging.h"

namespace kcd2_ht {

namespace {

const char* kIniName = "HeadTracking.ini";
const char* kTracking = "HeadTracking";
const char* kHotkeys = "Hotkeys";
const char* kPosition = "Position";

// Metres. Deliberately far wider than anything a player would choose - the
// bound exists to stop a typo reaching the maths, not to second-guess a setting.
constexpr float kMaxPositionLimit = 5.0f;

// Nothing downstream of the INI rejects a bad float. strtod accepts "nan" and
// "inf" and overflows a literal like 1e400 to +inf; a NaN limit then poisons the
// smoothing state for the rest of the session and presents as the view simply
// being gone, so the substitution is logged with the key that caused it instead
// of being applied quietly.
float ReadFloatChecked(const cameraunlock::IniReader& reader, const char* section,
                       const char* key, float fallback, float lo, float hi) {
    const float raw = reader.ReadFloat(section, key, fallback);
    const float value = cameraunlock::math::SanitizeFinite(raw, fallback, lo, hi);
    if (value != raw)
        Log::Line("WARNING: config [%s] %s = %g is not a number in [%g, %g] - using %g.",
            section, key, static_cast<double>(raw), static_cast<double>(lo),
            static_cast<double>(hi), static_cast<double>(value));
    return value;
}

// GetAsyncKeyState's virtual-key codes run 1..254. 0 is not a key at all: the
// poller reads it as "no binding" and skips the entry, so a mistyped ToggleKey
// would leave the player with no way to turn tracking off and nothing in the log
// saying why. Out-of-range codes are worse than useless - GetAsyncKeyState only
// defines its result inside that range.
constexpr int kMinVirtualKey = 1;
constexpr int kMaxVirtualKey = 254;

int ReadHotkeyChecked(const cameraunlock::IniReader& reader, const char* key, int fallback) {
    const int raw = reader.ReadHex(kHotkeys, key, fallback);
    if (raw >= kMinVirtualKey && raw <= kMaxVirtualKey) return raw;
    Log::Line("WARNING: config [%s] %s = 0x%X is not a virtual-key code in [0x%02X, 0x%02X] "
              "- using 0x%02X.",
              kHotkeys, key, static_cast<unsigned>(raw), static_cast<unsigned>(kMinVirtualKey),
              static_cast<unsigned>(kMaxVirtualKey), static_cast<unsigned>(fallback));
    return fallback;
}

// The old value is deliberately NOT migrated: the single Smoothing key carried a
// hidden 0.15 floor, so the number in an existing config does not mean what it
// used to, and copying it into one of the two new keys would be a guess about
// which connection the player was on.
void WarnRetiredKey(const cameraunlock::IniReader& reader, const char* section,
                    const char* key, const char* advice) {
    if (reader.ReadString(section, key, "").empty()) return;
    Log::Line("WARNING: config key [%s] %s has been retired and is IGNORED. %s",
              section, key, advice);
}

std::string IniPath(const std::string& exeDir) {
    return exeDir + "\\" + kIniName;
}

}  // namespace

void LoadConfig(const std::string& exeDir, Config& out) {
    const std::string path = IniPath(exeDir);

    cameraunlock::IniReader reader;
    if (!reader.Open(path)) {
        Log::Line("No %s beside the game exe - using defaults.", kIniName);
        return;
    }

    bool portValid = true;
    const int rawPort = reader.ReadInt(kTracking, "UdpPort", out.udp_port);
    out.udp_port = cameraunlock::NormalizeUdpPort(rawPort,
                                                  static_cast<std::uint16_t>(out.udp_port),
                                                  portValid);
    if (!portValid)
        Log::Line("WARNING: config [%s] UdpPort = %d is out of range - using %d.",
                  kTracking, rawPort, out.udp_port);

    out.enable_on_startup = reader.ReadBool(kTracking, "EnableOnStartup", out.enable_on_startup);
    out.world_space_yaw = reader.ReadBool(kTracking, "WorldSpaceYaw", out.world_space_yaw);
    out.move_crosshair = reader.ReadBool(kTracking, "MoveCrosshair", out.move_crosshair);

    out.local_smoothing = ReadFloatChecked(reader, kTracking, "LocalSmoothing",
                                           out.local_smoothing, 0.0f, 1.0f);
    out.remote_smoothing = ReadFloatChecked(reader, kTracking, "RemoteSmoothing",
                                            out.remote_smoothing, 0.0f, 1.0f);
    out.max_extrapolation_fraction = ReadFloatChecked(reader, kTracking,
                                                      "MaxExtrapolationFraction",
                                                      out.max_extrapolation_fraction, 0.0f, 1.0f);

    WarnRetiredKey(reader, kTracking, "Smoothing",
                   "Smoothing is now two keys: LocalSmoothing (default 0, a tracker on this "
                   "machine) and RemoteSmoothing (default 0.15, a tracker on the network). The "
                   "old value is not migrated because the semantics changed - it carried a "
                   "hidden 0.15 floor that no longer exists.");
    WarnRetiredKey(reader, kTracking, "YawSensitivity",
                   "Sensitivity belongs in the tracker app (opentrack mapping curves, the phone "
                   "app settings) so one profile behaves the same in every game.");
    WarnRetiredKey(reader, kTracking, "ShowReticle",
                   "The mod no longer draws a reticle of its own. It moves the game's own "
                   "crosshair instead, which carries weapon and interaction state a plain dot "
                   "cannot. Use MoveCrosshair to turn that off.");
    WarnRetiredKey(reader, kHotkeys, "RecenterKey",
                   "There is no recenter binding: the tracker app owns the centre. Use the "
                   "Center bind in opentrack or the CENTER button in your phone app.");

    out.position_enabled = reader.ReadBool(kPosition, "Enabled", out.position_enabled);
    out.limit_x = ReadFloatChecked(reader, kPosition, "LimitX", out.limit_x,
                                   0.01f, kMaxPositionLimit);
    out.limit_y = ReadFloatChecked(reader, kPosition, "LimitY", out.limit_y,
                                   0.01f, kMaxPositionLimit);
    // Falls back to whatever LimitY resolved to, not to the struct default: a config
    // that sets only LimitY would otherwise keep 0.20 m of downward travel while the
    // upward budget moved, and nothing in the log would say the key was half-effective.
    out.limit_y_down = ReadFloatChecked(reader, kPosition, "LimitYDown", out.limit_y,
                                        0.01f, kMaxPositionLimit);
    out.limit_z = ReadFloatChecked(reader, kPosition, "LimitZ", out.limit_z,
                                   0.01f, kMaxPositionLimit);
    out.limit_z_back = ReadFloatChecked(reader, kPosition, "LimitZBack", out.limit_z_back,
                                        0.01f, kMaxPositionLimit);

    out.toggle_key = ReadHotkeyChecked(reader, "ToggleKey", out.toggle_key);
    out.position_key = ReadHotkeyChecked(reader, "PositionKey", out.position_key);
    out.yaw_mode_key = ReadHotkeyChecked(reader, "YawModeKey", out.yaw_mode_key);
}

void WriteDefaultConfigIfMissing(const std::string& exeDir) {
    const std::string path = IniPath(exeDir);
    if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) return;

    // Comments for bool keys go on their OWN line: the INI reader is built on
    // GetPrivateProfileStringA, which does not treat ';' as an inline comment
    // introducer, so "Enabled=true ; note" matches no known bool spelling and
    // silently falls back to the default.
    static const char kDefaults[] =
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
        "LimitYDown=0.20\r\n"
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
    const DWORD size = static_cast<DWORD>(sizeof(kDefaults) - 1);
    DWORD written = 0;
    const BOOL wrote = WriteFile(file, kDefaults, size, &written, nullptr);
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
