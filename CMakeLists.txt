cmake_minimum_required(VERSION 3.15)
project(micro_idle C)

option(DEV "Enable dev mode" ON)
option(DETERMINISM "Enable deterministic simulation" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(raylib QUIET)
if (NOT raylib_FOUND)
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/third_party/raylib/CMakeLists.txt")
        add_subdirectory(third_party/raylib)
    else()
        include(FetchContent)
        FetchContent_Declare(raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 5.5
        )
        FetchContent_MakeAvailable(raylib)
    endif()
endif()

add_executable(game
    apps/game/main.c
    engine/engine.c
    engine/time.c
    engine/util/rng.c
    game/game.c
)

target_include_directories(game PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(game PRIVATE raylib)

target_compile_definitions(game PRIVATE
    $<$<BOOL:${DEV}>:DEV_MODE>
    $<$<BOOL:${DETERMINISM}>:DETERMINISM_MODE>
)

add_executable(tests
    apps/tests/test_main.c
    apps/tests/test_time.c
    apps/tests/test_rng.c
    apps/tests/test_game_constants.c
    engine/time.c
    engine/util/rng.c
)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(tests PRIVATE raylib)

if (UNIX AND NOT APPLE)
    target_link_libraries(game PRIVATE m)
    target_link_libraries(tests PRIVATE m)
endif()
