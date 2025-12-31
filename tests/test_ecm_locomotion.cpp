#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "src/systems/ECMLocomotionSystem.h"
#include "src/components/ECMLocomotion.h"

using namespace micro_idle;
TEST_CASE("ECMLocomotion - Initialize locomotion state", "[ecm_locomotion]") {
    components::ECMLocomotion locomotion = {};
    ECMLocomotionSystem::initialize(locomotion, 123.0f);

    for (int i = 0; i < components::ECMLocomotion::CortexSamples; i++) {
        REQUIRE(locomotion.memory[i] > 0.0f);
        REQUIRE(locomotion.inhibitor[i] >= 0.0f);
    }

    for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
        REQUIRE(locomotion.pods[i].index == -1);
        REQUIRE(locomotion.pods[i].state == 0);
        REQUIRE(locomotion.pods[i].time == 0.0f);
        REQUIRE(locomotion.pods[i].duration == 0.0f);
        REQUIRE(locomotion.pods[i].extent == 0.0f);
        REQUIRE(locomotion.pods[i].anchorLocal.x == 0.0f);
        REQUIRE(locomotion.pods[i].anchorLocal.y == 0.0f);
        REQUIRE(locomotion.pods[i].anchorLocal.z == 0.0f);
        REQUIRE(locomotion.pods[i].anchorSet == false);
    }
    REQUIRE(std::abs(locomotion.targetDirection.y) < 0.001f);
    REQUIRE((locomotion.zigzagSign == -1 || locomotion.zigzagSign == 1));
}
