// The canonical HeadTracking.ini: the committed file the table renders, what the
// owner creates and reads, what each hotkey saves and what it may not, and the
// frozen legacy reader's own contract.

#include "config.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <windows.h>

#include <cameraunlock/config/config_owner.h>
#include <cameraunlock/input/key_bindings.h>

#include "hotkey_bindings.h"
#include "legacy_config/legacy_config.h"
#include "test_support.h"

namespace {

namespace fs = std::filesystem;
using kcd_tests::Check;
using kcd_tests::NearEqual;
using cameraunlock::config::ConfigLoadStatus;
using cameraunlock::config::ConfigOwner;
using cameraunlock::config::ConfigSaveStatus;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

const fs::path kCommitted = fs::path(KCD2_REPO_DIR) / "HeadTracking.ini";

std::string ReadBytes(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void WriteBytes(const fs::path& path, const std::string& bytes)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << bytes;
}

fs::path FreshDir(const char* name)
{
    const fs::path dir = fs::temp_directory_path() / "kcd-ht-config-tests" / name;
    std::error_code ignored;
    fs::remove_all(dir, ignored);
    fs::create_directories(dir);
    return dir;
}

// The lines of @p a that differ from @p b, which must have as many.
std::vector<std::string> ChangedLines(const std::string& a, const std::string& b)
{
    std::vector<std::string> la, lb, changed;
    std::istringstream sa(a), sb(b);
    for (std::string line; std::getline(sa, line);) la.push_back(line);
    for (std::string line; std::getline(sb, line);) lb.push_back(line);
    if (la.size() != lb.size()) return {"(line count differs)"};
    for (std::size_t i = 0; i < la.size(); ++i)
        if (la[i] != lb[i]) changed.push_back(lb[i]);
    return changed;
}

std::string Render()
{
    cameraunlock::config::RenderHeader header;
    header.display_name = kcd2_ht::kGameDisplayName;
    const auto table = kcd2_ht::ConfigTableFor();
    return cameraunlock::config::RenderCanonical(table, table.defaults(), header);
}

void CommittedFileTests(int& failures)
{
    Check(failures, ReadBytes(kCommitted) == Render(),
          "HeadTracking.ini is the table's render of its defaults (pixi run render-config rewrites it)");

    const fs::path dir = FreshDir("created");
    ConfigOwner<kcd2_ht::Config> owner(kcd2_ht::OwnerOptions((dir / "HeadTracking.ini").wstring()));
    const auto loaded = owner.Load();
    Check(failures, loaded.status == ConfigLoadStatus::Created, "a missing file is created");
    Check(failures, ReadBytes(dir / "HeadTracking.ini") == ReadBytes(kCommitted),
          "and holds the committed file byte for byte");

    const kcd2_ht::Config& c = loaded.config;
    Check(failures, c.udp_port == 4242 && c.enable_on_startup && c.world_space_yaw
                 && c.rotation_enabled && c.position_enabled && !c.true_free_look,
          "defaults: port 4242, on at startup, world yaw, 6DOF, sights locked");
    Check(failures, NearEqual(c.local_smoothing, 0.0f) && NearEqual(c.remote_smoothing, 0.15f)
                 && NearEqual(c.max_extrapolation_fraction, 0.5f),
          "defaults: smoothing 0 local, 0.15 remote, extrapolation 0.5");
    Check(failures, NearEqual(c.limit_z, 0.40f) && NearEqual(c.limit_z_back, 0.10f),
          "the Z limits default asymmetric: more room to lean in than back");
    Check(failures, c.toggle_key == "End, Ctrl+Shift+Y"
                 && c.cycle_tracking_mode_key == "PageUp, Ctrl+Shift+G"
                 && c.yaw_mode_key == "PageDown, Ctrl+Shift+H"
                 && c.true_free_look_key == "Insert, Ctrl+Shift+U",
          "the hotkeys default to the fleet's nav keys and chords");

    const auto again = owner.Reload();
    Check(failures, again.status == cameraunlock::config::ConfigReloadStatus::Unchanged,
          "reloading the created file finds nothing to apply");
}

void CanonicalReadTests(int& failures)
{
    const fs::path dir = FreshDir("canonical");
    std::string text = ReadBytes(kCommitted);
    const auto set = [&text](const std::string& from, const std::string& to) {
        const std::size_t at = text.find(from);
        if (at == std::string::npos) throw std::logic_error(from + " is not in the committed file");
        text.replace(at, from.size(), to);
    };
    set("UdpPort=4242", "UdpPort=5252");
    set("EnableOnStartup=true", "EnableOnStartup=false");
    set("WorldSpaceYaw=true", "WorldSpaceYaw=false");
    set("RotationEnabled=true", "RotationEnabled=false");
    set("TrueFreeLook=false", "TrueFreeLook=true");
    set("MaxExtrapolationFraction=0.5", "MaxExtrapolationFraction=0.0");
    set("PositionLimitYDown=0.2", "PositionLimitYDown=0.05");
    set("ToggleKey=End, Ctrl+Shift+Y", "ToggleKey=F9");
    WriteBytes(dir / "HeadTracking.ini", text);

    ConfigOwner<kcd2_ht::Config> owner(kcd2_ht::OwnerOptions((dir / "HeadTracking.ini").wstring()));
    const auto loaded = owner.Load();
    const kcd2_ht::Config& c = loaded.config;
    Check(failures, loaded.status == ConfigLoadStatus::Canonical && loaded.diagnostics.empty(),
          "a stamped file is read as canonical with nothing to report");
    Check(failures, c.udp_port == 5252 && !c.enable_on_startup && !c.world_space_yaw
                 && !c.rotation_enabled && c.position_enabled && c.true_free_look
                 && NearEqual(c.max_extrapolation_fraction, 0.0f)
                 && NearEqual(c.limit_y_down, 0.05f) && c.toggle_key == "F9",
          "every edited row is read");
    Check(failures, kcd2_ht::StartupMode(c) == cameraunlock::TrackingMode::PositionOnly,
          "RotationEnabled=false with PositionEnabled=true starts position only");
}

void SaveTests(int& failures)
{
    const fs::path dir = FreshDir("save");
    const fs::path file = dir / "HeadTracking.ini";
    ConfigOwner<kcd2_ht::Config> owner(kcd2_ht::OwnerOptions(file.wstring()));
    owner.Load();

    std::string before = ReadBytes(file);
    Check(failures, owner.Save([](kcd2_ht::Config& c) { c.world_space_yaw = false; }).status
                     == ConfigSaveStatus::Saved,
          "the yaw mode saves");
    Check(failures, ChangedLines(before, ReadBytes(file)) == std::vector<std::string>{"WorldSpaceYaw=false\r"},
          "and changes the WorldSpaceYaw line and no other byte");

    before = ReadBytes(file);
    const cameraunlock::TrackingModeChannels rotationOnly =
        cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::RotationOnly);
    owner.Save([rotationOnly](kcd2_ht::Config& c) {
        c.rotation_enabled = rotationOnly.rotation_enabled;
        c.position_enabled = rotationOnly.position_enabled;
    });
    Check(failures, ChangedLines(before, ReadBytes(file)) == std::vector<std::string>{"PositionEnabled=false\r"},
          "a mode change to rotation only writes the pair, of which only PositionEnabled differs");

