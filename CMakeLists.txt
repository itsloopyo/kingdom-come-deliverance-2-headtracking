cmake_minimum_required(VERSION 3.20)
project(KingdomComeDeliverance2HeadTracking VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(MSVC)
    add_compile_options(/permissive- /Zc:__cplusplus /MP /W4)
endif()

# MinHook (for the CView::Update trampoline). Fetched + built locally;
# cameraunlock-core's hook manager links against this `minhook` target.
#
# Pinned to the commit, not the tag. A tag is a movable ref: whoever controls
# the upstream repo can retag v1.3.3 at any commit, and every clean build
# machine and CI runner would then compile that instead - into a DLL that gets
# injected into the player's game. Same rule the workflows follow for actions.
include(FetchContent)
FetchContent_Declare(
    minhook_src
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        9fbd087432700d73fc571118d6a9697a36443d88 # v1.3.3
)
FetchContent_MakeAvailable(minhook_src)
add_library(minhook STATIC
    ${minhook_src_SOURCE_DIR}/src/hook.c
    ${minhook_src_SOURCE_DIR}/src/buffer.c
    ${minhook_src_SOURCE_DIR}/src/trampoline.c
    ${minhook_src_SOURCE_DIR}/src/hde/hde64.c
    ${minhook_src_SOURCE_DIR}/src/hde/hde32.c
)
target_include_directories(minhook PUBLIC ${minhook_src_SOURCE_DIR}/include)
if(MSVC)
    target_compile_options(minhook PRIVATE /wd4267 /wd4244 /wd4127 /wd4131)
endif()

set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

set(MOD_SOURCES
    src/dllmain.cpp
    src/headtracking_mod.cpp
    src/config.cpp
    src/exe_paths.cpp
    src/hotkeys.cpp
    src/view_hook.cpp
    src/view_injection.cpp
    src/aim_projection.cpp
    src/cursor_hook.cpp
    src/builds/build_registry.cpp
    src/builds/steam_offsets.cpp
)

add_library(KingdomComeDeliverance2HeadTracking SHARED ${MOD_SOURCES})

target_include_directories(KingdomComeDeliverance2HeadTracking PRIVATE
    src
    cameraunlock-core/cpp/include
)

target_link_libraries(KingdomComeDeliverance2HeadTracking PRIVATE
    cameraunlock
    cameraunlock_hooks
    minhook
    ws2_32
)

# Output as .asi for Ultimate ASI Loader.
set_target_properties(KingdomComeDeliverance2HeadTracking PROPERTIES
    SUFFIX ".asi"
    PREFIX ""
)

# Characterization tests for the parts of the injection that are decidable
# without a game process: the camera maths, the INI contract and the build
# registry. `pixi run test` builds and runs them.
option(KCD2_BUILD_TESTS "Build unit tests" ON)
if(KCD2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
