#ifndef MICRO_IDLE_TRANSFORM_H
#define MICRO_IDLE_TRANSFORM_H

#include "raymath.h"

namespace components {

struct Transform {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vector3 scale{1.0f, 1.0f, 1.0f};
};

} // namespace components

#endif
