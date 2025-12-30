#include "InputSystem.h"
#include "src/components/Input.h"
#include "raylib.h"

namespace micro_idle {

void InputSystem::registerSystem(flecs::world& world) {
    // System that updates InputState singleton from Raylib
    // Runs in OnUpdate phase (first phase, before simulation)
    // Use a system without component query since we're updating a singleton
    world.system("InputSystem")
        .kind(flecs::OnUpdate)
        .run([](flecs::iter& it) {
            // Get or create InputState singleton
            auto input = it.world().get_mut<components::InputState>();
            if (input) {
                // Poll Raylib input and update FLECS component
                input->mousePosition = GetMousePosition();
                input->mouseDelta = GetMouseDelta();
                input->mouseLeftDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
                input->mouseLeftPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
                input->mouseRightDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
                input->mouseRightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
                input->mouseWheel = GetMouseWheelMove();
            }
        });
}

} // namespace micro_idle
