cmake_minimum_required(VERSION 3.15)
project(micro_idle C)

option(DEV "Enable dev mode" ON)
option(DETERMINISM "Enable deterministic simulation" OFF)
option(ENABLE_VULKAN "Enable Vulkan compute path" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(GRAPHICS GRAPHICS_API_OPENGL_43 CACHE STRING "" FORCE)
option(USE_SYSTEM_RAYLIB "Use system-installed raylib" OFF)

if (USE_SYSTEM_RAYLIB)
    find_package(raylib REQUIRED)
else()
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/third_party/raylib/CMakeLists.txt")
        add_subdirectory(third_party/raylib)
    else()
        include(FetchContent)
        # Enable Wayland/EGL for zink compatibility (WSL2)
        set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 5.5
        )
        FetchContent_MakeAvailable(raylib)
    endif()
endif()

if (WIN32)
    set(ENABLE_VULKAN OFF)
endif()

if (ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
endif()

add_executable(game
    bin/main.c
    engine/platform/engine.c
    engine/platform/time.c
    engine/util/rng.c
    game/game.c
    game/gpu_sim.c
    $<$<BOOL:${ENABLE_VULKAN}>:game/vk_compute.c>
    game/xpbd.c
)

target_include_directories(game PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(game PRIVATE raylib)
if (ENABLE_VULKAN)
    target_link_libraries(game PRIVATE Vulkan::Vulkan)
endif()

target_compile_definitions(game PRIVATE
    $<$<BOOL:${DEV}>:DEV_MODE>
    $<$<BOOL:${DETERMINISM}>:DETERMINISM_MODE>
    $<$<BOOL:${ENABLE_VULKAN}>:ENABLE_VULKAN>
    GRAPHICS_API_OPENGL_46
)

add_executable(tests
    tests/test_main.c
    tests/test_time.c
    tests/test_rng.c
    tests/test_engine.c
    tests/test_game_constants.c
    tests/test_gpu_sim.c
    tests/test_game.c
    tests/test_render_output.c
    engine/platform/engine.c
    engine/platform/time.c
    engine/util/rng.c
    game/game.c
    game/gpu_sim.c
    $<$<BOOL:${ENABLE_VULKAN}>:game/vk_compute.c>
    game/xpbd.c
)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(tests PRIVATE raylib)
if (ENABLE_VULKAN)
    target_link_libraries(tests PRIVATE Vulkan::Vulkan)
endif()
target_compile_definitions(tests PRIVATE GPU_SIM_TESTING)
if (NOT WIN32)
    target_compile_options(tests PRIVATE --coverage -O0)
    target_link_options(tests PRIVATE --coverage)
endif()

if (UNIX AND NOT APPLE)
    target_link_libraries(game PRIVATE m)
endif()
