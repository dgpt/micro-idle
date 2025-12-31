#ifndef MICRO_IDLE_ECMLocomotion_H
#define MICRO_IDLE_ECMLocomotion_H

#include "raylib.h"

namespace components {

// EC&M (Excitable Cortex & Memory) locomotion state
// Cortex modeled as 1D ring with memory and local inhibitor fields
struct ECMLocomotion {
    static constexpr int CortexSamples = 36;

    float memory[CortexSamples]{};      // Excitability enhancer M(x,t)
    float inhibitor[CortexSamples]{};   // Local inhibitor L(x,t)
    int activeIndex{-1};                // Active pseudopod cortex index
    float activeTime{0.0f};             // Time since pseudopod started
    float activeDuration{0.0f};         // Target duration for pseudopod
    float idleTime{0.0f};               // Time since last pseudopod ended
    float activeAngle{0.0f};            // Angle of active pseudopod
    float lastAngle{0.0f};              // Previous pseudopod angle
    int zigzagSign{1};                  // Alternates left/right bias
    Vector3 targetDirection{0.0f, 0.0f, 1.0f}; // Current pseudopod direction
};

} // namespace components

#endif
