// Characterization tests for the path split the log and the INI both sit on.
// DirectoryOf is the only thing standing between GetModuleFileName and the two
// files this mod writes beside the game exe, and its answers for the awkward
// inputs (no separator at all, a trailing separator, a bare drive root) are what
// decides whether those land next to KingdomCome.exe or somewhere the player
// will never find them.

#include "exe_paths.h"

#include <algorithm>

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
// the test binary. What is checkable is the contract both callers rely on: a
// non-empty directory with no trailing separator, in both character widths, and
// the two agreeing with each other.
void RunningProcessTests(int& failures)
{
    const std::wstring wide = kcd2_ht::ExeDirectory();
    const std::string narrow = kcd2_ht::ExeDirectoryNarrow();

    Check(failures, !wide.empty() && !narrow.empty(),
          "the exe directory is never empty, so the log path is never a bare file name");
    if (wide.empty() || narrow.empty()) return;

    Check(failures, wide.back() != L'\\' && wide.back() != L'/'
                 && narrow.back() != '\\' && narrow.back() != '/',
          "the exe directory carries no trailing separator, so appending one cannot double it");
    // Only compared while the path stays ASCII: the narrow form is the active
    // code page's rendering of the same characters, and outside ASCII that is
    // free to be a different number of bytes.
    const bool ascii = std::all_of(wide.begin(), wide.end(), [](wchar_t c) { return c < 128; });
    Check(failures, !ascii || std::equal(wide.begin(), wide.end(), narrow.begin(), narrow.end()),
          "an ASCII exe directory reads the same in both character widths");
}

}  // namespace

int RunExePathsTests()
{
    int failures = 0;
    std::cout << "Exe path tests\n";

    NarrowTests(failures);
    WideTests(failures);
    RunningProcessTests(failures);

    return kcd_tests::Report("Exe path tests", failures);
}
