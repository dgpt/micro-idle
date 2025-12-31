#ifndef MICRO_IDLE_ECM_LOCOMOTION_SYSTEM_H
#define MICRO_IDLE_ECM_LOCOMOTION_SYSTEM_H

#include <flecs.h>
#include "src/components/Microbe.h"
#include "src/components/ECMLocomotion.h"
#include "src/systems/PhysicsSystem.h"

namespace micro_idle {

// EC&M (Excitable Cortex & Memory) Locomotion System
// Implements biologically-grounded amoeba movement as described in README.md
class ECMLocomotionSystem {
public:
    // Register the EC&M locomotion system with FLECS
    static void registerSystem(flecs::world& world, PhysicsSystemState* physics);

    // Initialize EC&M state for new microbe
    static void initialize(components::ECMLocomotion& locomotion, float seed);

private:
    static constexpr int CortexSamples = components::ECMLocomotion::CortexSamples;

    // EC&M parameters (scaled for real-time simulation)
    static constexpr float K0 = 0.1f;
    static constexpr float K1 = 1.4f;
    static constexpr float TAU_M = 30.0f;
    static constexpr float D_M = 0.14f;
    static constexpr float K_L = 0.3f;
    static constexpr float TAU_L = 2.33f;
    static constexpr float D_L = 0.1f;
    static constexpr float ECM_EPSILON = 1.0e-3f; // Scaled up from paper for visible motion
    static constexpr float A = 34.0f;
    static constexpr float B = 11.0f;
    static constexpr float MU = 1.0f;

    // Motion tuning
    static constexpr float MIN_PSEUDOPOD_DURATION = 0.9f;
    static constexpr float MAX_PSEUDOPOD_DURATION = 2.0f;
    static constexpr float START_COOLDOWN = 0.25f;
    static constexpr float FORCE_MAGNITUDE = 8.0f;
    static constexpr float CONTRACTION_MAGNITUDE = 5.0f;
    static constexpr float BODY_FORCE = 120.0f;
    static constexpr float FORCE_RAMP_TIME = 0.3f;
    static constexpr float ZIGZAG_STRENGTH = 0.35f;

    // System update function (called by FLECS)
    static void update(
        flecs::entity e,
        components::Microbe& microbe,
        components::ECMLocomotion& locomotion,
        PhysicsSystemState* physics,
        float dt
    );

    static void stepCortex(components::ECMLocomotion& locomotion, float dt);
    static bool shouldStopPseudopod(const components::ECMLocomotion& locomotion, float dt);
    static bool tryStartPseudopod(components::ECMLocomotion& locomotion, float dt);
    static void applyPseudopodForces(
        components::ECMLocomotion& locomotion,
        components::Microbe& microbe,
        PhysicsSystemState* physics,
        float dt
    );
};

} // namespace micro_idle

#endif
