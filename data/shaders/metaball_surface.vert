#version 430 core

// Metaball Surface Pass - Vertex Shader
// Full-screen quad to sample field texture and render final surface

layout(location = 0) in vec2 aPos;  // Quad vertices

out vec2 vTexCoord;

void main() {
    vTexCoord = aPos * 0.5 + 0.5;  // Convert from [-1,1] to [0,1]
    gl_Position = vec4(aPos, 0.0, 1.0);
}
