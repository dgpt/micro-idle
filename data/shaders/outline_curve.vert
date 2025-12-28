#version 430 core

layout(location = 0) in vec2 aPosition;  // Position on curve segment (0-1)
layout(location = 1) in int aPointIndex; // Which control point we're at

struct Microbe {
    vec4 center;
    vec4 color;
    vec4 params;
    vec4 aabb;
};

layout(std430, binding = 0) readonly buffer OutlinePoints {
    vec4 points[];  // Sorted membrane particle positions (xyz, w=unused)
};

layout(std430, binding = 1) readonly buffer Microbes {
    Microbe microbes[];
};

uniform mat4 u_vp;
uniform int u_point_count;
uniform int u_microbe_index;
uniform float u_offset_radius;  // How far to push curve outward

out vec4 vColor;

// Catmull-Rom interpolation
vec3 catmullRom(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    return 0.5 * (
        (2.0 * p1) +
        (-p0 + p2) * t +
        (2.0*p0 - 5.0*p1 + 4.0*p2 - p3) * t2 +
        (-p0 + 3.0*p1 - 3.0*p2 + p3) * t3
    );
}

void main() {
    // Get 4 control points for Catmull-Rom (p0, p1, p2, p3)
    int i = aPointIndex;
    int i0 = (i - 1 + u_point_count) % u_point_count;
    int i1 = i;
    int i2 = (i + 1) % u_point_count;
    int i3 = (i + 2) % u_point_count;

    vec3 p0 = points[i0].xyz;
    vec3 p1 = points[i1].xyz;
    vec3 p2 = points[i2].xyz;
    vec3 p3 = points[i3].xyz;

    // Interpolate position on curve
    vec3 curvePos = catmullRom(p0, p1, p2, p3, aPosition.x);

    // Calculate tangent for offset direction
    float epsilon = 0.01;
    vec3 tangent = normalize(catmullRom(p0, p1, p2, p3, aPosition.x + epsilon) - curvePos);

    // Normal points outward (perpendicular to tangent, in XZ plane)
    vec3 normal = normalize(vec3(-tangent.z, 0, tangent.x));

    // Offset outward to wrap around spheres
    vec3 offsetPos = curvePos + normal * u_offset_radius;
    offsetPos.y = 0.0;  // Keep at Y=0

    gl_Position = u_vp * vec4(offsetPos, 1.0);
    vColor = microbes[u_microbe_index].color;
}
