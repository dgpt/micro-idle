#include "game/vk_compute.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define WORKGROUP_SIZE 256
#define GRID_W 128
#define GRID_H 128
#define GRID_CELLS (GRID_W * GRID_H)

struct VkComputeContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue compute_queue;
    uint32_t queue_family;

    VkBuffer entity_buffer;
    VkDeviceMemory entity_memory;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    VkBuffer head_buffer;
    VkDeviceMemory head_memory;
    VkBuffer next_buffer;
    VkDeviceMemory next_memory;

    VkDescriptorSetLayout desc_layout;
    VkDescriptorPool desc_pool;
    VkDescriptorSet desc_set;

    VkShaderModule insert_shader;
    VkShaderModule collide_shader;
    VkPipelineLayout pipeline_layout;
    VkPipeline insert_pipeline;
    VkPipeline collide_pipeline;

    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;
    VkFence fence;

    int entity_count;
    int active_count;
    size_t entity_buffer_size;
    float sim_time;
    bool ready;
};

static uint32_t find_memory_type(VkPhysicalDevice pdev, uint32_t type_filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(pdev, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool create_buffer(VkDevice device, VkPhysicalDevice pdev, VkDeviceSize size,
                          VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                          VkBuffer *buffer, VkDeviceMemory *memory) {
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(device, &buf_info, NULL, buffer) != VK_SUCCESS) return false;

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, *buffer, &mem_reqs);

    uint32_t mem_type = find_memory_type(pdev, mem_reqs.memoryTypeBits, props);
    if (mem_type == UINT32_MAX) return false;

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type
    };
    if (vkAllocateMemory(device, &alloc_info, NULL, memory) != VK_SUCCESS) return false;
    vkBindBufferMemory(device, *buffer, *memory, 0);
    return true;
}

// SPIR-V bytecode embedded from compiled shaders
#include "sim_insert.spv.h"
#include "sim_collide.spv.h"

