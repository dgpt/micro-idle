#ifndef MICRO_IDLE_WORLD_H
#define MICRO_IDLE_WORLD_H

#include <flecs.h>
#include "raylib.h"

namespace micro_idle {

struct PhysicsSystemState; // Forward declaration
struct WorldBoundaries;     // Forward declaration

} // namespace micro_idle

namespace components {
    struct Microbe; // Forward declaration
}

namespace micro_idle {

class World {
public:
    World();
    ~World();

    // Core update methods
    void update(float dt);
    void render(Camera3D camera, float alpha);
    void handleInput(Camera3D camera, float dt, int screen_w, int screen_h);
    void renderUI(int screen_w, int screen_h);

    // Entity creation helpers
    flecs::entity createTestSphere(Vector3 position, float radius, Color color, bool withPhysics = false, bool isStatic = false);
    flecs::entity createAmoeba(Vector3 position, float radius, Color color);

    // Screen boundary management
    void createScreenBoundaries(float worldWidth, float worldHeight);
    void updateScreenBoundaries(float worldWidth, float worldHeight);
    void repositionMicrobesInBounds(float worldWidth, float worldHeight);

    // Access to underlying FLECS world
    flecs::world& getWorld() { return world; }

    // Public for testing
    PhysicsSystemState* physics;

private:
    flecs::world world;
    Shader sdfMembraneShader;  // SDF raymarching shader for microbe membranes
    WorldBoundaries* boundaries;   // Screen boundaries (opaque)

    // System registration
    void registerComponents();
    void registerSystems();
    void registerPhysicsObservers();
};

} // namespace micro_idle

#endif
