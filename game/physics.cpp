#include "game/physics.h"
#include "game/microbe_bodies.h"
#include <cstdlib>
#include <cstring>
#include <stdio.h>

// raylib/OpenGL
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "external/glad.h"

// Bullet Physics headers
#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyHelpers.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>
#include <BulletSoftBody/btDefaultSoftBodySolver.h>

// Microbe body structure (Phase 2.4)
struct MicrobeBody {
    btSoftBody* softBody;
    MicrobeType type;
    float seed;

    // EC&M locomotion state (Phase 2.5)
    float ecmPhase;          // 0-1 cycle position
    int pseudopodTarget;     // Which node extends as pseudopod
    btVector3 pseudopodDir;  // Direction of pseudopod extension

    // EC&M force application (Phase 2.5)
    void applyECMForces(float dt);
};

PhysicsContext::PhysicsContext()
    : world(nullptr)
    , worldInfo(nullptr)
    , broadphase(nullptr)
    , dispatcher(nullptr)
    , collisionConfig(nullptr)
    , solver(nullptr)
    , microbes(nullptr)
    , maxMicrobes(0)
    , microbeCount(0)
    , particlesCPU(nullptr)
    , microbesCPU(nullptr)
    , maxParticles(0)
    , particleCount(0)
    , particleSSBO(0)
    , microbeSSBO(0)
    , ready(false)
{
}

PhysicsContext::~PhysicsContext() {
    // Phase 2.2: Cleanup
    // Delete all microbe soft bodies
    if (microbes) {
        for (int i = 0; i < microbeCount; i++) {
            if (microbes[i] && microbes[i]->softBody) {
                world->removeSoftBody(microbes[i]->softBody);
                delete microbes[i]->softBody;
            }
            delete microbes[i];
        }
        free(microbes);
    }

    // Delete Bullet world (worldInfo is owned by world, don't delete separately)
    delete world;  // This also deletes the soft body solver
    delete solver;
    delete broadphase;
    delete dispatcher;
    delete collisionConfig;

    // Delete CPU buffers
    free(particlesCPU);
    free(microbesCPU);

    // Delete OpenGL SSBOs
    if (particleSSBO) glDeleteBuffers(1, &particleSSBO);
    if (microbeSSBO) glDeleteBuffers(1, &microbeSSBO);
}

PhysicsContext* PhysicsContext::create(int max_microbes) {
    PhysicsContext* ctx = new PhysicsContext();
    if (!ctx->init(max_microbes)) {
        delete ctx;
        return nullptr;
    }
    return ctx;
}

void PhysicsContext::destroy() {
    // TODO Phase 2.2: Cleanup Bullet world, soft bodies, SSBOs
    delete this;
}

