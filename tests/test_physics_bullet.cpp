#include <stdio.h>
#include "raylib.h"
#include "game/physics.h"

int test_physics_bullet_run(void) {
    printf("test_physics_bullet: starting\n");

    // Initialize OpenGL context (required for SSBOs)
    printf("test_physics_bullet: setting window flags\n");
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    printf("test_physics_bullet: initializing window\n");
    InitWindow(800, 600, "physics_test");
    if (!IsWindowReady()) {
        printf("FAIL - could not init window\n");
        return 1;
    }
    printf("test_physics_bullet: window ready\n");

    // Test 1: Create physics context
    printf("test_physics_bullet: creating physics context\n");
    fflush(stdout);  // Force flush before potentially blocking call
    PhysicsContext* ctx = PhysicsContext::create(10);
    if (!ctx) {
        printf("FAIL - could not create physics context\n");
        return 1;
    }
    printf("test_physics_bullet: physics context created\n");
    fflush(stdout);

    // Test 2: Initial state
    printf("test_physics_bullet: checking initial state\n");
    fflush(stdout);
    if (ctx->getMicrobeCount() != 0) {
        printf("FAIL - initial microbe count should be 0\n");
        return 1;
    }
    printf("test_physics_bullet: initial state OK\n");

    // Test 3: Spawn a microbe
    printf("test_physics_bullet: spawning microbe\n");
    fflush(stdout);
    ctx->spawnMicrobe(0.0f, 0.0f, MicrobeType::AMOEBA, 12345.0f);
    printf("test_physics_bullet: spawn complete\n");
    if (ctx->getMicrobeCount() != 1) {
        printf("FAIL - microbe count should be 1 after spawn\n");
        return 1;
    }
    printf("test_physics_bullet: spawn count OK\n");

    // Test 4: Physics update (should not crash)
    printf("test_physics_bullet: updating physics\n");
    fflush(stdout);
    ctx->update(1.0f/60.0f, 100.0f, 100.0f, 0.0f, 0.0f);
    printf("test_physics_bullet: physics update complete\n");

    // Test 5: SSBOs should be valid
    printf("test_physics_bullet: checking SSBOs\n");
    if (ctx->getParticleSSBO() == 0) {
        printf("FAIL - particle SSBO should be created\n");
        return 1;
    }
    if (ctx->getMicrobeSSBO() == 0) {
        printf("FAIL - microbe SSBO should be created\n");
        return 1;
    }
    printf("test_physics_bullet: SSBOs OK\n");

    // Cleanup
    printf("test_physics_bullet: cleaning up\n");
    fflush(stdout);
    ctx->destroy();
    CloseWindow();

    printf("PASS\n");
    return 0;
}
