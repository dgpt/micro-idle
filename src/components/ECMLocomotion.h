#ifndef MICRO_IDLE_ECMLocomotion_H
#define MICRO_IDLE_ECMLocomotion_H

#include "raylib.h"

namespace components {

// EC&M (Excitable Cortex & Memory) locomotion state
// Cortex modeled as 1D ring with memory and local inhibitor fields
struct ECMLocomotion {
    static constexpr int CortexSamples = 36;
    static constexpr int MaxPods = 4;
    struct Pod {
        int index{-1};
        float time{0.0f};
        float duration{0.0f};
        float angle{0.0f};
        float extent{0.0f};
        Vector3 anchorLocal{0.0f, 0.0f, 0.0f};
        bool anchorSet{false};
        int state{0}; // 0 = inactive, 1 = extending, 2 = holding, 3 = retracting
    };

    float memory[CortexSamples]{};      // Excitability enhancer M(x,t)
    float inhibitor[CortexSamples]{};   // Local inhibitor L(x,t)
    Pod pods[MaxPods]{};
    float idleTime{0.0f};               // Time since last pseudopod ended
    float lastAngle{0.0f};              // Previous pseudopod angle
    int zigzagSign{1};                  // Alternates left/right bias
    int orbitSign{1};                   // Orbit direction around cursor
    Vector3 targetDirection{0.0f, 0.0f, 1.0f}; // Current pseudopod direction
};

} // namespace components

#endif
