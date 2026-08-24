#pragma once

#include <windows.h>

namespace kcd2_ht
{
    // Called from DllMain. Spawns the bootstrap thread and returns immediately -
    // nothing that waits, hooks or loads a library may run under the loader lock.
    void Initialize(HMODULE module);

    // Called from DLL_PROCESS_DETACH on an explicit FreeLibrary only.
    void Shutdown();
}
