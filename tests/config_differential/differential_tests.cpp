// The differential test for the config conversion.
//
// Oracle: the reader of the newest published build (the rolling dev pre-release,
// ed0140b, core f92be69), vendored under oracle/published/ byte for byte, with the
// startup code that turned its output into the session's state.
// Import: the frozen reader in src/legacy_config/, through the mod's startup code.
//
// Comparison 1, oracle against import, runs on every input: the published build's
// first-run file, every other committed version of that file, no file, an empty
// file and the corpus core generates from the first-run file. It compares every
// field both Config types have, floats bit for bit, the startup state and the
// registered hotkey bindings. The differences it finds, each with the commit that
// made it, are exactly these, and every other field has to match:
//
//   - [ADS] AdsMode is no longer read, and the paused / marker / tracked cycle it
//     seeded is gone. abebb77.
//   - [Hotkeys] AdsModeKey is no longer read, and neither it nor Ctrl+Shift+U is
//     registered. abebb77.
//
// The published build never refuses a file, so no input is refused.
//
// What is recorded here, and checked by hash below:
//   - The published builds: only the dev pre-release, at ed0140b. No v* tag and no
//     predecessor repo exists.
//   - oracle/published/src: ed0140b:src/{config.h,config.cpp,logging.h}.
//   - oracle/published/core: the core sources those include, at f92be69, the pin
//     ed0140b built against.
//   - The frozen import: src/legacy_config/{legacy_config.h,legacy_config.cpp}. It
//     compiles core's IniReader, which hashes equal to f92be69's; legacy_import.h
//     gives it only the LegacyKey type, and file_log only its log lines.
//   - inputs/: the first-run file of each distinct committed version of
//     WriteDefaultConfigIfMissing up to ed0140b (fd75b15, b95f216 and the published
//     ed0140b one). No build shipped a config file in its ZIP or launcher seed; the
//     mod wrote one at first launch.

#include <windows.h>

#include <bcrypt.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <cameraunlock/config/testing/ini_mutations.h>
#include <cameraunlock/input/key_bindings.h>

#include "config.h"
#include "hotkey_bindings.h"
#include "legacy_config/legacy_config.h"
#include "oracle/oracle.h"

namespace {

namespace fs = std::filesystem;
using cameraunlock::config::testing::GenerateIniMutations;
using cameraunlock::config::testing::IniMutation;
using cameraunlock::config::testing::MutationKey;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

int g_failures = 0;

void Check(bool condition, const std::string& name) {
    std::cout << (condition ? "  [PASS] " : "  [FAIL] ") << name << "\n";
    if (!condition) ++g_failures;
}

// A failure in a loop over the corpus, printed once per input.
void Fail(const std::string& name) {
    std::cout << "  [FAIL] " << name << "\n";
    ++g_failures;
}

std::string ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.string());
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void WriteBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + path.string());
    out << bytes;
}

std::string Sha256(const std::string& bytes) {
    unsigned char digest[32] = {};
    const NTSTATUS status = BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                                       reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),
                                       static_cast<ULONG>(bytes.size()), digest, sizeof(digest));
    if (status != 0) throw std::runtime_error("BCryptHash failed");
    std::string hex;
    char two[3];
    for (const unsigned char b : digest) {
        std::snprintf(two, sizeof(two), "%02x", b);
        hex += two;
    }
    return hex;
}

