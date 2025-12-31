#version 330

// Vertex shader for SDF raymarching
// Passes world position to fragment shader for proper ray calculation

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;  // Model matrix for world position

out vec2 fragTexCoord;
out vec3 fragNormal;
out vec4 fragColor;
out vec3 fragWorldPos;  // World space position

void main()
{
    // Calculate world position
    fragWorldPos = (matModel * vec4(vertexPosition, 1.0)).xyz;

    fragTexCoord = vertexTexCoord;
    fragNormal = vertexNormal;
    fragColor = vertexColor;

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
