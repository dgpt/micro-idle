#ifndef XPBD_H
#define XPBD_H

#include <stdbool.h>
#include <stdint.h>
#include "raylib.h"

// XPBD Soft-Body Physics System
// Each microbe is represented as a cluster of particles connected by distance constraints.
// The XPBD solver maintains shape while allowing natural deformation on collision.

#define XPBD_PARTICLES_PER_MICROBE 32  // Ring of particles forming membrane (higher resolution)
#define XPBD_CONSTRAINTS_PER_MICROBE 96 // Distance constraints (ring + cross + diagonals)
#define XPBD_SOLVER_ITERATIONS 6        // Balanced for smooth motion

// Particle data (GPU layout, 48 bytes, padded to 64)
typedef struct {
    float pos[4];      // xyz position, w = inverse mass
    float pos_prev[4]; // previous position for velocity derivation
    float vel[4];      // xyz velocity, w = microbe_id (as float)
    float data[4];     // x = particle_index_in_microbe, y = constraint_start, z = constraint_count, w = padding
} XpbdParticle;

// Distance constraint (GPU layout, 32 bytes)
typedef struct {
    int32_t p1;           // particle index 1
    int32_t p2;           // particle index 2
    float rest_length;    // target distance
    float compliance;     // alpha = 1/stiffness (0 = infinitely stiff)
    float lambda;         // accumulated Lagrange multiplier
    float padding[3];
} XpbdConstraint;

// Microbe metadata (GPU layout, 64 bytes)
typedef struct {
    float center[4];      // xyz center of mass, w = radius
    float color[4];       // rgba
    float params[4];      // x = type, y = stiffness, z = seed, w = squish_amount
    float aabb[4];        // bounding box for broad-phase (min_x, min_z, max_x, max_z)
} XpbdMicrobe;

// Main XPBD context
typedef struct XpbdContext XpbdContext;

// Create/destroy
XpbdContext *xpbd_create(int max_microbes);
void xpbd_destroy(XpbdContext *ctx);

// Initialization
void xpbd_spawn_microbe(XpbdContext *ctx, float x, float z, int type, float seed);
void xpbd_clear(XpbdContext *ctx);

// Simulation
void xpbd_update(XpbdContext *ctx, float dt, float bounds_x, float bounds_y, float cursor_x, float cursor_z);

// Rendering
void xpbd_render(const XpbdContext *ctx, Camera3D camera);

// Rendering access
unsigned int xpbd_get_particle_ssbo(const XpbdContext *ctx);
unsigned int xpbd_get_microbe_ssbo(const XpbdContext *ctx);
int xpbd_get_microbe_count(const XpbdContext *ctx);
int xpbd_get_particle_count(const XpbdContext *ctx);

// Debug
void xpbd_debug_print(const XpbdContext *ctx);

#endif // XPBD_H
