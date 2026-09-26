// Characterization tests for the path split the log and the INI both sit on.
// DirectoryOf is the only thing standing between GetModuleFileName and the two
// files this mod writes beside the game exe, and its answers for the awkward
// inputs (no separator at all, a trailing separator, a bare drive root) are what
// decides whether those land next to KingdomCome.exe or somewhere the player
// will never find them.

#include "exe_paths.h"

#include <windows.h>

#include "test_support.h"

namespace {

using kcd_tests::Check;

void NarrowTests(int& failures)
{
    using kcd2_ht::DirectoryOf;

    Check(failures, DirectoryOf(std::string("C:\\Games\\KCD2\\KingdomCome.exe"))
                        == "C:\\Games\\KCD2",
          "a Windows path drops the file name and keeps no trailing separator");
    Check(failures, DirectoryOf(std::string("C:/Games/KCD2/KingdomCome.exe")) == "C:/Games/KCD2",
          "forward slashes split the same way");
    Check(failures, DirectoryOf(std::string("KingdomCome.exe")) == ".",
          "a bare file name falls back to the working directory");
    Check(failures, DirectoryOf(std::string("")) == ".",
          "an empty path falls back to the working directory");
    Check(failures, DirectoryOf(std::string("C:\\Games\\KCD2\\")) == "C:\\Games\\KCD2",
          "a trailing separator is stripped rather than doubled");
    Check(failures, DirectoryOf(std::string("C:\\KingdomCome.exe")) == "C:",
          "a file at a drive root reports the drive");
}

void WideTests(int& failures)
{
    using kcd2_ht::DirectoryOf;

    Check(failures, DirectoryOf(std::wstring(L"C:\\Games\\KCD2\\KingdomCome.exe"))
                        == L"C:\\Games\\KCD2",
          "the wide overload splits identically");
    Check(failures, DirectoryOf(std::wstring(L"KingdomCome.exe")) == L".",
          "the wide overload falls back the same way");
}

// ExeDirectory reads the running process, so the exact answer is whatever built
// the test binary. What is checkable is the contract its callers rely on: a
// non-empty directory with no trailing separator.
void RunningProcessTests(int& failures)
{
    const std::wstring wide = kcd2_ht::ExeDirectory();

    Check(failures, !wide.empty(),
          "the exe directory is never empty, so the log path is never a bare file name");
    if (wide.empty()) return;

    Check(failures, wide.back() != L'\\' && wide.back() != L'/',
          "the exe directory carries no trailing separator, so appending one cannot double it");
}

// The fallback when the exe path cannot be read. The config owner throws for a
// path that is not fully qualified, so "." would end the game process.
void WorkingDirectoryTests(int& failures)
{
    const std::wstring dir = kcd2_ht::WorkingDirectory();

    const bool drive = dir.size() >= 2 && dir[1] == L':';
    const bool unc = dir.size() >= 2 && dir[0] == L'\\' && dir[1] == L'\\';
    Check(failures, drive || unc, "the working directory is a full path, which the config owner accepts");
    if (dir.empty()) return;
    Check(failures, dir.back() != L'\\' && dir.back() != L'/',
          "the working directory carries no trailing separator");

    wchar_t previous[MAX_PATH]{};
    GetCurrentDirectoryW(MAX_PATH, previous);
    SetCurrentDirectoryW(L"C:\\");
    const std::wstring root = kcd2_ht::WorkingDirectory();
    SetCurrentDirectoryW(previous);
    Check(failures, root == L"C:",
          "a drive root drops its separator too, so the settings path is C:\\CameraUnlock.ini");
}

}  // namespace

int RunExePathsTests()
{
    int failures = 0;
    std::cout << "Exe path tests\n";

    NarrowTests(failures);
    WideTests(failures);
    RunningProcessTests(failures);
    WorkingDirectoryTests(failures);

    return kcd_tests::Report("Exe path tests", failures);
}
