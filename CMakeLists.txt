cmake_minimum_required(VERSION 3.15)

# Force Windows cross-compilation from WSL
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

project(micro_idle C CXX)

option(DEV "Enable dev mode" ON)
option(DETERMINISM "Enable deterministic simulation" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GRAPHICS GRAPHICS_API_OPENGL_43 CACHE STRING "" FORCE)
option(USE_SYSTEM_RAYLIB "Use system-installed raylib" OFF)

# ============================================================================
# Dependencies
# ============================================================================

# Raylib dependency
if (USE_SYSTEM_RAYLIB)
    find_package(raylib REQUIRED)
else()
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/third_party/raylib/CMakeLists.txt")
        add_subdirectory(third_party/raylib)
    else()
        include(FetchContent)
        # Windows cross-compilation settings
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 5.5
        )
        FetchContent_MakeAvailable(raylib)
    endif()
endif()

# FLECS (Entity Component System)
include(FetchContent)
FetchContent_Declare(flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
    GIT_TAG v4.0.3
)
FetchContent_MakeAvailable(flecs)

# Catch2 (Testing Framework)
include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(Catch2)

# Enable CTest
enable_testing()

# Jolt Physics
FetchContent_Declare(JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.2.0
)
# Jolt configuration
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
set(GENERATE_DEBUG_SYMBOLS ON CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(CROSS_PLATFORM_DETERMINISTIC OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JoltPhysics)

# Add Jolt Build directory (contains Jolt.cmake)
set(PHYSICS_REPO_ROOT ${joltphysics_SOURCE_DIR})
add_subdirectory(${joltphysics_SOURCE_DIR}/Build ${CMAKE_CURRENT_BINARY_DIR}/JoltBuild)

# ============================================================================
# Main Game Executable
# ============================================================================

add_executable(game
    bin/main.cpp
    engine/platform/engine.cpp
    engine/platform/time.cpp
    engine/util/rng.cpp
    game/game.cpp
    src/World.cpp
    src/math/Random.cpp
    src/systems/PhysicsSystem.cpp
    src/systems/SoftBodyFactory.cpp
    src/systems/ECMLocomotionSystem.cpp
    src/systems/InputSystem.cpp
    src/systems/TransformSyncSystem.cpp
    src/systems/UpdateSDFUniforms.cpp
    src/systems/SDFRenderSystem.cpp
    src/systems/SpawnSystem.cpp
    src/systems/DestructionSystem.cpp
    src/systems/ResourceSystem.cpp
    src/physics/Icosphere.cpp
    src/physics/Constraints.cpp
    src/rendering/SDFShader.cpp
    src/rendering/RaymarchBounds.cpp
)

target_include_directories(game PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(game PRIVATE
    raylib
    flecs::flecs_static
    Jolt
)

# Static link MinGW runtime (no DLL dependencies) - Windows cross-compilation
target_link_options(game PRIVATE -static-libgcc -static-libstdc++ -static)

target_compile_definitions(game PRIVATE
    $<$<BOOL:${DEV}>:DEV_MODE>
    $<$<BOOL:${DETERMINISM}>:DETERMINISM_MODE>
    GRAPHICS_API_OPENGL_46
)

# Optimization flags
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(game PRIVATE -O3 -march=native)
elseif (MSVC)
    target_compile_options(game PRIVATE /O2 /arch:AVX2)
endif()

# ============================================================================
# Tests Executable
# ============================================================================

# Common test sources
set(TEST_SOURCES
    tests/test_flecs_reset.cpp
    tests/test_time.cpp
    tests/test_rng.cpp
    tests/test_engine.cpp
    tests/test_game_constants.cpp
    tests/test_visual.cpp
    tests/test_game_render.cpp
    tests/test_rendering_debug.cpp
    tests/test_microbe_spawn_accumulation.cpp
    tests/test_microbe_persistence.cpp
    tests/test_spawn_positions.cpp
    tests/test_microbe_position_persistence.cpp
    tests/test_icosphere.cpp
    tests/test_constraints.cpp
    tests/test_softbody_factory.cpp
    tests/test_ecm_locomotion.cpp
    tests/test_sdf_render_system.cpp
    tests/test_microbe_integration.cpp
    engine/platform/engine.cpp
    engine/platform/time.cpp
    engine/util/rng.cpp
    game/game.cpp
    src/World.cpp
    src/math/Random.cpp
    src/systems/PhysicsSystem.cpp
    src/systems/SoftBodyFactory.cpp
    src/systems/ECMLocomotionSystem.cpp
    src/systems/InputSystem.cpp
    src/systems/TransformSyncSystem.cpp
    src/systems/UpdateSDFUniforms.cpp
    src/systems/SDFRenderSystem.cpp
    src/systems/SpawnSystem.cpp
    src/systems/DestructionSystem.cpp
    src/systems/ResourceSystem.cpp
    src/physics/Icosphere.cpp
    src/physics/Constraints.cpp
    src/rendering/SDFShader.cpp
    src/rendering/RaymarchBounds.cpp
)

# Create test executable with Catch2
add_executable(tests ${TEST_SOURCES})

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(tests PRIVATE
    raylib
    flecs::flecs_static
    Jolt
    Catch2::Catch2WithMain
)

# Static link MinGW runtime and set console subsystem - Windows cross-compilation
target_link_options(tests PRIVATE -static-libgcc -static-libstdc++ -static -Wl,-subsystem,console)

target_compile_definitions(tests PRIVATE GRAPHICS_API_OPENGL_46)

# Register tests with CTest
include(CTest)
# Include Catch2 test discovery (must be after FetchContent_MakeAvailable)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Bulk tests: discover normally with default 2 second timeout
catch_discover_tests(tests
    EXTRA_ARGS "--order rand"
    PROPERTIES
        TIMEOUT 2  # Default timeout for all discovered tests
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Visual/E2E tests: manually add with extended timeout
# These tests are tagged [visual] and need more time for rendering/screenshots
add_test(NAME visual::headless_game_run
         COMMAND tests "[visual]")

set_tests_properties(visual::headless_game_run PROPERTIES
    TIMEOUT 20
    LABELS "visual;e2e"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
