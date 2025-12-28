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

out vec2 vBillboardUV;
out float vMicrobeID;
out vec3 vParticleWorldPos;
out float vInfluenceRadius;

void main() {
    int particle_id = gl_InstanceID;
    Particle p = particles[particle_id];

    // Render ONLY skeleton particles - they're the ones that actually deform with pseudopods
    bool is_membrane = (p.data.w > 0.5);
    if (is_membrane) {
        // Skip membrane ring - it doesn't follow pseudopod shape
        gl_Position = vec4(0, 0, -10, 1);
        vBillboardUV = vec2(0);
        vMicrobeID = 0.0;
        vParticleWorldPos = vec3(0);
        vInfluenceRadius = 0.0;
        return;
    }

    vec3 particlePos = p.pos.xyz;
    float microbe_id = p.vel.w;
    int m_id = int(microbe_id + 0.5);

    // Get microbe data for per-type radius
    Microbe m = microbes[m_id];
    float base_radius = m.center.w;

    // Moderate billboards - let skeleton shape show through
    float billboardSize = 0.7;

    // Create billboard in world space (aligned to XZ plane for top-down view)
    vec3 right = vec3(1, 0, 0);
    vec3 forward = vec3(0, 0, 1);

    vec3 worldPos = particlePos + (right * aCorner.x + forward * aCorner.y) * billboardSize;

    gl_Position = u_vp * vec4(worldPos, 1.0);

    vBillboardUV = aCorner;
    vMicrobeID = microbe_id;
    vParticleWorldPos = particlePos;
    vInfluenceRadius = base_radius * 0.9;
}
