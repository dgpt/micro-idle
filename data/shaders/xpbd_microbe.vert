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
    vec2 localDir = normalize(aPos.xz + vec2(0.0001));
    float localAngle = atan(localDir.y, localDir.x);
    float localDist = length(aPos.xz);

    // Smooth blob-like deformation using weighted blend of ALL particles
    // This creates organic, amoeba-like shapes instead of faceted polygons
    vec3 blendedPos = vec3(0.0);
    float totalWeight = 0.0;

    for (int i = 0; i < u_particles_per_microbe; i++) {
        vec3 ppos = particles[particle_start + i].pos.xyz;
        vec3 toParticle = ppos - center;
        vec2 particleDir = normalize(toParticle.xz + vec2(0.0001));

        // Angular distance between this vertex and particle
        float angle_diff = acos(clamp(dot(localDir, particleDir), -1.0, 1.0));

        // Smooth falloff - particles influence nearby vertices smoothly
        // Using a wide Gaussian falloff for blob-like organic shapes
        float sigma = 1.5;  // Wide influence for smooth blending
        float weight = exp(-(angle_diff * angle_diff) / (2.0 * sigma * sigma));

        // Radial influence - particles affect the shape outward
        float particleRadius = length(toParticle.xz);
        vec3 extendedPos = center + normalize(toParticle) * particleRadius * (1.0 + localDist * 0.5);

        blendedPos += extendedPos * weight;
        totalWeight += weight;
    }

    // Normalize by total weight
    vec3 deformedEdge = (totalWeight > 0.001) ? (blendedPos / totalWeight) : center;

    // Smooth interpolation from center to deformed edge
    // Use smoothstep for even more organic feel
    float blend = smoothstep(0.0, 1.0, localDist);
    vec3 worldPos = mix(center, deformedEdge, blend);

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
