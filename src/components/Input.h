#ifndef MICRO_IDLE_INPUT_H
#define MICRO_IDLE_INPUT_H

#include "raylib.h"

namespace components {

// Singleton component for input state
struct InputState {
    Vector2 mousePosition{0.0f, 0.0f};
    Vector2 mouseDelta{0.0f, 0.0f};
    bool mouseLeftDown{false};
    bool mouseRightDown{false};
    float mouseWheel{0.0f};
};

} // namespace components

#endif
