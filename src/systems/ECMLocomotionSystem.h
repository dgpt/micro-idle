#ifndef MICRO_IDLE_ECM_LOCOMOTION_SYSTEM_H
#define MICRO_IDLE_ECM_LOCOMOTION_SYSTEM_H

#include "src/components/Microbe.h"
#include "src/systems/PhysicsSystem.h"

namespace micro_idle {

// EC&M (Excitable Cortex & Memory) Locomotion System
// Implements biologically-grounded amoeba movement as described in README.md
class ECMLocomotionSystem {
public:
    // EC&M cycle constants (from README.md) - public for initialization
    static constexpr float CYCLE_DURATION = 120.0f; // 12-second cycle
    static constexpr float EXTEND_PHASE = 0.33f;   // 0.0 - 0.33
    static constexpr float SEARCH_PHASE = 0.67f;   // 0.33 - 0.67
    static constexpr float RETRACT_PHASE = 1.0f;   // 0.67 - 1.0

    // Update EC&M locomotion for one microbe
    static void update(
        components::Microbe& microbe,
        PhysicsSystemState* physics,
        float dt
    );

    // Initialize EC&M state for new microbe
    static void initialize(components::ECMLocomotion& locomotion);

private:

    // Force magnitudes (10x increase for dramatic visible deformation)
    static constexpr float PSEUDOPOD_EXTENSION_FORCE = 150.0f;
    static constexpr float WIGGLE_FORCE = 50.0f;
    static constexpr float RETRACT_FORCE = 100.0f;

    // Apply forces based on current phase
    static void applyExtensionForces(
        components::Microbe& microbe,
        PhysicsSystemState* physics,
        float dt
    );

    static void applySearchForces(
        components::Microbe& microbe,
        PhysicsSystemState* physics,
        float dt
    );

    static void applyRetractionForces(
        components::Microbe& microbe,
        PhysicsSystemState* physics,
        float dt
    );
};

} // namespace micro_idle

#endif
