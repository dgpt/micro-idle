#include "engine/platform/time.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("Time system - basic step", "[time]") {
    TimeState state;
    time_init(&state, 60);

    int steps = time_update(&state, 1.0 / 60.0);
    REQUIRE(steps == 1);
    REQUIRE(state.tick == 1);
}

TEST_CASE("Time system - zero delta time", "[time]") {
    TimeState state;
    time_init(&state, 60);

    int steps = time_update(&state, 0.0);
    REQUIRE(steps == 0);
}

TEST_CASE("Time system - half tick accumulation", "[time]") {
    TimeState state;
    time_init(&state, 60);

    int steps = time_update(&state, 1.0 / 120.0);
    REQUIRE(steps == 0);
    REQUIRE(std::abs(time_alpha(&state) - 0.5f) < 0.05f);
}

TEST_CASE("Time system - negative delta time clamping", "[time]") {
    TimeState state;
    time_init(&state, 60);

    int steps = time_update(&state, -1.0);
    REQUIRE(steps == 0);
    REQUIRE(state.real_dt == 0.0);
}

TEST_CASE("Time system - max step clamping", "[time]") {
    TimeState state;
    time_init(&state, 60);

    int steps = time_update(&state, state.tick_dt * 20.0);
    REQUIRE(steps == 9);
}

TEST_CASE("Time system - alpha with zero tick_dt", "[time]") {
    TimeState state;
    time_init(&state, 60);
    state.tick_dt = 0.0;

    REQUIRE(std::abs(time_alpha(&state) - 0.0f) < 0.0001f);
}
