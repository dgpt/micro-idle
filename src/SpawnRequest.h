#ifndef MICRO_IDLE_SPAWN_REQUEST_H
#define MICRO_IDLE_SPAWN_REQUEST_H

#include "raylib.h"

namespace micro_idle {

// Spawn request structure for deferred spawning
struct SpawnRequest {
    Vector3 position;
    float radius;
    Color color;
};

} // namespace micro_idle

#endif // MICRO_IDLE_SPAWN_REQUEST_H