bool PhysicsContext::init(int max_microbes) {
    maxMicrobes = max_microbes;
    microbeCount = 0;

    // Allocate microbe array
    microbes = (MicrobeBody**)calloc(max_microbes, sizeof(MicrobeBody*));
    if (!microbes) {
        fprintf(stderr, "physics: failed to allocate microbe array\n");
        return false;
    }

    // Allocate CPU staging buffers
    const int PARTICLES_PER_MICROBE = 32;  // Estimated particles per microbe
    maxParticles = max_microbes * PARTICLES_PER_MICROBE;
    particleCount = 0;

    particlesCPU = (ParticleData*)calloc(maxParticles, sizeof(ParticleData));
    microbesCPU = (MicrobeData*)calloc(max_microbes, sizeof(MicrobeData));

    if (!particlesCPU || !microbesCPU) {
        fprintf(stderr, "physics: failed to allocate CPU staging buffers\n");
        return false;
    }

    // Create OpenGL SSBOs for rendering
    glGenBuffers(1, &particleSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxParticles * sizeof(ParticleData), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &microbeSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, microbeSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, max_microbes * sizeof(MicrobeData), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Initialize Bullet physics world
    collisionConfig = new btSoftBodyRigidBodyCollisionConfiguration();
    dispatcher = new btCollisionDispatcher(collisionConfig);
    broadphase = new btDbvtBroadphase();
    solver = new btSequentialImpulseConstraintSolver();

    btSoftBodySolver* softBodySolver = new btDefaultSoftBodySolver();
    world = new btSoftRigidDynamicsWorld(dispatcher, broadphase, solver, collisionConfig, softBodySolver);

    world->setGravity(btVector3(0, 0, 0));  // Microbes float in medium

    // Get world info from dynamics world
    worldInfo = &(world->getWorldInfo());

    ready = true;
    printf("physics: initialized Bullet soft body world (max %d microbes)\n", max_microbes);
    return true;
}

void PhysicsContext::spawnMicrobe(float x, float z, MicrobeType type, float seed) {
    if (microbeCount >= maxMicrobes) {
        fprintf(stderr, "physics: max microbes reached\n");
        return;
    }

    // Get body plan (for future custom node layouts)
    MicrobeBodyPlan plan = getBodyPlan(type);

    // Create Bullet soft body ellipsoid (Phase 2.4: simple deformable blob)
    // Use lower node count for stability (32 is enough for blob-like amoeba)
    btSoftBody* body = btSoftBodyHelpers::CreateEllipsoid(
        *worldInfo,
        btVector3(x, 0, z),  // Center position
        btVector3(1.5f, 1.5f, 1.5f),  // Radii
        32  // Resolution (number of nodes) - reduced for stability
    );

    // TODO Phase 4: Use custom node layouts from body plan for different microbe types

    // Configure soft body material properties for gel-like amoeba
    body->m_cfg.kDF = 0.2f;                   // Dynamic friction
    body->m_cfg.kDP = 0.1f;                   // Damping (higher = more viscous)
    body->m_cfg.kPR = 100.0f;                 // Pressure (volume preservation)
    body->m_cfg.kVC = 0.0f;                   // Volume conservation coefficient
    body->m_cfg.piterations = 4;              // Position solver iterations
    body->m_cfg.viterations = 0;              // Velocity solver iterations
    body->m_cfg.collisions = btSoftBody::fCollision::SDF_RS;  // Collision with rigid bodies

    // Set material stiffness for links (soft and squishy like gel)
    btSoftBody::Material* pm = body->appendMaterial();
    pm->m_kLST = 0.3f;  // Linear stiffness (0-1, lower = more deformable)
    pm->m_kAST = 0.3f;  // Area stiffness
    pm->m_kVST = 0.3f;  // Volume stiffness
    body->generateBendingConstraints(2, pm);

    // Add to Bullet world
    world->addSoftBody(body);

    // Create and store microbe
    MicrobeBody* microbe = new MicrobeBody();
    microbe->softBody = body;
    microbe->type = type;
    microbe->seed = seed;
    microbe->ecmPhase = 0.0f;
    microbe->pseudopodTarget = (int)(seed * 1000) % body->m_nodes.size();
    microbe->pseudopodDir = btVector3(1, 0, 0);  // Initial direction

    microbes[microbeCount] = microbe;
    microbeCount++;

    // Update particle count based on actual soft body nodes
    particleCount += body->m_nodes.size();
}

// EC&M Locomotion Implementation (Phase 2.5)
void MicrobeBody::applyECMForces(float dt) {
    const float CYCLE_DURATION = 12.0f;  // 12-second behavioral cycle
    const float FORCE_MAGNITUDE = 2.0f;   // Force strength (reduced for stability)

    // Advance cycle phase
    ecmPhase += dt / CYCLE_DURATION;

    // Reset cycle and pick new pseudopod direction
    if (ecmPhase >= 1.0f) {
        ecmPhase = 0.0f;

        // Pick random node on surface as new pseudopod target
        int nodeCount = softBody->m_nodes.size();
        pseudopodTarget = (int)(seed * 1000 + ecmPhase * 10000) % nodeCount;

        // Calculate outward direction from center of mass
        btVector3 centerOfMass(0, 0, 0);
        for (int i = 0; i < nodeCount; i++) {
            centerOfMass += softBody->m_nodes[i].m_x;
        }
        centerOfMass /= (float)nodeCount;

        btVector3 targetPos = softBody->m_nodes[pseudopodTarget].m_x;
        pseudopodDir = (targetPos - centerOfMass);
        pseudopodDir.setY(0.0f);  // Project to XZ plane (2D top-down movement only)
        pseudopodDir = pseudopodDir.normalized();
    }

    // Calculate cycle phase strengths (smoothstep for smooth transitions)
    auto smoothstep = [](float edge0, float edge1, float x) {
        float t = (x - edge0) / (edge1 - edge0);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return t * t * (3.0f - 2.0f * t);
    };

    float extend = smoothstep(0.0f, 0.35f, ecmPhase);
    float search = smoothstep(0.35f, 0.75f, ecmPhase) * (1.0f - smoothstep(0.75f, 1.0f, ecmPhase));
    float retract = smoothstep(0.75f, 1.0f, ecmPhase);

    // Get target node
    btSoftBody::Node& targetNode = softBody->m_nodes[pseudopodTarget];

    // Phase 1: Extension (0-35%) - Push pseudopod outward
    if (extend > 0.01f) {
        btVector3 force = pseudopodDir * FORCE_MAGNITUDE * extend;
        targetNode.m_f += force;
    }

    // Phase 2: Search (35-75%) - Lateral wiggle to find substrate
    if (search > 0.01f) {
        // Perpendicular direction for lateral movement (in XZ plane)
        btVector3 up(0, 1, 0);
        btVector3 perpendicular = pseudopodDir.cross(up);
        perpendicular.setY(0.0f);  // Keep in XZ plane
        perpendicular = perpendicular.normalized();

        // Sinusoidal wiggle
        float wiggleFreq = 5.0f;  // Multiple wiggles during search phase
        float wiggle = sinf(ecmPhase * wiggleFreq * 2.0f * 3.14159f);

        btVector3 force = perpendicular * wiggle * FORCE_MAGNITUDE * search * 0.5f;
        targetNode.m_f += force;
    }

    // Phase 3: Retraction (75-100%) - Pull body forward
    if (retract > 0.01f) {
        // Pull pseudopod back toward center, dragging body forward
        btVector3 centerOfMass(0, 0, 0);
        int nodeCount = softBody->m_nodes.size();
        for (int i = 0; i < nodeCount; i++) {
            centerOfMass += softBody->m_nodes[i].m_x;
        }
        centerOfMass /= (float)nodeCount;

        btVector3 retractDir = (centerOfMass - targetNode.m_x);
        retractDir.setY(0.0f);  // Keep in XZ plane (2D top-down movement)
        retractDir = retractDir.normalized();
        btVector3 force = retractDir * FORCE_MAGNITUDE * retract;
        targetNode.m_f += force;

        // Also apply small forward force to rest of body (simulating adhesion release)
        btVector3 bodyForce = pseudopodDir * FORCE_MAGNITUDE * retract * 0.2f;
        for (int i = 0; i < nodeCount; i++) {
            if (i != pseudopodTarget) {
                softBody->m_nodes[i].m_f += bodyForce / (float)nodeCount;
            }
        }
    }
}

void PhysicsContext::clear() {
    // TODO Phase 2.2: Remove all soft bodies from world
    microbeCount = 0;
}

void PhysicsContext::update(float dt, float bounds_x, float bounds_y,
                             float cursor_x, float cursor_z) {
    if (!ready || microbeCount == 0) return;

    // Phase 2.5: Apply EC&M locomotion forces
    for (int i = 0; i < microbeCount; i++) {
        if (microbes[i] && microbes[i]->type == MicrobeType::AMOEBA) {
            microbes[i]->applyECMForces(dt);
        }
    }

    // TODO Phase 2.6: Apply cursor attraction forces

    // Apply boundary forces to keep microbes in visible area and at Y=0
    for (int i = 0; i < microbeCount; i++) {
        if (!microbes[i] || !microbes[i]->softBody) continue;

        btSoftBody* body = microbes[i]->softBody;
        int nodeCount = body->m_nodes.size();

        // Calculate center of mass
        btVector3 centerOfMass(0, 0, 0);
        for (int j = 0; j < nodeCount; j++) {
            centerOfMass += body->m_nodes[j].m_x;
        }
        centerOfMass /= (float)nodeCount;

        // Apply soft boundary forces (gentle push when near edges)
        const float margin = 2.0f;  // Start applying force this far from edge
        btVector3 boundaryForce(0, 0, 0);

        // X boundaries
        if (centerOfMass.x() > bounds_x - margin) {
            float penetration = centerOfMass.x() - (bounds_x - margin);
            boundaryForce.setX(-penetration * 5.0f);
        } else if (centerOfMass.x() < -(bounds_x - margin)) {
            float penetration = -(bounds_x - margin) - centerOfMass.x();
            boundaryForce.setX(penetration * 5.0f);
        }

        // Y constraint - keep microbes near Y=0 plane (2D top-down view)
        // Apply VERY strong restoring force to counteract any Y drift
        if (fabsf(centerOfMass.y()) > 0.01f) {
            boundaryForce.setY(-centerOfMass.y() * 500.0f);  // Very strong pull to Y=0
        }

        // Z boundaries
        if (centerOfMass.z() > bounds_y - margin) {
            float penetration = centerOfMass.z() - (bounds_y - margin);
            boundaryForce.setZ(-penetration * 5.0f);
        } else if (centerOfMass.z() < -(bounds_y - margin)) {
            float penetration = -(bounds_y - margin) - centerOfMass.z();
            boundaryForce.setZ(penetration * 5.0f);
        }

        // Apply boundary force to all nodes
        if (boundaryForce.length2() > 0.01f) {
            for (int j = 0; j < nodeCount; j++) {
                body->m_nodes[j].m_f += boundaryForce / (float)nodeCount;
            }
        }
    }

    // Step Bullet physics simulation
    // Use fixed internal timestep (1/60) for stability, allow up to 10 substeps
    const float fixedTimeStep = 1.0f / 60.0f;
    world->stepSimulation(dt, 10, fixedTimeStep);

    // Phase 2.6: Sync Bullet state to CPU buffers and upload to GPU
    syncToSSBOs();
}

// Sync Bullet physics state to SSBOs for rendering (Phase 2.6)
void PhysicsContext::syncToSSBOs() {
    int particleIdx = 0;

    // Copy soft body node positions to particle buffer
    for (int m = 0; m < microbeCount; m++) {
        MicrobeBody* microbe = microbes[m];
        if (!microbe || !microbe->softBody) continue;

        btSoftBody* body = microbe->softBody;
        int nodeCount = body->m_nodes.size();

        // Update microbe metadata
        if (m < maxMicrobes) {
            // Calculate center of mass
            btVector3 centerOfMass(0, 0, 0);
            for (int i = 0; i < nodeCount; i++) {
                centerOfMass += body->m_nodes[i].m_x;
            }
            centerOfMass /= (float)nodeCount;

            microbesCPU[m].center[0] = centerOfMass.x();
            microbesCPU[m].center[1] = centerOfMass.y();
            microbesCPU[m].center[2] = centerOfMass.z();
            microbesCPU[m].center[3] = 1.5f;  // radius

            microbesCPU[m].color[0] = 0.3f;  // r
            microbesCPU[m].color[1] = 0.8f;  // g
            microbesCPU[m].color[2] = 0.5f;  // b
            microbesCPU[m].color[3] = 0.7f;  // a

            microbesCPU[m].params[0] = (float)microbe->type;
            microbesCPU[m].params[1] = 200.0f;  // stiffness
            microbesCPU[m].params[2] = microbe->seed;
            microbesCPU[m].params[3] = 0.0f;  // squish

            // AABB (simplified - just use center ± radius for now)
            microbesCPU[m].aabb[0] = centerOfMass.x() - 2.0f;
            microbesCPU[m].aabb[1] = centerOfMass.z() - 2.0f;
            microbesCPU[m].aabb[2] = centerOfMass.x() + 2.0f;
            microbesCPU[m].aabb[3] = centerOfMass.z() + 2.0f;
        }

        // Copy node positions to particle buffer
        for (int i = 0; i < nodeCount && particleIdx < maxParticles; i++) {
            btSoftBody::Node& node = body->m_nodes[i];

            particlesCPU[particleIdx].pos[0] = node.m_x.x();
            particlesCPU[particleIdx].pos[1] = node.m_x.y();
            particlesCPU[particleIdx].pos[2] = node.m_x.z();
            particlesCPU[particleIdx].pos[3] = 1.0f / node.m_im;  // inverse mass -> mass

            particlesCPU[particleIdx].pos_prev[0] = node.m_q.x();
            particlesCPU[particleIdx].pos_prev[1] = node.m_q.y();
            particlesCPU[particleIdx].pos_prev[2] = node.m_q.z();
            particlesCPU[particleIdx].pos_prev[3] = 0;

            particlesCPU[particleIdx].vel[0] = node.m_v.x();
            particlesCPU[particleIdx].vel[1] = node.m_v.y();
            particlesCPU[particleIdx].vel[2] = node.m_v.z();
            particlesCPU[particleIdx].vel[3] = (float)m;  // microbe_id

            particlesCPU[particleIdx].data[0] = (float)particleIdx;
            particlesCPU[particleIdx].data[1] = 0;
            particlesCPU[particleIdx].data[2] = 0;
            particlesCPU[particleIdx].data[3] = 0;  // is_membrane

            particleIdx++;
        }
    }

    // Upload to GPU SSBOs
    if (particleIdx > 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                       particleIdx * sizeof(ParticleData), particlesCPU);
    }

    if (microbeCount > 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, microbeSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                       microbeCount * sizeof(MicrobeData), microbesCPU);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Update particle count
    particleCount = particleIdx;
}

