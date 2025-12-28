#include <stdio.h>
#include "tests/test_env.h"

int test_time_run(void);
int test_rng_run(void);
int test_engine_run(void);
int test_game_constants_run(void);
int test_physics_bullet_run(void);
int test_visual_run(void);
int test_microbe_positions_run(void);
int test_deflation_run(void);

int main(void) {
    test_set_env("MICRO_IDLE_ALLOW_SOFT", "1");
    int fails = 0;

    // Core engine tests (should always pass)
    fails += test_time_run();
    fails += test_rng_run();
    fails += test_engine_run();
    fails += test_game_constants_run();

    // Visual test runs first (initializes OpenGL context needed for physics tests)
    printf("\n--- Visual Tests ---\n");
    fails += test_visual_run();

    // New Bullet physics tests (requires OpenGL context from visual test)
    printf("\n--- Bullet Physics Tests (expected to partially fail until Phase 2.4+) ---\n");
    fails += test_physics_bullet_run();

    printf("\n--- Microbe Position Tests ---\n");
    fails += test_microbe_positions_run();

    printf("\n--- Deflation Test ---\n");
    fails += test_deflation_run();

    if (fails != 0) {
        printf("\nFAIL %d test(s)\n", fails);
        return 1;
    }
    printf("\nOK - All tests passed\n");
    return 0;
}
