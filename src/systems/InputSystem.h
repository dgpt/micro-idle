#ifndef MICRO_IDLE_INPUT_SYSTEM_H
#define MICRO_IDLE_INPUT_SYSTEM_H

#include <flecs.h>

namespace micro_idle {

// Input system - polls Raylib input and updates FLECS InputState singleton
// Runs in OnUpdate phase (first phase of simulation loop)
class InputSystem {
public:
    // Register the input system with FLECS world
    static void registerSystem(flecs::world& world);
};

} // namespace micro_idle

#endif
