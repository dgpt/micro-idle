// Test microbe spawning and particle counts
#include "raylib.h"
#include "game/physics.h"
#include "tests/test_env.h"
#include <stdio.h>

int test_microbe_positions_run(void) {
    printf("test_microbe_positions: starting\n");

    // Initialize window for OpenGL context (needed for SSBOs)
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(800, 600, "test_microbe_positions");

    // Create physics context
    PhysicsContext* physics = PhysicsContext::create(10);
    if (!physics) {
        printf("FAIL: Could not create physics context\n");
        CloseWindow();
        return 1;
    }

    // Spawn 2 microbes
    physics->spawnMicrobe(0.0f, 0.0f, MicrobeType::AMOEBA, 1.0f);
    physics->spawnMicrobe(5.0f, 5.0f, MicrobeType::AMOEBA, 2.0f);

    printf("test_microbe_positions: spawned 2 microbes\n");
    printf("test_microbe_positions: microbe count = %d\n", physics->getMicrobeCount());
    printf("test_microbe_positions: particle count = %d\n", physics->getParticleCount());

    // Update physics for a few frames
    for (int i = 0; i < 60; i++) {
        physics->update(1.0f/60.0f, 10.0f, 10.0f, 0.0f, 0.0f);
    }

    printf("test_microbe_positions: ran 60 physics frames\n");
    printf("test_microbe_positions: final particle count = %d\n", physics->getParticleCount());

    // Check if particle count makes sense
    if (physics->getParticleCount() == 0) {
        printf("FAIL: No particles after spawning microbes!\n");
        physics->destroy();
        CloseWindow();
        return 1;
    }

    printf("test_microbe_positions: PASS\n");
    physics->destroy();
    CloseWindow();
    return 0;
}