bool SameBits(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

// A fresh folder under %TEMP% for the whole run, removed at the end.
class Scratch {
public:
    Scratch() {
        root_ = fs::temp_directory_path() /
                ("kcd2-config-differential-" + std::to_string(GetCurrentProcessId()));
        Clear();
        fs::create_directories(root_);
    }
    ~Scratch() { Clear(); }

    // A new empty folder, never reused within the run.
    fs::path Folder(const char* kind) {
        const fs::path dir = root_ / (std::string(kind) + "-" + std::to_string(next_++));
        fs::create_directories(dir);
        return dir;
    }

private:
    void Clear() {
        if (!fs::exists(root_)) return;
        for (const auto& entry : fs::recursive_directory_iterator(root_))
            SetFileAttributesW(entry.path().c_str(), FILE_ATTRIBUTE_NORMAL);
        fs::remove_all(root_);
    }

    fs::path root_;
    int next_ = 0;
};

struct Input {
    std::string name;
    bool present = true;
    std::string bytes;
};

// Every key the frozen reader reads, with a valid value other than the shipped
// one and, for each range it clamps or refuses, values on both sides of it.
std::vector<MutationKey> Descriptors() {
    const auto k = [](const char* section, const char* key, const char* alternate,
                      std::vector<std::string> out_of_range, bool hotkey = false) {
        MutationKey d;
        d.section = section;
        d.key = key;
        d.alternate = alternate;
        d.out_of_range = std::move(out_of_range);
        d.hotkey = hotkey;
        return d;
    };
    return {
        k("HeadTracking", "UdpPort", "5252", {"1023", "65536"}),
        k("HeadTracking", "EnableOnStartup", "false", {}),
        k("HeadTracking", "WorldSpaceYaw", "false", {}),
        k("HeadTracking", "MoveCrosshair", "false", {}),
        k("HeadTracking", "LocalSmoothing", "0.25", {"-0.5", "1.5"}),
        k("HeadTracking", "RemoteSmoothing", "0.5", {"-1", "2"}),
        k("HeadTracking", "MaxExtrapolationFraction", "0.25", {"-0.1", "1.1"}),
        k("HeadTracking", "Smoothing", "0.3", {}),
        k("HeadTracking", "YawSensitivity", "2.0", {}),
        k("HeadTracking", "ShowReticle", "false", {}),
        k("Hotkeys", "RecenterKey", "0x24", {}),
        k("Position", "Enabled", "false", {}),
        k("Position", "LimitX", "0.25", {"0.001", "6"}),
        k("Position", "LimitY", "0.3", {"0.001", "6"}),
        k("Position", "LimitYDown", "0.1", {"0.001", "6"}),
        k("Position", "LimitZ", "0.5", {"0.001", "6"}),
        k("Position", "LimitZBack", "0.05", {"0.001", "6"}),
        k("Hotkeys", "ToggleKey", "0x70", {"0", "0xFF"}, true),
        k("Hotkeys", "PositionKey", "0x71", {"0", "0xFF"}, true),
        k("Hotkeys", "YawModeKey", "0x72", {"0", "0xFF"}, true),
    };
}

const fs::path kDir = KCD2_DIFFERENTIAL_DIR;
const fs::path kRepo = KCD2_REPO_DIR;

std::string PublishedFirstRun() { return ReadBytes(kDir / "inputs" / "first-run-dev-ed0140b.ini"); }

std::vector<Input> Inputs() {
    std::vector<Input> inputs;
    inputs.push_back({"first run of the published build (dev, ed0140b)", true, PublishedFirstRun()});
    inputs.push_back({"first run as committed at fd75b15, 804f603 and 4b522a9", true,
                      ReadBytes(kDir / "inputs" / "first-run-fd75b15.ini")});
    inputs.push_back({"first run as committed at b95f216", true,
                      ReadBytes(kDir / "inputs" / "first-run-b95f216.ini")});
    inputs.push_back({"no file", false, {}});
    inputs.push_back({"empty file", true, {}});
    for (IniMutation& m : GenerateIniMutations(PublishedFirstRun(), kcd2_ht::legacy::Keys(), Descriptors()))
        inputs.push_back({"corpus: " + m.name, true, std::move(m.bytes)});
    return inputs;
}

// What both builds can be compared on: every field their Config types share,
// and the startup state.
struct Observed {
    int udp_port = 0;
    bool enable_on_startup = false;
    bool world_space_yaw = false;
    bool move_crosshair = false;
    float local_smoothing = 0.0f;
    float remote_smoothing = 0.0f;
    float max_extrapolation_fraction = 0.0f;
    bool position_enabled = false;
    float limit_x = 0.0f;
    float limit_y = 0.0f;
    float limit_y_down = 0.0f;
    float limit_z = 0.0f;
    float limit_z_back = 0.0f;
    int toggle_key = 0;
    int position_key = 0;
    int yaw_mode_key = 0;

    bool tracking_enabled = false;
    int tracking_mode = 0;
    bool world_yaw = false;
    std::vector<KeyBinding> toggle;
    std::vector<KeyBinding> cycle_tracking_mode;
    std::vector<KeyBinding> yaw_mode;
};

std::vector<KeyBinding> Bindings(const std::vector<kcd2_config_oracle::Binding>& published) {
    std::vector<KeyBinding> out;
    for (const auto& b : published) out.push_back({static_cast<KeyModifiers>(b.modifiers), b.vk});
    return out;
}

Observed FromOracle(const kcd2_config_oracle::Result& r) {
    Observed o;
    o.udp_port = r.udp_port;
    o.enable_on_startup = r.enable_on_startup;
    o.world_space_yaw = r.world_space_yaw;
    o.move_crosshair = r.move_crosshair;
    o.local_smoothing = r.local_smoothing;
    o.remote_smoothing = r.remote_smoothing;
    o.max_extrapolation_fraction = r.max_extrapolation_fraction;
    o.position_enabled = r.position_enabled;
    o.limit_x = r.limit_x;
    o.limit_y = r.limit_y;
    o.limit_y_down = r.limit_y_down;
    o.limit_z = r.limit_z;
    o.limit_z_back = r.limit_z_back;
    o.toggle_key = r.toggle_key;
    o.position_key = r.position_key;
    o.yaw_mode_key = r.yaw_mode_key;
    o.tracking_enabled = r.tracking_enabled;
    o.tracking_mode = r.tracking_mode;
    o.world_yaw = r.world_yaw;
    o.toggle = Bindings(r.toggle);
    o.cycle_tracking_mode = Bindings(r.cycle_tracking_mode);
    o.yaw_mode = Bindings(r.yaw_mode);
    return o;
}

// The mod's own startup on <dir>, which reads through the frozen reader.
Observed FromImport(const fs::path& dir) {
    const std::string exeDir = dir.string();
    kcd2_ht::WriteDefaultConfigIfMissing(exeDir);
    kcd2_ht::Config c;
    kcd2_ht::LoadConfig(exeDir, c);

    Observed o;
    o.udp_port = c.udp_port;
    o.enable_on_startup = c.enable_on_startup;
    o.world_space_yaw = c.world_space_yaw;
    o.move_crosshair = c.move_crosshair;
    o.local_smoothing = c.local_smoothing;
    o.remote_smoothing = c.remote_smoothing;
    o.max_extrapolation_fraction = c.max_extrapolation_fraction;
    o.position_enabled = c.position_enabled;
    o.limit_x = c.limit_x;
    o.limit_y = c.limit_y;
    o.limit_y_down = c.limit_y_down;
    o.limit_z = c.limit_z;
    o.limit_z_back = c.limit_z_back;
    o.toggle_key = c.toggle_key;
    o.position_key = c.position_key;
    o.yaw_mode_key = c.yaw_mode_key;
    o.tracking_enabled = c.enable_on_startup;
    o.tracking_mode = static_cast<int>(kcd2_ht::StartupMode(c));
    o.world_yaw = c.world_space_yaw;
    const kcd2_ht::HotkeyBindings bindings = kcd2_ht::BindingsFor(c);
    o.toggle = bindings.toggle;
    o.cycle_tracking_mode = bindings.cycle_tracking_mode;
    o.yaw_mode = bindings.yaw_mode;
    return o;
}

std::vector<std::string> Differences(const Observed& a, const Observed& b) {
    std::vector<std::string> d;
    const auto field = [&d](bool same, const char* name) {
        if (!same) d.push_back(name);
    };
    field(a.udp_port == b.udp_port, "udp_port");
    field(a.enable_on_startup == b.enable_on_startup, "enable_on_startup");
    field(a.world_space_yaw == b.world_space_yaw, "world_space_yaw");
    field(a.move_crosshair == b.move_crosshair, "move_crosshair");
    field(SameBits(a.local_smoothing, b.local_smoothing), "local_smoothing");
    field(SameBits(a.remote_smoothing, b.remote_smoothing), "remote_smoothing");
    field(SameBits(a.max_extrapolation_fraction, b.max_extrapolation_fraction), "max_extrapolation_fraction");
    field(a.position_enabled == b.position_enabled, "position_enabled");
    field(SameBits(a.limit_x, b.limit_x), "limit_x");
    field(SameBits(a.limit_y, b.limit_y), "limit_y");
    field(SameBits(a.limit_y_down, b.limit_y_down), "limit_y_down");
    field(SameBits(a.limit_z, b.limit_z), "limit_z");
    field(SameBits(a.limit_z_back, b.limit_z_back), "limit_z_back");
    field(a.toggle_key == b.toggle_key, "toggle_key");
    field(a.position_key == b.position_key, "position_key");
    field(a.yaw_mode_key == b.yaw_mode_key, "yaw_mode_key");
    field(a.tracking_enabled == b.tracking_enabled, "startup: tracking enabled");
    field(a.tracking_mode == b.tracking_mode, "startup: tracking mode");
    field(a.world_yaw == b.world_yaw, "startup: yaw mode");
    field(a.toggle == b.toggle, "hotkeys: toggle");
    field(a.cycle_tracking_mode == b.cycle_tracking_mode, "hotkeys: cycle tracking mode");
    field(a.yaw_mode == b.yaw_mode, "hotkeys: yaw mode");
    return d;
}

std::vector<fs::path> Listing(const fs::path& dir) {
    std::vector<fs::path> names;
    for (const auto& entry : fs::directory_iterator(dir)) names.push_back(entry.path().filename());
    return names;
}

// The oracle is the published source and the core sources it compiled against,
// unchanged; the import compiles core's IniReader, which is identical to the one
// the published build compiled.
void FrozenSourceTests() {
    std::cout << "Frozen sources\n";
    struct Recorded {
        fs::path path;
        const char* sha256;
    };
    const fs::path published = kDir / "oracle" / "published";
    const fs::path core = kRepo / "cameraunlock-core" / "cpp";
    const Recorded recorded[] = {
        {published / "src" / "config.h", "eed97fa0660121300647cca508549c291c1d0cebad6e6964db98c5176f4f40f4"},
        {published / "src" / "config.cpp", "46d3f40c4762616199cb492777475f481e4fa542648243a22259cf43255d22ae"},
        {published / "src" / "logging.h", "e839d762359102210e51eaf76a25aa7a56355012da022a2872689cc8d7cf1895"},
        {published / "core/include/cameraunlock/config/ini_reader.h",
         "a7ffb44210ff59672fa97e8e5feaa2cb3e81938fcc0334a384c68bc371b3857a"},
        {published / "core/include/cameraunlock/math/finite_utils.h",
         "c59772d698d54ade3374ee1221b74f5563a86d76f0eb34a989ef7efab389c0ad"},
        {published / "core/include/cameraunlock/protocol/port_utils.h",
         "91bff564d5e279b66527ec5553afcf4d591812dab0e78f71a48ef412e4db7a44"},
        {published / "core/include/cameraunlock/ads/ads_mode.h",
         "94cd36b585e878e673566f602e9496417cb797970162fcc9c2af1e50e24358de"},
        {published / "core/include/cameraunlock/data/position_settings.h",
         "b24dceb8e25475aebc5a468a5c7362a4a4e64204d183d1408525345f32f547f5"},
        {published / "core/include/cameraunlock/math/smoothing_utils.h",
         "fc2146f8c585e5f610c7234e302f59de4945679cfa28ff479ca47477ec073f22"},
        {published / "core/include/cameraunlock/math/angle_utils.h",
         "d7a905270933e3cb0c4c361d29d3fd701655498cbcd1875ea79d180468bdbe6a"},
        {published / "core/include/cameraunlock/logging/file_log.h",
         "43bdd2ef8554c78e5f440333463750c13b95110fe672b0b6e273244df9e7d169"},
        {published / "core/src/config/ini_reader.cpp",
         "e01515c2656aaf533bae4350dc45b702c3e3d4043935743dcc9bd5ea581fbe1c"},
        {published / "core/src/logging/file_log.cpp",
         "73c53c2baa06bbfebe8211f62678aa2b60cb95f604743d3686951ba56b87ea47"},
        {core / "include/cameraunlock/config/ini_reader.h",
         "a7ffb44210ff59672fa97e8e5feaa2cb3e81938fcc0334a384c68bc371b3857a"},
        {core / "src/config/ini_reader.cpp", "e01515c2656aaf533bae4350dc45b702c3e3d4043935743dcc9bd5ea581fbe1c"},
        {kRepo / "src/legacy_config/legacy_config.h",
         "91da6292f96c1c73a9e41b379573524e3adc6e2aec5e92ca8f84a4bfeb64dc7a"},
        {kRepo / "src/legacy_config/legacy_config.cpp",
         "e370ebf8fc3d4eda587c2b86b8a0741764fb915c53aa1b707f2751f5225e2c32"},
    };
    for (const Recorded& r : recorded)
        Check(Sha256(ReadBytes(r.path)) == r.sha256, r.path.lexically_relative(kRepo).generic_string() +
                                                         " hashes as recorded");
}

void FirstRunTests(Scratch& scratch) {
    std::cout << "Published first run\n";
    const fs::path dir = scratch.Folder("first-run");
    kcd2_config_oracle::Startup(dir.string());
    Check(ReadBytes(dir / "HeadTracking.ini") == PublishedFirstRun(),
          "the published build's first run writes inputs/first-run-dev-ed0140b.ini byte for byte");
}

void ComparisonOne(Scratch& scratch) {
    std::cout << "Comparison 1: published build against the import\n";
    const std::vector<Input> inputs = Inputs();
    int compared = 0;
    for (const Input& input : inputs) {
        const fs::path oracleDir = scratch.Folder("oracle");
        const fs::path importDir = scratch.Folder("import");
        if (input.present) {
            WriteBytes(oracleDir / "HeadTracking.ini", input.bytes);
            WriteBytes(importDir / "HeadTracking.ini", input.bytes);
            SetFileAttributesW((importDir / "HeadTracking.ini").c_str(), FILE_ATTRIBUTE_READONLY);
        }

        const kcd2_config_oracle::Result published = kcd2_config_oracle::Startup(oracleDir.string());
        const Observed imported = FromImport(importDir);

        for (const std::string& field : Differences(FromOracle(published), imported))
            Fail(input.name + ": " + field + " differs from the published build");
        if (published.ads_mode_cycle.size() != 2)
            Fail(input.name + ": the published build registered no ADS mode cycle");

        if (input.present) {
            if (Listing(importDir) != std::vector<fs::path>{"HeadTracking.ini"}
                    || ReadBytes(importDir / "HeadTracking.ini") != input.bytes)
                Fail(input.name + ": the import changed the folder of a read-only file");
        }
        ++compared;
    }
    Check(compared > 1000, std::to_string(compared) + " inputs compared");
}

}  // namespace

int main() {
    std::cout << "Kingdom Come: Deliverance II config differential test\n";
    try {
        Scratch scratch;
        FrozenSourceTests();
        FirstRunTests(scratch);
        ComparisonOne(scratch);
    } catch (const std::exception& e) {
        std::cout << "  [FAIL] threw: " << e.what() << "\n";
        ++g_failures;
    }
    if (g_failures == 0) {
        std::cout << "All differential tests passed\n";
        return 0;
    }
    std::cout << g_failures << " differential test(s) FAILED\n";
    return 1;
}
