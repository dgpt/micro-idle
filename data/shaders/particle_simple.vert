#version 430 core

// Simple particle rendering - direct circles, no metaballs

layout(location = 0) in vec2 aCorner;  // Billboard corner (-1,-1) to (1,1)

struct Particle {
    vec4 pos;       // xyz position, w = inverse mass
    vec4 pos_prev;
    vec4 vel;       // xyz velocity, w = microbe_id
    vec4 data;      // x = particle_index, y = constraint_start, z = constraint_count, w = type (0=skeleton, 1=membrane)
};

struct Microbe {
    vec4 center;    // xyz center, w = radius
    vec4 color;
    vec4 params;    // x = type, y = stiffness, z = seed, w = squish
    vec4 aabb;
};

layout(std430, binding = 0) readonly buffer Particles { Particle particles[]; };
layout(std430, binding = 1) readonly buffer Microbes { Microbe microbes[]; };

uniform mat4 u_vp;

out vec2 vUV;
out vec4 vColor;

void main() {
    int particle_id = gl_InstanceID;
    Particle p = particles[particle_id];

    vec3 particlePos = p.pos.xyz;
    float microbe_id = p.vel.w;
    int m_id = int(microbe_id + 0.5);
    Microbe m = microbes[m_id];

    // Render ALL particles - skeleton fills interior, membrane creates outline
    bool is_membrane = (p.data.w > 0.5);
    float particleSize = is_membrane ? 1.2 : 0.9;

    // Create billboard in world space
    vec3 right = vec3(1, 0, 0);
    vec3 forward = vec3(0, 0, 1);
    vec3 worldPos = particlePos + (right * aCorner.x + forward * aCorner.y) * particleSize;

    gl_Position = u_vp * vec4(worldPos, 1.0);
    vUV = aCorner;  // -1 to 1
    vColor = m.color;
}