static VkShaderModule create_shader_module(VkDevice device, const uint32_t *code, size_t size) {
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = code
    };
    VkShaderModule module;
    if (vkCreateShaderModule(device, &create_info, NULL, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

VkComputeContext *vk_compute_create(int entity_count) {
    VkComputeContext *ctx = calloc(1, sizeof(VkComputeContext));
    if (!ctx) return NULL;

    ctx->entity_count = entity_count;
    ctx->active_count = entity_count;
    ctx->entity_buffer_size = sizeof(VkSimData) * (size_t)entity_count;

    // Create instance
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "MicroIdle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "MicroEngine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2
    };

    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info
    };

    if (vkCreateInstance(&instance_info, NULL, &ctx->instance) != VK_SUCCESS) {
        fprintf(stderr, "vk_compute: failed to create instance\n");
        free(ctx);
        return NULL;
    }

    // Find physical device (prefer discrete GPU)
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, NULL);
    if (device_count == 0) {
        fprintf(stderr, "vk_compute: no Vulkan devices found\n");
        vkDestroyInstance(ctx->instance, NULL);
        free(ctx);
        return NULL;
    }

    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, devices);

    ctx->physical_device = devices[0];
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        fprintf(stderr, "vk_compute: found device [%u]: %s\n", i, props.deviceName);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            ctx->physical_device = devices[i];
        }
    }
    free(devices);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(ctx->physical_device, &props);
    fprintf(stderr, "vk_compute: using device: %s\n", props.deviceName);

    // Find compute queue family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->physical_device, &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->physical_device, &queue_family_count, queue_families);

    ctx->queue_family = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            ctx->queue_family = i;
            break;
        }
    }
    free(queue_families);

    if (ctx->queue_family == UINT32_MAX) {
        fprintf(stderr, "vk_compute: no compute queue found\n");
        vkDestroyInstance(ctx->instance, NULL);
        free(ctx);
        return NULL;
    }

    // Create logical device
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = ctx->queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info
    };

    if (vkCreateDevice(ctx->physical_device, &device_info, NULL, &ctx->device) != VK_SUCCESS) {
        fprintf(stderr, "vk_compute: failed to create device\n");
        vkDestroyInstance(ctx->instance, NULL);
        free(ctx);
        return NULL;
    }

    vkGetDeviceQueue(ctx->device, ctx->queue_family, 0, &ctx->compute_queue);

    // Create buffers
    if (!create_buffer(ctx->device, ctx->physical_device, ctx->entity_buffer_size,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       &ctx->entity_buffer, &ctx->entity_memory)) {
        fprintf(stderr, "vk_compute: failed to create entity buffer\n");
        goto cleanup;
    }

    if (!create_buffer(ctx->device, ctx->physical_device, ctx->entity_buffer_size,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &ctx->staging_buffer, &ctx->staging_memory)) {
        fprintf(stderr, "vk_compute: failed to create staging buffer\n");
        goto cleanup;
    }

    size_t grid_buffer_size = sizeof(int) * GRID_CELLS;
    if (!create_buffer(ctx->device, ctx->physical_device, grid_buffer_size,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       &ctx->head_buffer, &ctx->head_memory)) {
        fprintf(stderr, "vk_compute: failed to create head buffer\n");
        goto cleanup;
    }

    size_t next_buffer_size = sizeof(int) * (size_t)entity_count;
    if (!create_buffer(ctx->device, ctx->physical_device, next_buffer_size,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       &ctx->next_buffer, &ctx->next_memory)) {
        fprintf(stderr, "vk_compute: failed to create next buffer\n");
        goto cleanup;
    }

    // Initialize entity data
    VkSimData *staging_data;
    vkMapMemory(ctx->device, ctx->staging_memory, 0, ctx->entity_buffer_size, 0, (void**)&staging_data);

    static const float palette[6][3] = {
        {0.46f, 0.92f, 0.74f},
        {0.47f, 0.78f, 0.97f},
        {0.97f, 0.78f, 0.33f},
        {0.55f, 0.46f, 0.98f},
        {0.52f, 0.94f, 0.98f},
        {0.98f, 0.58f, 0.72f}
    };
    static const float base_radius[6] = {0.85f, 1.25f, 1.05f, 1.0f, 1.05f, 0.95f};
    static const float var_radius[6] = {0.35f, 0.3f, 0.35f, 0.3f, 0.4f, 0.25f};

    for (int i = 0; i < entity_count; i++) {
        uint32_t seed = (uint32_t)(i + 1) * 2654435761u;
        seed ^= seed >> 16;
        float r1 = (float)(seed & 0xFFFFu) / 65535.0f;
        seed = seed * 2246822519u + 3266489917u;
        float r2 = (float)(seed & 0xFFFFu) / 65535.0f;
        seed = seed * 3266489917u + 668265263u;
        float r3 = (float)(seed & 0xFFFFu) / 65535.0f;
        int type = (int)(seed % 6u);
        float px = ((float)(i % 1000) / 1000.0f - 0.5f) * 24.0f;
        float pz = ((float)(i / 1000) / 1000.0f - 0.5f) * 20.0f;

        staging_data[i].pos[0] = px;
        staging_data[i].pos[1] = 0.0f;
        staging_data[i].pos[2] = pz;
        staging_data[i].pos[3] = 1.0f;
        staging_data[i].vel[0] = (r1 - 0.5f) * 0.3f;
        staging_data[i].vel[1] = 0.0f;
        staging_data[i].vel[2] = (r2 - 0.5f) * 0.3f;
        staging_data[i].vel[3] = 0.0f;
        staging_data[i].color[0] = palette[type][0] + (r3 - 0.5f) * 0.08f;
        staging_data[i].color[1] = palette[type][1] + (r1 - 0.5f) * 0.08f;
        staging_data[i].color[2] = palette[type][2] + (r2 - 0.5f) * 0.08f;
        staging_data[i].color[3] = 0.62f + (r1 - 0.5f) * 0.1f;
        staging_data[i].params[0] = base_radius[type] + r3 * var_radius[type];
        staging_data[i].params[1] = 0.0f;
        staging_data[i].params[2] = (float)type;
        staging_data[i].params[3] = r2;
    }
    vkUnmapMemory(ctx->device, ctx->staging_memory);

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding bindings[3] = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT}
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings = bindings
    };

    if (vkCreateDescriptorSetLayout(ctx->device, &layout_info, NULL, &ctx->desc_layout) != VK_SUCCESS) {
        fprintf(stderr, "vk_compute: failed to create descriptor layout\n");
        goto cleanup;
    }

    // Create descriptor pool and set
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 3
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size
    };

    if (vkCreateDescriptorPool(ctx->device, &pool_info, NULL, &ctx->desc_pool) != VK_SUCCESS) {
        fprintf(stderr, "vk_compute: failed to create descriptor pool\n");
        goto cleanup;
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx->desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &ctx->desc_layout
    };

    if (vkAllocateDescriptorSets(ctx->device, &alloc_info, &ctx->desc_set) != VK_SUCCESS) {
        fprintf(stderr, "vk_compute: failed to allocate descriptor set\n");
        goto cleanup;
    }

    // Update descriptor set
    VkDescriptorBufferInfo buffer_infos[3] = {
        {.buffer = ctx->entity_buffer, .offset = 0, .range = ctx->entity_buffer_size},
        {.buffer = ctx->head_buffer, .offset = 0, .range = grid_buffer_size},
        {.buffer = ctx->next_buffer, .offset = 0, .range = next_buffer_size}
    };

    VkWriteDescriptorSet writes[3] = {
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ctx->desc_set, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffer_infos[0]},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ctx->desc_set, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffer_infos[1]},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ctx->desc_set, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffer_infos[2]}
    };

    vkUpdateDescriptorSets(ctx->device, 3, writes, 0, NULL);

    // Create shader modules
    ctx->insert_shader = create_shader_module(ctx->device, (const uint32_t*)sim_insert_spv, sim_insert_spv_len);
    ctx->collide_shader = create_shader_module(ctx->device, (const uint32_t*)sim_collide_spv, sim_collide_spv_len);

    if (!ctx->insert_shader || !ctx->collide_shader) {
        fprintf(stderr, "vk_compute: failed to create shader modules\n");
        goto cleanup;
    }

    // Create pipeline layout with push constants
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = 32  // dt, bounds_x, bounds_y, cell, grid_w, grid_h, active, pad
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx->desc_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range
    };

    if (vkCreatePipelineLayout(ctx->device, &pipeline_layout_info, NULL, &ctx->pipeline_layout) != VK_SUCCESS) {
        fprintf(stderr, "vk_compute: failed to create pipeline layout\n");
        goto cleanup;
    }

    // Create compute pipelines
    VkComputePipelineCreateInfo pipeline_infos[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = ctx->insert_shader,
                .pName = "main"
            },
            .layout = ctx->pipeline_layout
        },
        {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = ctx->collide_shader,
                .pName = "main"
            },
            .layout = ctx->pipeline_layout
        }
    };

    VkPipeline pipelines[2];
    if (vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 2, pipeline_infos, NULL, pipelines) != VK_SUCCESS) {
        fprintf(stderr, "vk_compute: failed to create compute pipelines\n");
        goto cleanup;
    }
    ctx->insert_pipeline = pipelines[0];
    ctx->collide_pipeline = pipelines[1];

    // Create command pool and buffer
    VkCommandPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx->queue_family
    };

    if (vkCreateCommandPool(ctx->device, &pool_create_info, NULL, &ctx->cmd_pool) != VK_SUCCESS) {
        fprintf(stderr, "vk_compute: failed to create command pool\n");
        goto cleanup;
    }

    VkCommandBufferAllocateInfo cmd_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    if (vkAllocateCommandBuffers(ctx->device, &cmd_alloc_info, &ctx->cmd_buffer) != VK_SUCCESS) {
        fprintf(stderr, "vk_compute: failed to allocate command buffer\n");
        goto cleanup;
    }

    // Create fence
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    if (vkCreateFence(ctx->device, &fence_info, NULL, &ctx->fence) != VK_SUCCESS) {
        fprintf(stderr, "vk_compute: failed to create fence\n");
        goto cleanup;
    }

    // Copy initial data to GPU
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkWaitForFences(ctx->device, 1, &ctx->fence, VK_TRUE, UINT64_MAX);
    vkResetFences(ctx->device, 1, &ctx->fence);
    vkResetCommandBuffer(ctx->cmd_buffer, 0);
    vkBeginCommandBuffer(ctx->cmd_buffer, &begin_info);

    VkBufferCopy copy_region = {.size = ctx->entity_buffer_size};
    vkCmdCopyBuffer(ctx->cmd_buffer, ctx->staging_buffer, ctx->entity_buffer, 1, &copy_region);

    vkEndCommandBuffer(ctx->cmd_buffer);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &ctx->cmd_buffer
    };

    vkQueueSubmit(ctx->compute_queue, 1, &submit_info, ctx->fence);
    vkWaitForFences(ctx->device, 1, &ctx->fence, VK_TRUE, UINT64_MAX);

    ctx->ready = true;
    fprintf(stderr, "vk_compute: initialized successfully with %d entities\n", entity_count);
    return ctx;

