#version 430 core

// Vertex shader for XPBD-driven microbe rendering
// Deforms a base mesh based on particle positions from the physics simulation.

layout(location = 0) in vec3 aPos;  // Base vertex position (unit circle)

// Particle data from XPBD simulation
struct Particle {
    vec4 pos;       // xyz position, w = inverse mass
    vec4 pos_prev;  // previous position
    vec4 vel;       // xyz velocity, w = microbe_id
    vec4 data;      // x = particle_index, y = constraint_start, z = constraint_count
};

struct Microbe {
    vec4 center;    // xyz center of mass, w = radius
    vec4 color;     // rgba
    vec4 params;    // x = type, y = stiffness, z = seed, w = squish_amount
    vec4 aabb;      // bounding box
};

layout(std430, binding = 0) readonly buffer Particles { Particle particles[]; };
layout(std430, binding = 1) readonly buffer Microbes { Microbe microbes[]; };

uniform mat4 u_vp;
uniform float u_time;
uniform int u_particles_per_microbe;

out vec4 vColor;
out vec2 vLocal;
out float vType;
out float vSquish;
out float vSeed;

void main() {
    int microbe_id = gl_InstanceID;
    Microbe m = microbes[microbe_id];

    int particle_start = microbe_id * u_particles_per_microbe;

    // Get base properties
    vec3 center = m.center.xyz;
    float base_radius = m.center.w;
    float type = m.params.x;
    float seed = m.params.z;
    float squish = m.params.w;

    // Compute local vertex position
    // aPos is on a unit circle, we'll deform it based on nearby particles
    vec2 localDir = normalize(aPos.xz + vec2(0.0001));
    float localAngle = atan(localDir.y, localDir.x);
    float localDist = length(aPos.xz);

    // Find the particle closest to this vertex direction and use its position
    // to deform the mesh
    float bestDot = -1.0;
    vec3 closestParticle = center;
    vec3 secondClosest = center;

    for (int i = 0; i < u_particles_per_microbe; i++) {
        vec3 ppos = particles[particle_start + i].pos.xyz;
        vec3 toParticle = ppos - center;
        vec2 particleDir = normalize(toParticle.xz + vec2(0.0001));

        float d = dot(localDir, particleDir);
        if (d > bestDot) {
            secondClosest = closestParticle;
            closestParticle = ppos;
            bestDot = d;
        }
    }

    // Interpolate between closest particles for smooth deformation
    float t = (bestDot + 1.0) * 0.5;  // 0 to 1
    vec3 deformedEdge = mix(secondClosest, closestParticle, t);

    // Compute final vertex position
    // Blend between center and deformed edge based on localDist
    vec3 worldPos = mix(center, deformedEdge, localDist);

    // Add subtle organic motion
    float breathe = 1.0 + sin(u_time * 1.5 + seed * 6.28) * 0.03;
    float wave = sin(localAngle * 3.0 + u_time * 2.0 + seed * 4.0) * 0.02;
    worldPos.xz += (worldPos.xz - center.xz) * (breathe - 1.0 + wave);

    // Compute dome height (3D effect for top-down view)
    float distFromCenter = length(worldPos.xz - center.xz) / (base_radius + 0.001);
    float dome = sqrt(max(0.0, 1.0 - distFromCenter * distFromCenter));

    // Squish affects dome height
    float domeHeight = 0.35 * (1.0 + squish * 0.3);
    worldPos.y = dome * domeHeight * base_radius;

    gl_Position = u_vp * vec4(worldPos, 1.0);

    // Pass to fragment shader
    vColor = m.color;
    vLocal = aPos.xz;
    vType = type;
    vSquish = squish;
    vSeed = seed;
}
