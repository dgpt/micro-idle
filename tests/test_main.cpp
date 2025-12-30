#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include "tests/test_env.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <dirent.h>
#include <unistd.h>
#endif

// Cross-platform function to delete all PNG files in screenshots directory
static void clearScreenshots() {
#ifdef _WIN32
    // Windows: Use FindFirstFile/FindNextFile for reliable deletion
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile("screenshots\\*.png", &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            char filepath[256];
            snprintf(filepath, sizeof(filepath), "screenshots\\%s", findData.cFileName);
            DeleteFile(filepath);
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }
#else
    // Unix: Use opendir/readdir
    DIR* dir = opendir("screenshots");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".png") != NULL) {
                char filepath[256];
                snprintf(filepath, sizeof(filepath), "screenshots/%s", entry->d_name);
                unlink(filepath);
            }
        }
        closedir(dir);
    }
#endif
}

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
int test_game_render_run(void);

int main(void) {
    test_set_env("MICRO_IDLE_ALLOW_SOFT", "1");

    // Clear all screenshots at the start of test run
    printf("Clearing existing screenshots...\n");
    mkdir("screenshots", 0755);
    clearScreenshots();
    printf("Screenshots cleared.\n\n");

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

    // Game render test (reproduces game.exe initialization)
    printf("\n--- Game Render Tests ---\n");
    fails += test_game_render_run();

    if (fails != 0) {
        printf("\nFAIL %d test(s)\n", fails);
        return 1;
    }
    printf("\nOK - All tests passed\n");
    return 0;
}