cleanup:
    vk_compute_destroy(ctx);
    return NULL;
}

void vk_compute_destroy(VkComputeContext *ctx) {
    if (!ctx) return;

    if (ctx->device) {
        vkDeviceWaitIdle(ctx->device);

        if (ctx->fence) vkDestroyFence(ctx->device, ctx->fence, NULL);
        if (ctx->cmd_pool) vkDestroyCommandPool(ctx->device, ctx->cmd_pool, NULL);
        if (ctx->insert_pipeline) vkDestroyPipeline(ctx->device, ctx->insert_pipeline, NULL);
        if (ctx->collide_pipeline) vkDestroyPipeline(ctx->device, ctx->collide_pipeline, NULL);
        if (ctx->pipeline_layout) vkDestroyPipelineLayout(ctx->device, ctx->pipeline_layout, NULL);
        if (ctx->insert_shader) vkDestroyShaderModule(ctx->device, ctx->insert_shader, NULL);
        if (ctx->collide_shader) vkDestroyShaderModule(ctx->device, ctx->collide_shader, NULL);
        if (ctx->desc_pool) vkDestroyDescriptorPool(ctx->device, ctx->desc_pool, NULL);
        if (ctx->desc_layout) vkDestroyDescriptorSetLayout(ctx->device, ctx->desc_layout, NULL);

        if (ctx->entity_buffer) vkDestroyBuffer(ctx->device, ctx->entity_buffer, NULL);
        if (ctx->entity_memory) vkFreeMemory(ctx->device, ctx->entity_memory, NULL);
        if (ctx->staging_buffer) vkDestroyBuffer(ctx->device, ctx->staging_buffer, NULL);
        if (ctx->staging_memory) vkFreeMemory(ctx->device, ctx->staging_memory, NULL);
        if (ctx->head_buffer) vkDestroyBuffer(ctx->device, ctx->head_buffer, NULL);
        if (ctx->head_memory) vkFreeMemory(ctx->device, ctx->head_memory, NULL);
        if (ctx->next_buffer) vkDestroyBuffer(ctx->device, ctx->next_buffer, NULL);
        if (ctx->next_memory) vkFreeMemory(ctx->device, ctx->next_memory, NULL);

        vkDestroyDevice(ctx->device, NULL);
    }

    if (ctx->instance) vkDestroyInstance(ctx->instance, NULL);
    free(ctx);
}