void PhysicsContext::render(Camera3D camera) {
    // Phase 2.7: Simple temporary rendering (draw microbe centers as spheres)
    // TODO Phase 3: Replace with proper metaball rendering

    if (!ready || microbeCount == 0) return;

    for (int i = 0; i < microbeCount; i++) {
        MicrobeBody* microbe = microbes[i];
        if (!microbe || !microbe->softBody) continue;

        btSoftBody* body = microbe->softBody;
        int nodeCount = body->m_nodes.size();

        // Calculate center of mass
        btVector3 centerOfMass(0, 0, 0);
        for (int j = 0; j < nodeCount; j++) {
            centerOfMass += body->m_nodes[j].m_x;
        }
        centerOfMass /= (float)nodeCount;

        // Draw center as sphere
        DrawSphere(
            (Vector3){centerOfMass.x(), centerOfMass.y(), centerOfMass.z()},
            1.5f,
            (Color){76, 204, 128, 180}
        );

        // Draw a few nodes as small spheres to show deformation
        for (int j = 0; j < nodeCount && j < 32; j += 16) {
            btVector3 nodePos = body->m_nodes[j].m_x;
            DrawSphere(
                (Vector3){nodePos.x(), nodePos.y(), nodePos.z()},
                0.3f,
                (Color){100, 220, 150, 200}
            );
        }
    }
}

