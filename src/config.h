#pragma once

#include <string>

namespace kcd2_ht {

struct Config {
    int udp_port = 4242;
    bool enable_on_startup = true;

    // true = yaw about the world up-axis (horizon-locked); false = yaw about
    // the camera's own up-axis, which leans on pitched turns.
    bool world_space_yaw = true;

    int toggle_key = 0x23;    // End
    int position_key = 0x21;  // Page Up
    int yaw_mode_key = 0x22;  // Page Down

    // Two smoothing parameters, picked per connection from the packet source
    // address. Both cover rotation and position. There is no third knob and no
    // hidden floor - see AGENTS.md "Smoothing Model".
    float local_smoothing = 0.0f;
    float remote_smoothing = 0.15f;

    // How far past the newest sample the interpolators may continue the last
    // velocity, as a fraction of the estimated sample interval. 0 interpolates
    // only between known samples.
    float max_extrapolation_fraction = 0.5f;

    // Move the game's own crosshair to where the shot actually goes. It is
    // fixed to screen centre, which stops being the aim point the moment the
    // head turns. Off leaves the HUD completely untouched.
    bool move_crosshair = true;

    bool position_enabled = true;
    float limit_x = 0.30f;
    float limit_y = 0.20f;
    float limit_y_down = 0.20f;
    float limit_z = 0.40f;
    float limit_z_back = 0.10f;
};

// There is deliberately NO per-axis sensitivity or inversion here. The tracker
// owns pose shaping (AGENTS.md), and a backwards axis is a boundary-conversion
// bug to fix in view_injection.cpp, not a knob to hand the player.

// Both take the directory holding KingdomCome.exe; the INI sits beside it.
// Keys absent from the file keep the defaults above, so a partial INI is valid.
// LoadConfig validates every value it reads: a key that is out of range or not
// a number keeps its default and the substitution is logged, so nothing here is
// ever NaN, infinite, or outside the range its consumer can take.
void LoadConfig(const std::string& exeDir, Config& out);
void WriteDefaultConfigIfMissing(const std::string& exeDir);

}
