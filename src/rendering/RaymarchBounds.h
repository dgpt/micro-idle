#ifndef MICRO_IDLE_RAYMARCH_BOUNDS_H
#define MICRO_IDLE_RAYMARCH_BOUNDS_H

#include "raylib.h"

namespace micro_idle {
namespace rendering {

// Raymarching bounds calculation utilities
// Computes bounding volumes for SDF raymarching based on microbe properties

// Calculate bounding sphere radius for a microbe
// The bounding sphere should encompass the entire soft body including deformation
// multiplier: scaling factor for safety margin (default 2.5f)
float calculateBoundRadius(float baseRadius, float multiplier = 2.5f);

// Calculate bounding sphere radius accounting for soft body deformation
// vertexCount: number of vertices in soft body
// baseRadius: base microbe radius
// maxDeformation: maximum expected deformation factor (default 1.5f)
float calculateBoundRadiusWithDeformation(float baseRadius, int vertexCount, float maxDeformation = 1.5f);

} // namespace rendering
} // namespace micro_idle

#endif
