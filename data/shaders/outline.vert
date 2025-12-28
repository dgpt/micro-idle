#version 430 core

// Outline rendering - smooth boundary curve around particles

layout(location = 0) in vec2 aPosition;  // 2D position in XZ plane

uniform mat4 u_vp;
uniform vec4 u_color;

out vec4 vColor;

void main() {
    // Convert 2D position to 3D (Y=0 for top-down view)
    vec3 worldPos = vec3(aPosition.x, 0.0, aPosition.y);
    gl_Position = u_vp * vec4(worldPos, 1.0);
    vColor = u_color;
}
