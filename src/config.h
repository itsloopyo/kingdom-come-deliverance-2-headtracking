#pragma once

#include <string>

#include <cameraunlock/config/config_owner.h>
#include <cameraunlock/config/config_table.h>
#include <cameraunlock/config/legacy_import.h>
#include <cameraunlock/data/position_settings.h>
#include <cameraunlock/math/smoothing_utils.h>
#include <cameraunlock/tracking/tracking_mode.h>

namespace kcd2_ht {

// HeadTracking.ini beside KingdomCome.exe, in cameraunlock-core's canonical
// format. ConfigOwner is its one reader and writer; ConfigTableFor() lists its
// rows.
struct Config {
    int udp_port = 4242;
    bool enable_on_startup = true;

    // true = yaw about the world up-axis (horizon-locked); false = yaw about
    // the camera's own up-axis, which leans on pitched turns.
    bool world_space_yaw = true;

    // The tracking mode at startup, as the pair the mode hotkey saves.
    bool rotation_enabled = true;
    bool position_enabled = true;

    // false: the lean eases out while a bow or crossbow is aimed (sights
    // locked). true: the lean stays in full (true free look).
    bool true_free_look = false;

    // Two smoothing parameters, picked per connection from the packet source
    // address. Both cover rotation and position. There is no third knob and no
    // hidden floor - see AGENTS.md "Smoothing Model".
    float local_smoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remote_smoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    // How far past the newest sample the interpolators may continue the last
    // velocity, as a fraction of the estimated sample interval. 0 interpolates
    // only between known samples.
    float max_extrapolation_fraction = 0.5f;

    float limit_x = cameraunlock::PositionSettings{}.limit_x;
    float limit_y = cameraunlock::PositionSettings{}.limit_y;
    float limit_y_down = cameraunlock::PositionSettings{}.limit_y_down;
    float limit_z = cameraunlock::PositionSettings{}.limit_z;
    float limit_z_back = cameraunlock::PositionSettings{}.limit_z_back;

    // Hotkey lists as the canonical format writes them.
    std::string toggle_key = "End, Ctrl+Shift+Y";
    std::string cycle_tracking_mode_key = "PageUp, Ctrl+Shift+G";
    std::string yaw_mode_key = "PageDown, Ctrl+Shift+H";
    std::string true_free_look_key = "Insert, Ctrl+Shift+U";
};

// There is deliberately NO per-axis sensitivity or inversion here. The tracker
// owns pose shaping (AGENTS.md), and a backwards axis is a boundary-conversion
// bug to fix in view_injection.cpp, not a knob to hand the player.

// The rows of HeadTracking.ini. The mode pair, WorldSpaceYaw and TrueFreeLook
// are Writable: their hotkeys save them. EnableOnStartup is not, so End never
// reaches the file.
cameraunlock::config::ConfigTable<Config> ConfigTableFor();

// The display name the file's header names the game by, as data/games.json
// spells it.
extern const char* const kGameDisplayName;

// The frozen reader in legacy_config/ as the owner's import: it reads a
// HeadTracking.ini an older build wrote and maps it into Config.
cameraunlock::config::LegacyImport<Config> LegacyImportFor();

// The owner of the file at @p path, a full path.
cameraunlock::config::ConfigOwnerOptions<Config> OwnerOptions(const std::wstring& path);

// The tracking mode the session starts in.
cameraunlock::TrackingMode StartupMode(const Config& config);

}
