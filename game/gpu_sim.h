#ifndef MICRO_IDLE_GPU_SIM_H
#define MICRO_IDLE_GPU_SIM_H

#include <stdbool.h>
#include "raylib.h"

typedef struct GpuSim {
    bool ready;
    unsigned int sim_insert_program;
    unsigned int sim_collide_program;
    Shader render_shader;
    unsigned int ssbo;
    unsigned int ssbo_head;
    unsigned int ssbo_next;
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;
    int entity_count;
    int active_count;
    int indices_count;
    int loc_vp;
    int loc_bounds;
    int loc_dt;
    int loc_grid_dim;
    int loc_cell;
    int loc_active_insert;
    int loc_active_collide;
    int loc_time;
    int loc_time_collide;
    float sim_time;
} GpuSim;

bool gpu_sim_init(GpuSim *sim, int entity_count);
void gpu_sim_shutdown(GpuSim *sim);
void gpu_sim_update(GpuSim *sim, float dt, Vector2 bounds);
void gpu_sim_render(const GpuSim *sim, Camera3D camera);
bool gpu_sim_supported(void);
void gpu_sim_set_active_count(GpuSim *sim, int active_count);
void gpu_sim_upload_entities(GpuSim *sim, const void *data, int count);
#ifdef GPU_SIM_TESTING
void gpu_sim_test_set_fail_mode(int mode);
#endif

#endif
