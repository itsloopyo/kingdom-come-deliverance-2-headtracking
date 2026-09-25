# Only the mod TUs whose behaviour is decidable without a game process are
# compiled in - headtracking_mod.cpp, view_hook.cpp, cursor_hook.cpp and
# hotkeys.cpp are left out because they only do anything against a live hook.
# config.cpp is in because it pulls core's logger and INI reader, which is what
# target_link_libraries satisfies.
add_executable(kcd_tests
    test_main.cpp
    view_injection_tests.cpp
    aim_projection_tests.cpp
    config_tests.cpp
    pose_guard_tests.cpp
    exe_paths_tests.cpp
    build_profile_tests.cpp
    ads_gate_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/view_injection.cpp
    ${CMAKE_SOURCE_DIR}/src/aim_projection.cpp
    ${CMAKE_SOURCE_DIR}/src/config.cpp
    ${CMAKE_SOURCE_DIR}/src/legacy_config/legacy_config.cpp
    ${CMAKE_SOURCE_DIR}/src/ads.cpp
    ${CMAKE_SOURCE_DIR}/src/exe_paths.cpp
    ${CMAKE_SOURCE_DIR}/src/builds/steam_offsets.cpp
    ${CMAKE_SOURCE_DIR}/src/builds/gdk_offsets.cpp
)

target_include_directories(kcd_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/cameraunlock-core/cpp/include
)

target_link_libraries(kcd_tests PRIVATE cameraunlock)

if(MSVC)
    target_compile_options(kcd_tests PRIVATE /W4 /WX)
endif()

add_test(NAME kcd_tests COMMAND kcd_tests)
add_subdirectory(config_differential)
if(WIN32)
    add_test(NAME game_installs_tests COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/game_installs_tests.ps1")
endif()