bool vk_compute_ready(const VkComputeContext *ctx) {
    return ctx && ctx->ready;
}

void vk_compute_update(VkComputeContext *ctx, float dt, float bounds_x, float bounds_y, int active_count) {
    if (!ctx || !ctx->ready) return;

    ctx->active_count = active_count;
    ctx->sim_time += dt;
    float cell = (bounds_x * 2.0f) / (float)GRID_W;

    struct {
        float dt;
        float bounds_x;
        float bounds_y;
        float cell;
        int grid_w;
        int grid_h;
        int active;
        float time;
    } push_data = {
        .dt = dt,
        .bounds_x = bounds_x,
        .bounds_y = bounds_y,
        .cell = cell,
        .grid_w = GRID_W,
        .grid_h = GRID_H,
        .active = active_count,
        .time = ctx->sim_time
    };

    vkWaitForFences(ctx->device, 1, &ctx->fence, VK_TRUE, UINT64_MAX);
    vkResetFences(ctx->device, 1, &ctx->fence);
    vkResetCommandBuffer(ctx->cmd_buffer, 0);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(ctx->cmd_buffer, &begin_info);

    // Clear head buffer
    vkCmdFillBuffer(ctx->cmd_buffer, ctx->head_buffer, 0, sizeof(int) * GRID_CELLS, 0xFFFFFFFF);

    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
    };
    vkCmdPipelineBarrier(ctx->cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);

    vkCmdBindDescriptorSets(ctx->cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->pipeline_layout, 0, 1, &ctx->desc_set, 0, NULL);
    vkCmdPushConstants(ctx->cmd_buffer, ctx->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_data), &push_data);

    uint32_t groups = (active_count + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;

    // Insert pass
    vkCmdBindPipeline(ctx->cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->insert_pipeline);
    vkCmdDispatch(ctx->cmd_buffer, groups, 1, 1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(ctx->cmd_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);

    // Collide pass
    vkCmdBindPipeline(ctx->cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->collide_pipeline);
    vkCmdDispatch(ctx->cmd_buffer, groups, 1, 1);

    vkEndCommandBuffer(ctx->cmd_buffer);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &ctx->cmd_buffer
    };
    vkQueueSubmit(ctx->compute_queue, 1, &submit_info, ctx->fence);
}