    before = ReadBytes(file);
    owner.Save([](kcd2_ht::Config& c) { c.true_free_look = true; });
    Check(failures, ChangedLines(before, ReadBytes(file)) == std::vector<std::string>{"TrueFreeLook=true\r"},
          "true free look saves its own line and no other byte");

    bool refused = false;
    try {
        owner.Save([](kcd2_ht::Config& c) { c.enable_on_startup = false; });
    } catch (const std::logic_error&) {
        refused = true;
    }
    Check(failures, refused, "EnableOnStartup is not Writable, so nothing End does can reach the file");

    ConfigOwner<kcd2_ht::Config> next(kcd2_ht::OwnerOptions(file.wstring()));
    const kcd2_ht::Config restarted = next.Load().config;
    Check(failures, !restarted.world_space_yaw && restarted.rotation_enabled && !restarted.position_enabled
                 && restarted.true_free_look && restarted.enable_on_startup,
          "every saved toggle comes back at the next start");
}

void HotkeyBindingTests(int& failures)
{
    const kcd2_ht::HotkeyBindings b = kcd2_ht::BindingsFor(kcd2_ht::Config{});
    constexpr KeyModifiers kChord = KeyModifiers::kCtrl | KeyModifiers::kShift;
    Check(failures, b.toggle == std::vector<KeyBinding>{{KeyModifiers::kNone, 0x23}, {kChord, 0x59}},
          "End and Ctrl+Shift+Y toggle tracking");
    Check(failures, b.cycle_tracking_mode == std::vector<KeyBinding>{{KeyModifiers::kNone, 0x21}, {kChord, 0x47}},
          "Page Up and Ctrl+Shift+G cycle the tracking mode");
    Check(failures, b.yaw_mode == std::vector<KeyBinding>{{KeyModifiers::kNone, 0x22}, {kChord, 0x48}},
          "Page Down and Ctrl+Shift+H switch the yaw mode");
    Check(failures, b.true_free_look == std::vector<KeyBinding>{{KeyModifiers::kNone, 0x2D}, {kChord, 0x55}},
          "Insert and Ctrl+Shift+U switch true free look");
}

// The frozen reader, as the published builds read HeadTracking.ini.
void LegacyReaderTests(int& failures)
{
    const auto read = [](const char* name, const char* body) {
        const fs::path dir = FreshDir(name);
        WriteBytes(dir / "HeadTracking.ini", body);
        kcd2_ht::legacy::Config config;
        kcd2_ht::legacy::LoadConfig(dir.string(), config);
        return config;
    };

    const auto raised = read("legacy-limit-y", "[Position]\nLimitY=0.40\n");
    Check(failures, NearEqual(raised.limit_y, 0.40f) && NearEqual(raised.limit_y_down, 0.40f),
          "legacy: LimitY without LimitYDown gives the same travel each way");

    const auto hotkeys = read("legacy-hotkeys", "[Hotkeys]\nToggleKey=0\nPositionKey=0x1FF\nYawModeKey=zzz\n");
    Check(failures, hotkeys.toggle_key == 0x23 && hotkeys.position_key == 0x21 && hotkeys.yaw_mode_key == 0x22,
          "legacy: a hotkey outside 0x01-0xFE keeps its default");

    const auto bad = read("legacy-values", "[HeadTracking]\nUdpPort=99999\nLocalSmoothing=nan\n[Position]\nLimitZ=-3\n");
    Check(failures, bad.udp_port == 4242 && NearEqual(bad.local_smoothing, 0.0f) && bad.limit_z > 0.0f,
          "legacy: an out-of-range port, a NaN and a negative limit do not get through");
}

}  // namespace

std::string RenderCommittedConfig() { return Render(); }

int RunConfigTests()
{
    int failures = 0;
    std::cout << "Config tests\n";

    CommittedFileTests(failures);
    CanonicalReadTests(failures);
    SaveTests(failures);
    HotkeyBindingTests(failures);
    LegacyReaderTests(failures);

    return kcd_tests::Report("Config tests", failures);
}
