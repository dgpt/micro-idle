#include <stdio.h>

int test_time_run(void);
int test_rng_run(void);
int test_engine_run(void);
int test_game_constants_run(void);
int test_gpu_sim_run(void);
int test_game_run(void);
int test_render_output_run(void);

int main(void) {
    int fails = 0;
    fails += test_time_run();
    fails += test_rng_run();
    fails += test_engine_run();
    fails += test_game_constants_run();
    fails += test_gpu_sim_run();
    fails += test_game_run();
    fails += test_render_output_run();
    if (fails != 0) {
        printf("FAIL %d\n", fails);
        return 1;
    }
    printf("OK\n");
    return 0;
}
