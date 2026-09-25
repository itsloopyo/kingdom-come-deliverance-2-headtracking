#include "legacy_config/legacy_config.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include <cameraunlock/config/ini_reader.h>

#include "logging.h"

namespace kcd2_ht::legacy {

namespace {

// cameraunlock::math::SanitizeFinite and cameraunlock::NormalizeUdpPort as the
// reader called them at core f92be69, copied here because core does not freeze
// them the way it freezes IniReader.
float SanitizeFinite(float value, float fallback, float lo, float hi) {
    return std::clamp(std::isfinite(value) ? value : fallback, lo, hi);
}

std::uint16_t NormalizeUdpPort(int raw, std::uint16_t fallback, bool& valid) {
    valid = (raw >= 1024 && raw <= 65535);
    return valid ? static_cast<std::uint16_t>(raw) : fallback;
}

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
    const float value = SanitizeFinite(raw, fallback, lo, hi);
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
    out.udp_port = NormalizeUdpPort(rawPort, static_cast<std::uint16_t>(out.udp_port), portValid);
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
                   "Use MoveCrosshair for the game's crosshair.");
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

const std::vector<cameraunlock::config::LegacyKey>& Keys() {
    static const std::vector<cameraunlock::config::LegacyKey> keys = {
        {kTracking, "UdpPort"},
        {kTracking, "EnableOnStartup"},
        {kTracking, "WorldSpaceYaw"},
        {kTracking, "MoveCrosshair"},
        {kTracking, "LocalSmoothing"},
        {kTracking, "RemoteSmoothing"},
        {kTracking, "MaxExtrapolationFraction"},
        {kTracking, "Smoothing"},
        {kTracking, "YawSensitivity"},
        {kTracking, "ShowReticle"},
        {kHotkeys, "RecenterKey"},
        {kPosition, "Enabled"},
        {kPosition, "LimitX"},
        {kPosition, "LimitY"},
        {kPosition, "LimitYDown"},
        {kPosition, "LimitZ"},
        {kPosition, "LimitZBack"},
        {kHotkeys, "ToggleKey"},
        {kHotkeys, "PositionKey"},
        {kHotkeys, "YawModeKey"},
    };
    return keys;
}

}  // namespace kcd2_ht::legacy
