#pragma once

#include "raylib.h"

// Microbe type enum
enum class MicrobeType {
    AMOEBA,
    STENTOR,
    LACRYMARIA,
    VORTICELLA,
    DIDINIUM,
    HELIOZOA,
    RADIOLARIAN,
    DIATOM,
    // Bacteria types
    COCCUS,
    BACILLUS,
    VIBRIO,
    SPIRILLUM,
    // Viruses
    VIRUS_ICOSAHEDRAL,
    VIRUS_BACTERIOPHAGE
};

// Forward declarations for Bullet types
class btSoftRigidDynamicsWorld;
class btSoftBodyWorldInfo;
class btBroadphaseInterface;
class btCollisionDispatcher;
class btDefaultCollisionConfiguration;
class btSequentialImpulseConstraintSolver;
struct MicrobeBody;

// Physics context - manages Bullet soft body simulation
class PhysicsContext {
public:
    // Factory method - creates and initializes physics world
    static PhysicsContext* create(int max_microbes);

    // Destructor
    void destroy();

    // Microbe spawning
    void spawnMicrobe(float x, float z, MicrobeType type, float seed);
    void clear();

    // Physics simulation
    void update(float dt, float bounds_x, float bounds_y,
                float cursor_x, float cursor_z);

    // Rendering support - SSBO access (maintain compatibility with old rendering)
    void render(Camera3D camera);
    unsigned int getParticleSSBO() const;
    unsigned int getMicrobeSSBO() const;
    int getMicrobeCount() const;
    int getParticleCount() const;

    // Diagnostics
    float getMicrobeVolume(int microbeIndex) const;
    float getMicrobeMaxRadius(int microbeIndex) const;
    void getMicrobeCenterOfMass(int microbeIndex, float* x, float* y, float* z) const;

private:
    // Constructor is private - use create() factory method
    PhysicsContext();
    ~PhysicsContext();

    // Internal initialization
    bool init(int max_microbes);

    // Sync Bullet state to SSBOs for rendering
    void syncToSSBOs();

    // Bullet physics world
    btSoftRigidDynamicsWorld* world;
    btSoftBodyWorldInfo* worldInfo;
    btBroadphaseInterface* broadphase;
    btCollisionDispatcher* dispatcher;
    btDefaultCollisionConfiguration* collisionConfig;
    btSequentialImpulseConstraintSolver* solver;

    // Microbe storage
    MicrobeBody** microbes;
    int maxMicrobes;
    int microbeCount;

    // SSBO buffers for rendering (CPU-side staging)
    struct ParticleData {
        float pos[4];       // xyz position, w = inverse_mass
        float pos_prev[4];  // previous position
        float vel[4];       // xyz velocity, w = microbe_id
        float data[4];      // x=particle_idx, y=unused, z=unused, w=is_membrane
    };

    struct MicrobeData {
        float center[4];    // xyz center, w = radius
        float color[4];     // rgba
        float params[4];    // x=type, y=stiffness, z=seed, w=squish
        float aabb[4];      // bounding box (min_x, min_z, max_x, max_z)
    };

    ParticleData* particlesCPU;
    MicrobeData* microbesCPU;
    int maxParticles;
    int particleCount;

    // OpenGL SSBOs
    unsigned int particleSSBO;
    unsigned int microbeSSBO;
    bool ready;
};
