cmake_minimum_required(VERSION 3.15)
project(micro_idle C CXX)

option(DEV "Enable dev mode" ON)
option(DETERMINISM "Enable deterministic simulation" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GRAPHICS GRAPHICS_API_OPENGL_43 CACHE STRING "" FORCE)
option(USE_SYSTEM_RAYLIB "Use system-installed raylib" OFF)
option(USE_SYSTEM_BULLET "Use system-installed Bullet Physics" OFF)

# Raylib dependency
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

# Bullet Physics dependency
if (USE_SYSTEM_BULLET)
    find_package(Bullet REQUIRED)
else()
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/third_party/bullet3/CMakeLists.txt")
        add_subdirectory(third_party/bullet3)
    else()
        include(FetchContent)
        # Disable extras to speed up build
        set(USE_GRAPHICAL_BENCHMARK OFF CACHE BOOL "" FORCE)
        set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
        set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
        set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
        set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
        set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
        set(BUILD_BULLET3 ON CACHE BOOL "" FORCE)

        FetchContent_Declare(bullet3
            GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
            GIT_TAG 3.25
        )
        FetchContent_MakeAvailable(bullet3)
    endif()
endif()

# OpenCL for Bullet GPU acceleration
find_package(OpenCL)

add_executable(game
    bin/main.cpp
    engine/platform/engine.cpp
    engine/platform/time.cpp
    engine/util/rng.cpp
    game/game.cpp
    game/xpbd.cpp
    game/physics.cpp
    game/microbe_bodies.cpp
)

target_include_directories(game PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/_deps/bullet3-src/src
)
target_link_libraries(game PRIVATE
    raylib
    BulletSoftBody
    BulletDynamics
    BulletCollision
    LinearMath
)

# Static link MinGW runtime (so .exe doesn't need DLLs)
if (WIN32)
    target_link_options(game PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Link OpenCL if available (for GPU acceleration)
if (OpenCL_FOUND)
    target_link_libraries(game PRIVATE OpenCL::OpenCL)
    target_compile_definitions(game PRIVATE USE_OPENCL)
endif()

target_compile_definitions(game PRIVATE
    $<$<BOOL:${DEV}>:DEV_MODE>
    $<$<BOOL:${DETERMINISM}>:DETERMINISM_MODE>
    GRAPHICS_API_OPENGL_46
)

add_executable(tests
    tests/test_main.cpp
    tests/test_time.cpp
    tests/test_rng.cpp
    tests/test_engine.cpp
    tests/test_game_constants.cpp
    tests/test_physics_bullet.cpp
    tests/test_visual.cpp
    tests/test_microbe_positions.cpp
    tests/test_deflation.cpp
    engine/platform/engine.cpp
    engine/platform/time.cpp
    engine/util/rng.cpp
    game/game.cpp
    game/xpbd.cpp
    game/physics.cpp
    game/microbe_bodies.cpp
)
target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/_deps/bullet3-src/src
)
target_link_libraries(tests PRIVATE
    raylib
    BulletSoftBody
    BulletDynamics
    BulletCollision
    LinearMath
)

# Static link MinGW runtime (so .exe doesn't need DLLs)
if (WIN32)
    target_link_options(tests PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Link OpenCL if available
if (OpenCL_FOUND)
    target_link_libraries(tests PRIVATE OpenCL::OpenCL)
    target_compile_definitions(tests PRIVATE USE_OPENCL)
endif()

target_compile_definitions(tests PRIVATE GRAPHICS_API_OPENGL_46)
if (NOT WIN32)
    target_compile_options(tests PRIVATE --coverage -O0)
    target_link_options(tests PRIVATE --coverage)
endif()

if (UNIX AND NOT APPLE)
    target_link_libraries(game PRIVATE m)
    target_link_libraries(tests PRIVATE m)
endif()
