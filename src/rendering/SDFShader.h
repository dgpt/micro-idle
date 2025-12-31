#ifndef MICRO_IDLE_SDF_SHADER_H
#define MICRO_IDLE_SDF_SHADER_H

#include "raylib.h"

namespace micro_idle {
namespace rendering {

// SDF shader management utilities
// Handles loading, caching, and uniform location management for SDF raymarching shaders

struct SDFShaderUniforms {
    int viewPos{-1};
    int time{-1};
    int pointCount{-1};
    int baseRadius{-1};
    int microbeColor{-1};
    int skeletonPoints{-1};  // Base uniform location for skeletonPoints[0]
    int podDirs{-1};
    int podExtents{-1};
    int podAnchors{-1};
    int podCount{-1};
};

// Load SDF membrane shader from standard paths
// Returns shader with id=0 on failure
Shader loadSDFMembraneShader();

// Initialize uniform locations for an SDF shader
// Returns true if shader is valid and uniforms were found
bool initializeSDFUniforms(Shader shader, SDFShaderUniforms& uniforms);

// Set camera position uniform (called each frame)
void setCameraPosition(Shader shader, const SDFShaderUniforms& uniforms, Vector3 cameraPos);

// Set time uniform (called each frame)
void setTime(Shader shader, const SDFShaderUniforms& uniforms, float time);

// Set per-microbe uniforms (called for each microbe)
void setMicrobeUniforms(Shader shader, const SDFShaderUniforms& uniforms,
                        int vertexCount, float baseRadius, Color microbeColor);

// Set vertex positions uniform array (called for each microbe)
void setVertexPositions(Shader shader, const SDFShaderUniforms& uniforms,
                        const Vector3* positions, int count);

// Set pseudopod direction + extent arrays (optional)
void setPodData(Shader shader, const SDFShaderUniforms& uniforms,
                const Vector3* podDirs, const float* podExtents,
                const Vector3* podAnchors, int podCount);

} // namespace rendering
} // namespace micro_idle

#endif
