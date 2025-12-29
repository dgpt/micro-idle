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

} // namespace components

#endif
