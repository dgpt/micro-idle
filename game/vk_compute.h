#ifndef MICRO_IDLE_VK_COMPUTE_H
#define MICRO_IDLE_VK_COMPUTE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct VkComputeContext VkComputeContext;

typedef struct VkSimData {
    float pos[4];
    float vel[4];
    float color[4];
    float params[4];
} VkSimData;

VkComputeContext *vk_compute_create(int entity_count);
void vk_compute_destroy(VkComputeContext *ctx);
bool vk_compute_ready(const VkComputeContext *ctx);
void vk_compute_update(VkComputeContext *ctx, float dt, float bounds_x, float bounds_y, int active_count);
void vk_compute_read_entities(VkComputeContext *ctx, VkSimData *out, int count);
void vk_compute_set_active_count(VkComputeContext *ctx, int count);
int vk_compute_get_active_count(const VkComputeContext *ctx);

#endif
