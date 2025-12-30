#include "RaymarchBounds.h"
#include <cmath>

namespace micro_idle {
namespace rendering {

float calculateBoundRadius(float baseRadius, float multiplier) {
    return baseRadius * multiplier;
}

float calculateBoundRadiusWithDeformation(float baseRadius, int vertexCount, float maxDeformation) {
    // Base radius with deformation margin
    float deformedRadius = baseRadius * maxDeformation;

    // Additional margin for vertex spread (more vertices = more potential spread)
    // This is a conservative estimate
    float vertexSpreadFactor = 1.0f + (vertexCount / 100.0f);

    return deformedRadius * vertexSpreadFactor;
}

} // namespace rendering
} // namespace micro_idle
