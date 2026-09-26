#include "config.h"

#include <string>
#include <utility>
#include <vector>
#include <windows.h>

#include <cameraunlock/config/config_concepts.g.h>
#include <cameraunlock/config/value_codecs.h>
#include <cameraunlock/input/key_bindings.h>

#include "exe_paths.h"
#include "legacy_config/legacy_config.h"
#include "logging.h"

namespace kcd2_ht {

namespace {

namespace config = cameraunlock::config;
using config::DroppedValue;
using config::DropRule;
using config::ImportResult;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

// The Ctrl+Shift letters every pre-canonical build bound beside each nav key,
// hard-coded, which the import folds into each action's list.
constexpr KeyModifiers kChord = KeyModifiers::kCtrl | KeyModifiers::kShift;
constexpr int kVkY = 0x59;
constexpr int kVkG = 0x47;
constexpr int kVkH = 0x48;

// The frozen reader keeps every code inside 0x01-0xFE, so both bindings format.
std::string LegacyHotkey(int code, int chordLetter) {
    return cameraunlock::input::FormatKeyBindings(
        {KeyBinding{KeyModifiers::kNone, code}, KeyBinding{kChord, chordLetter}});
}

// input names the legacy file, HeadTracking.ini, which the frozen reader finds
// by its folder, as the published build did.
ImportResult RunLegacyImport(const config::LegacyInput& input, Config& out) {
    legacy::Config read;
    legacy::LoadConfig(DirectoryOf(input.ansi_path), read);

    std::vector<DroppedValue> dropped;
    out.udp_port = read.udp_port;
    out.enable_on_startup = read.enable_on_startup;
    out.world_space_yaw = read.world_space_yaw;
    // [Position] Enabled only chose the startup mode: 6DOF, or rotation only.
    out.rotation_enabled = true;
    out.position_enabled = read.position_enabled;
    out.local_smoothing = read.local_smoothing;
    out.remote_smoothing = read.remote_smoothing;
    out.max_extrapolation_fraction = read.max_extrapolation_fraction;
    out.limit_x = read.limit_x;
    out.limit_y = read.limit_y;
    out.limit_y_down = read.limit_y_down;
    out.limit_z = read.limit_z;
    out.limit_z_back = read.limit_z_back;
    out.toggle_key = LegacyHotkey(read.toggle_key, kVkY);
    out.cycle_tracking_mode_key = LegacyHotkey(read.position_key, kVkG);
    out.yaw_mode_key = LegacyHotkey(read.yaw_mode_key, kVkH);
    // The game's crosshair now always follows the aim.
    if (!read.move_crosshair)
        dropped.push_back({DropRule::Reticle, "HeadTracking", "MoveCrosshair", "false"});

    // The frozen reader finds the file the way the published build did, with
    // GetFileAttributesA on the ANSI path.
    if (GetFileAttributesA(input.ansi_path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return ImportResult::Absent(std::move(dropped));
    return ImportResult::Imported(std::move(dropped));
}

}  // namespace

const char* const kGameDisplayName = "Kingdom Come: Deliverance II";

config::ConfigTable<Config> ConfigTableFor() {
    using C = config::schema::Concept;
    config::ConfigTable<Config> table{Config{}};
    table.Concept<C::UdpPort>(&Config::udp_port)
        .Concept<C::EnableOnStartup>(&Config::enable_on_startup)
        .Concept<C::WorldSpaceYaw>(&Config::world_space_yaw).Writable()
        .Concept<C::RotationEnabled>(&Config::rotation_enabled).Writable()
        .Concept<C::LocalSmoothing>(&Config::local_smoothing)
        .Concept<C::RemoteSmoothing>(&Config::remote_smoothing)
        .Local("Smoothing", "MaxExtrapolationFraction", &Config::max_extrapolation_fraction,
               config::FloatCodec(0.0f, 1.0f),
               "How far past the newest tracker sample the view may carry on moving,\n"
               "as a fraction of the time between samples. 0 only moves between samples.")
        .Concept<C::PositionEnabled>(&Config::position_enabled).Writable()
        .Concept<C::TrueFreeLook>(&Config::true_free_look).Writable()
        .Concept<C::PositionLimitX>(&Config::limit_x)
        .Concept<C::PositionLimitY>(&Config::limit_y)
        .Concept<C::PositionLimitYDown>(&Config::limit_y_down)
        .Concept<C::PositionLimitZ>(&Config::limit_z)
        .Concept<C::PositionLimitZBack>(&Config::limit_z_back)
        .Concept<C::ToggleKey>(&Config::toggle_key)
        .Concept<C::CycleTrackingModeKey>(&Config::cycle_tracking_mode_key)
        .Concept<C::YawModeKey>(&Config::yaw_mode_key)
        .Concept<C::TrueFreeLookKey>(&Config::true_free_look_key);
    return table;
}

config::LegacyImport<Config> LegacyImportFor() {
    config::LegacyImport<Config> import;
    import.run = &RunLegacyImport;
    import.keys = legacy::Keys();
    return import;
}

config::ConfigOwnerOptions<Config> OwnerOptions(const std::wstring& directory, config::DefaultsFile defaults) {
    config::ConfigOwnerOptions<Config> options;
    options.path = directory + L"\\CameraUnlock.ini";
    options.table = ConfigTableFor();
    options.import = LegacyImportFor();
    options.legacy_path = directory + L"\\HeadTracking.ini";
    options.header.display_name = kGameDisplayName;
    options.defaults = std::move(defaults);
    // The mod draws no text of its own, so the player's message goes to the log.
    options.status_sink = [](const std::string& message) { Log::Line("%s", message.c_str()); };
    return options;
}

cameraunlock::TrackingMode StartupMode(const Config& config) {
    return cameraunlock::DecodeTrackingMode(config.rotation_enabled, config.position_enabled).value();
}

}  // namespace kcd2_ht
