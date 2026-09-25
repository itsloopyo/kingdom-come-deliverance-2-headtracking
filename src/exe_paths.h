#pragma once

#include <string>

// Where the game EXE lives - the log and the INI sit beside it. DirectoryOf has
// a narrow overload because the frozen legacy reader opens the INI through
// GetPrivateProfileStringA, by the path's ANSI form.

namespace kcd2_ht
{
    // Everything before the last path separator, with no trailing separator.
    // "." when the path has none.
    std::wstring DirectoryOf(const std::wstring& path);
    std::string  DirectoryOf(const std::string& path);

    std::wstring ExeDirectory();
}
