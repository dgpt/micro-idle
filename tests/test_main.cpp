#include <stdio.h>
#include "tests/test_env.h"

int test_time_run(void);
int test_rng_run(void);
int test_engine_run(void);
int test_game_constants_run(void);
int test_visual_run(void);
int test_icosphere_run(void);
int test_constraints_run(void);
int test_softbody_factory_run(void);
int test_ecm_locomotion_run(void);
int test_microbe_integration_run(void);

int main(void) {
    test_set_env("MICRO_IDLE_ALLOW_SOFT", "1");
    int fails = 0;

    // Core engine tests (should always pass)
    printf("\n--- Core Engine Tests ---\n");
    fails += test_time_run();
    fails += test_rng_run();
    fails += test_engine_run();
    fails += test_game_constants_run();

    // Physics tests
    printf("\n--- Physics Tests ---\n");
    fails += test_icosphere_run();
    fails += test_constraints_run();

    // Soft body tests (Puppet architecture)
    printf("\n--- Soft Body Tests ---\n");
    fails += test_softbody_factory_run();
    fails += test_ecm_locomotion_run();
    fails += test_microbe_integration_run();

    // Visual test (headless screenshot output)
    printf("\n--- Visual Tests (Headless) ---\n");
    fails += test_visual_run();

    if (fails != 0) {
        printf("\nFAIL %d test(s)\n", fails);
        return 1;
    }
    printf("\nOK - All tests passed\n");
    return 0;
}