void vk_compute_read_entities(VkComputeContext *ctx, VkSimData *out, int count) {
    if (!ctx || !ctx->ready || !out) return;

    vkWaitForFences(ctx->device, 1, &ctx->fence, VK_TRUE, UINT64_MAX);
    vkResetFences(ctx->device, 1, &ctx->fence);
    vkResetCommandBuffer(ctx->cmd_buffer, 0);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(ctx->cmd_buffer, &begin_info);

    size_t copy_size = sizeof(VkSimData) * (size_t)(count < ctx->entity_count ? count : ctx->entity_count);
    VkBufferCopy copy_region = {.size = copy_size};
    vkCmdCopyBuffer(ctx->cmd_buffer, ctx->entity_buffer, ctx->staging_buffer, 1, &copy_region);

    vkEndCommandBuffer(ctx->cmd_buffer);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &ctx->cmd_buffer
    };
    vkQueueSubmit(ctx->compute_queue, 1, &submit_info, ctx->fence);
    vkWaitForFences(ctx->device, 1, &ctx->fence, VK_TRUE, UINT64_MAX);

    void *mapped;
    vkMapMemory(ctx->device, ctx->staging_memory, 0, copy_size, 0, &mapped);
    memcpy(out, mapped, copy_size);
    vkUnmapMemory(ctx->device, ctx->staging_memory);
}

void vk_compute_set_active_count(VkComputeContext *ctx, int count) {
    if (ctx) ctx->active_count = count;
}

int vk_compute_get_active_count(const VkComputeContext *ctx) {
    return ctx ? ctx->active_count : 0;
}
