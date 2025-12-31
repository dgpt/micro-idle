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

// SDF rendering component - stores shader uniform data for raymarching
struct SDFRenderComponent {
    Shader shader{0};                    // SDF shader (lazy loaded)
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
