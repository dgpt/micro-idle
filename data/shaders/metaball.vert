#version 330

// Metaball Billboard Vertex Shader
// Renders particle positions as camera-facing billboards

// Raylib standard vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;

// Raylib standard uniforms
uniform mat4 mvp;

// Custom uniforms (set per particle)
uniform vec3 cameraRight;      // Camera right vector
uniform vec3 cameraUp;         // Camera up vector
uniform float instanceRadius;  // Particle radius
uniform vec4 instanceColor;    // Particle color

// Outputs to fragment shader
out vec2 fragTexCoord;
out float fragParticleRadius;
out vec4 fragColor;

void main() {
    // Billboard the quad to face the camera
    // vertexPosition.xy is the quad corner offset (-1 to 1)
    vec3 offset = cameraRight * vertexPosition.x * instanceRadius
                + cameraUp * vertexPosition.y * instanceRadius;

    // World position is model matrix position + billboard offset
    vec3 worldPos = vec3(0, 0, 0) + offset;

    gl_Position = mvp * vec4(worldPos, 1.0);

    // Pass data to fragment shader
    fragTexCoord = vertexTexCoord;
    fragParticleRadius = instanceRadius;
    fragColor = instanceColor;
}
