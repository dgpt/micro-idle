#ifndef MICRO_IDLE_WORLD_H
#define MICRO_IDLE_WORLD_H

#include <flecs.h>
#include "raylib.h"

namespace micro_idle {

struct PhysicsSystemState; // Forward declaration

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
    void createPetriDish(Vector3 position, float radius, float edgeHeight);

    // Access to underlying FLECS world
    flecs::world& getWorld() { return world; }

private:
    flecs::world world;
    PhysicsSystemState* physics;

    // Metaball rendering
    Shader metaballShader;
    Mesh billboardQuad;
    bool shadersLoaded;

    // System registration
    void registerComponents();
    void registerSystems();
    void registerPhysicsObservers();

    // Rendering helpers
    void loadMetaballShaders();
    void renderMicrobeParticles(const components::Microbe& microbe, Camera3D camera);
};

} // namespace micro_idle

#endif
