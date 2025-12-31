#ifndef MICRO_IDLE_WORLD_STATE_H
#define MICRO_IDLE_WORLD_STATE_H

namespace components {

// World state singleton - stores current world dimensions for systems
struct WorldState {
    float worldWidth{50.0f};   // Current world width (accounts for 32px margin)
    float worldHeight{50.0f};  // Current world height (accounts for 32px margin)
    int screenWidth{1280};      // Current screen width
    int screenHeight{720};      // Current screen height
    bool spawnEnabled{true};    // Allow SpawnSystem to create new microbes
};

} // namespace components

#endif
