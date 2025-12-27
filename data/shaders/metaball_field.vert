#version 430 core

// Metaball Field Pass - Vertex Shader
// Renders each particle as a billboard contributing to the metaball field

layout(location = 0) in vec2 aCorner;  // Billboard corner (-1,-1) to (1,1)

// Particle data from XPBD simulation
struct Particle {
    vec4 pos;       // xyz position, w = inverse mass
    vec4 pos_prev;
    vec4 vel;       // xyz velocity, w = microbe_id
    vec4 data;
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
uniform int u_particles_per_microbe;
uniform float u_particle_radius;  // Influence radius for each particle

out vec2 vBillboardUV;
out float vMicrobeID;
out vec3 vParticleWorldPos;
out float vInfluenceRadius;

void main() {
    int particle_id = gl_InstanceID;
    Particle p = particles[particle_id];

    vec3 particlePos = p.pos.xyz;
    float microbe_id = p.vel.w;
    int m_id = int(microbe_id + 0.5);

    // Get microbe data for per-type radius
    Microbe m = microbes[m_id];
    float base_radius = m.center.w;

    // Billboard size scaled to particle spacing for smooth blending
    float billboardSize = 0.5;  // Radius for good overlap between particles

    // Create billboard in world space (aligned to XZ plane for top-down view)
    // Y=0 plane, billboards expand in X and Z
    vec3 right = vec3(1, 0, 0);
    vec3 forward = vec3(0, 0, 1);

    vec3 worldPos = particlePos + (right * aCorner.x + forward * aCorner.y) * billboardSize;

    gl_Position = u_vp * vec4(worldPos, 1.0);

    vBillboardUV = aCorner;  // -1 to 1
    vMicrobeID = microbe_id;
    vParticleWorldPos = particlePos;
    vInfluenceRadius = base_radius * 0.9;  // Influence radius slightly smaller than billboard
}