unsigned int PhysicsContext::getParticleSSBO() const {
    return particleSSBO;
}

unsigned int PhysicsContext::getMicrobeSSBO() const {
    return microbeSSBO;
}

int PhysicsContext::getMicrobeCount() const {
    return microbeCount;
}

int PhysicsContext::getParticleCount() const {
    return particleCount;
}

float PhysicsContext::getMicrobeVolume(int microbeIndex) const {
    if (microbeIndex < 0 || microbeIndex >= microbeCount || !microbes[microbeIndex]) {
        return 0.0f;
    }

    btSoftBody* body = microbes[microbeIndex]->softBody;
    if (!body) return 0.0f;

    return body->getVolume();
}

float PhysicsContext::getMicrobeMaxRadius(int microbeIndex) const {
    if (microbeIndex < 0 || microbeIndex >= microbeCount || !microbes[microbeIndex]) {
        return 0.0f;
    }

    btSoftBody* body = microbes[microbeIndex]->softBody;
    if (!body) return 0.0f;

    int nodeCount = body->m_nodes.size();
    if (nodeCount == 0) return 0.0f;

    // Calculate center of mass
    btVector3 center(0, 0, 0);
    for (int i = 0; i < nodeCount; i++) {
        center += body->m_nodes[i].m_x;
    }
    center /= (float)nodeCount;

    // Find max distance from center
    float maxRadius = 0.0f;
    for (int i = 0; i < nodeCount; i++) {
        float dist = (body->m_nodes[i].m_x - center).length();
        if (dist > maxRadius) maxRadius = dist;
    }

    return maxRadius;
}

void PhysicsContext::getMicrobeCenterOfMass(int microbeIndex, float* x, float* y, float* z) const {
    if (microbeIndex < 0 || microbeIndex >= microbeCount || !microbes[microbeIndex]) {
        if (x) *x = 0.0f;
        if (y) *y = 0.0f;
        if (z) *z = 0.0f;
        return;
    }

    btSoftBody* body = microbes[microbeIndex]->softBody;
    if (!body) {
        if (x) *x = 0.0f;
        if (y) *y = 0.0f;
        if (z) *z = 0.0f;
        return;
    }

    int nodeCount = body->m_nodes.size();
    btVector3 center(0, 0, 0);
    for (int i = 0; i < nodeCount; i++) {
        center += body->m_nodes[i].m_x;
    }
    center /= (float)nodeCount;

    if (x) *x = center.x();
    if (y) *y = center.y();
    if (z) *z = center.z();
}
