#ifndef MICRO_IDLE_SDF_SHADER_H
#define MICRO_IDLE_SDF_SHADER_H

#include "raylib.h"

namespace micro_idle {
namespace rendering {

// SDF shader management utilities
// Handles loading, caching, and uniform location management for SDF raymarching shaders

struct SDFShaderUniforms {
    int viewPos{-1};
    int pointCount{-1};
    int baseRadius{-1};
    int microbeColor{-1};
    int skeletonPoints[64];  // Array of uniform locations for skeleton points
};

// Load SDF membrane shader from standard paths
// Returns shader with id=0 on failure
Shader loadSDFMembraneShader();

// Initialize uniform locations for an SDF shader
// Returns true if shader is valid and uniforms were found
bool initializeSDFUniforms(Shader shader, SDFShaderUniforms& uniforms);

// Set camera position uniform (called each frame)
void setCameraPosition(Shader shader, const SDFShaderUniforms& uniforms, Vector3 cameraPos);

// Set per-microbe uniforms (called for each microbe)
void setMicrobeUniforms(Shader shader, const SDFShaderUniforms& uniforms,
                        int vertexCount, float baseRadius, Color microbeColor);

// Set vertex positions uniform array (called for each microbe)
void setVertexPositions(Shader shader, const SDFShaderUniforms& uniforms,
                        const Vector3* positions, int count);

} // namespace rendering
} // namespace micro_idle

#endif
