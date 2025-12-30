#ifndef MICRO_IDLE_RENDERING_H
#define MICRO_IDLE_RENDERING_H

#include "raylib.h"

namespace components {

struct RenderMesh {
    Mesh mesh;
    bool ownsData{false};
};

struct RenderColor {
    Color color{WHITE};
};

struct RenderSphere {
    float radius{1.0f};
};

// SDF rendering component - stores shader uniform data for raymarching
struct SDFRenderComponent {
    Shader shader{0};                    // SDF shader (lazy loaded)
    int shaderLocViewPos{-1};            // Uniform location for camera position
    int shaderLocPointCount{-1};         // Uniform location for point count
    int shaderLocBaseRadius{-1};         // Uniform location for base radius
    int shaderLocMicrobeColor{-1};       // Uniform location for microbe color
    int shaderLocSkeletonPoints[64]{-1}; // Uniform locations for skeleton points array
    bool shaderLoaded{false};            // Whether shader has been loaded
    Vector3 vertexPositions[64];         // Cached vertex positions (updated each frame)
    int vertexCount{0};                   // Number of vertices
};

// Camera singleton - stores current camera state for rendering systems
struct CameraState {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 target{0.0f, 0.0f, 0.0f};
    Vector3 up{0.0f, 1.0f, 0.0f};
    float fovy{50.0f};
};

} // namespace components

#endif
