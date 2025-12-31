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

    REQUIRE(locomotion.activeIndex == -1);
    REQUIRE(locomotion.activeTime == 0.0f);
    REQUIRE(locomotion.activeDuration == 0.0f);
    REQUIRE(std::abs(locomotion.targetDirection.y) < 0.001f);
    REQUIRE((locomotion.zigzagSign == -1 || locomotion.zigzagSign == 1));
}
