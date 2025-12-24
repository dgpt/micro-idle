#include "game/game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raymath.h"
#include "engine/util/rng.h"

#define PI_F 3.14159265358979323846f
#define GRID_W 16
#define GRID_H 12
#define GRID_CELL 2.0f
#define GRID_ORIGIN_X -14.0f
#define GRID_ORIGIN_Z -12.0f

typedef enum BaseForm {
    FORM_COCCUS = 0,
    FORM_BACILLUS,
    FORM_VIBRIO,
    FORM_SPIRILLUM,
    FORM_FILAMENT,
    FORM_STELLATE,
    FORM_CLUSTER,
    FORM_DIATOM,
    FORM_AMOEBA,
    FORM_FLAGELLATE,
    FORM_COUNT
} BaseForm;

typedef enum TraitId {
    TRAIT_CAPSULE = 0,
    TRAIT_FLAGELLA,
    TRAIT_PILI,
    TRAIT_ENDOSPORE,
    TRAIT_LPS,
    TRAIT_PHOTOSYNTH,
    TRAIT_QUORUM,
    TRAIT_COUNT
} TraitId;

typedef enum ResourceType {
    RES_SODIUM = 0,
    RES_GLUCOSE,
    RES_IRON,
    RES_CALCIUM,
    RES_LIPIDS,
    RES_OXYGEN,
    RES_SIGNALS,
    RES_COUNT
} ResourceType;

typedef struct Resource {
    const char *name;
    float amount;
    bool unlocked;
} Resource;

typedef struct TraitDef {
    const char *name;
    const char *desc;
    ResourceType resource;
    ResourceType cost_resource;
    float cost_amount;
    uint32_t flag;
    uint32_t prereq_mask;
} TraitDef;

typedef struct UpgradeDef {
    const char *name;
    const char *desc;
    float base_cost;
    float cost_mult;
} UpgradeDef;

typedef struct Microbe {
    Vector3 pos;
    Vector3 vel;
    float size;
    float hp;
    float hp_max;
    BaseForm form;
    uint32_t traits;
    float wobble;
    float phase;
    float pulse;
    float spin;
    float drift;
    float twist;
    int appendages;
    float squish;
    float dormant_timer;
    bool dormant;
} Microbe;

typedef struct Zone {
    Vector3 pos;
    float radius;
    float timer;
    float strength;
    bool active;
} Zone;

struct GameState {
    Rng rng;
    Microbe microbes[MAX_MICROBES];
    int microbe_count;
    Zone zones[MAX_ZONES];
    int grid_head[GRID_W * GRID_H];
    int grid_next[MAX_MICROBES];

    Resource resources[RES_COUNT];
    TraitDef traits[TRAIT_COUNT];
    bool trait_unlocked[TRAIT_COUNT];

    UpgradeDef upgrades[RES_COUNT];
    int upgrade_level[RES_COUNT];

    float spawn_timer;
    float spawn_rate;
    float hover_damage;
    float click_damage;
    float aoe_cooldown;
    float aoe_timer;
    float pulse_timer;

    int hovered_index;
    Vector3 hover_pos;
    bool hover_valid;
    bool show_bounds;
};

static void cleanup_dead(GameState *game);

static Color color_for_resource(ResourceType type) {
    switch (type) {
        case RES_SODIUM: return (Color){160, 210, 255, 255};
        case RES_GLUCOSE: return (Color){255, 220, 140, 255};
        case RES_IRON: return (Color){220, 120, 120, 255};
        case RES_CALCIUM: return (Color){235, 235, 235, 255};
        case RES_LIPIDS: return (Color){255, 170, 90, 255};
        case RES_OXYGEN: return (Color){140, 220, 190, 255};
        case RES_SIGNALS: return (Color){210, 160, 255, 255};
        default: return WHITE;
    }
}

static float clampf(float v, float min, float max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

static float hashf(float n) {
    float s = sinf(n) * 43758.5453f;
    return s - floorf(s);
}

static float vec3_dist(Vector3 a, Vector3 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static float vec3_dist_sq(Vector3 a, Vector3 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static float vec3_dot(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static void grid_coords(Vector3 pos, int *out_x, int *out_z) {
    int gx = (int)((pos.x - GRID_ORIGIN_X) / GRID_CELL);
    int gz = (int)((pos.z - GRID_ORIGIN_Z) / GRID_CELL);
    if (gx < 0) gx = 0;
    if (gz < 0) gz = 0;
    if (gx >= GRID_W) gx = GRID_W - 1;
    if (gz >= GRID_H) gz = GRID_H - 1;
    *out_x = gx;
    *out_z = gz;
}

static void build_spatial_grid(GameState *game) {
    int cells = GRID_W * GRID_H;
    for (int i = 0; i < cells; ++i) {
        game->grid_head[i] = -1;
    }
    for (int i = 0; i < game->microbe_count; ++i) {
        int cx = 0;
        int cz = 0;
        grid_coords(game->microbes[i].pos, &cx, &cz);
        int idx = cz * GRID_W + cx;
        game->grid_next[i] = game->grid_head[idx];
        game->grid_head[idx] = i;
    }
}

static void resolve_collisions(GameState *game, float dt) {
    build_spatial_grid(game);
    for (int i = 0; i < game->microbe_count; ++i) {
        Microbe *a = &game->microbes[i];
        int cx = 0;
        int cz = 0;
        grid_coords(a->pos, &cx, &cz);

        for (int dz = -1; dz <= 1; ++dz) {
            int nz = cz + dz;
            if (nz < 0 || nz >= GRID_H) continue;
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = cx + dx;
                if (nx < 0 || nx >= GRID_W) continue;
                int head = game->grid_head[nz * GRID_W + nx];
                for (int j = head; j != -1; j = game->grid_next[j]) {
                    if (j <= i) continue;
                    Microbe *b = &game->microbes[j];
                    float radius = (a->size + b->size) * 0.9f;
                    float d2 = vec3_dist_sq(a->pos, b->pos);
                    if (d2 >= radius * radius) {
                        continue;
                    }
                    float dist = sqrtf(fmaxf(d2, 0.0001f));
                    Vector3 n = { (b->pos.x - a->pos.x) / dist, 0.0f, (b->pos.z - a->pos.z) / dist };
                    float overlap = radius - dist;
                    float inv_mass_a = 1.0f / fmaxf(a->size, 0.2f);
                    float inv_mass_b = 1.0f / fmaxf(b->size, 0.2f);
                    float total_inv = inv_mass_a + inv_mass_b;
                    float correction = overlap / total_inv;
                    a->pos.x -= n.x * correction * inv_mass_a;
                    a->pos.z -= n.z * correction * inv_mass_a;
                    b->pos.x += n.x * correction * inv_mass_b;
                    b->pos.z += n.z * correction * inv_mass_b;

                    Vector3 rel = { b->vel.x - a->vel.x, 0.0f, b->vel.z - a->vel.z };
                    float rel_norm = vec3_dot(rel, n);
                    if (rel_norm < 0.0f) {
                        float restitution = 0.6f;
                        float impulse = -(1.0f + restitution) * rel_norm / total_inv;
                        a->vel.x -= n.x * impulse * inv_mass_a;
                        a->vel.z -= n.z * impulse * inv_mass_a;
                        b->vel.x += n.x * impulse * inv_mass_b;
                        b->vel.z += n.z * impulse * inv_mass_b;
                    }

                    float softness = overlap * 10.0f;
                    a->vel.x -= n.x * softness * dt * inv_mass_a;
                    a->vel.z -= n.z * softness * dt * inv_mass_a;
                    b->vel.x += n.x * softness * dt * inv_mass_b;
                    b->vel.z += n.z * softness * dt * inv_mass_b;

                    float squish = overlap / fmaxf(radius, 0.001f);
                    squish = clampf(squish * 1.8f, 0.0f, 1.2f);
                    if (squish > a->squish) a->squish = squish;
                    if (squish > b->squish) b->squish = squish;
                }
            }
        }
    }
}

static void init_resources(GameState *game) {
    game->resources[RES_SODIUM] = (Resource){"Sodium", 0.0f, true};
    game->resources[RES_GLUCOSE] = (Resource){"Glucose", 0.0f, false};
    game->resources[RES_IRON] = (Resource){"Iron", 0.0f, false};
    game->resources[RES_CALCIUM] = (Resource){"Calcium", 0.0f, false};
    game->resources[RES_LIPIDS] = (Resource){"Lipids", 0.0f, false};
    game->resources[RES_OXYGEN] = (Resource){"Oxygen", 0.0f, false};
    game->resources[RES_SIGNALS] = (Resource){"Signals", 0.0f, false};
}

static void init_traits(GameState *game) {
    game->traits[TRAIT_CAPSULE] = (TraitDef){
        "Capsule",
        "Shielded cells, chain pop",
        RES_SODIUM,
        RES_SODIUM,
        0.0f,
        1u << TRAIT_CAPSULE,
        0u
    };
    game->traits[TRAIT_FLAGELLA] = (TraitDef){
        "Flagella",
        "Fast swimmers, dense spawns",
        RES_GLUCOSE,
        RES_SODIUM,
        20.0f,
        1u << TRAIT_FLAGELLA,
        1u << TRAIT_CAPSULE
    };
    game->traits[TRAIT_PILI] = (TraitDef){
        "Pili",
        "Adhesion, clumping elites",
        RES_IRON,
        RES_GLUCOSE,
        25.0f,
        1u << TRAIT_PILI,
        1u << TRAIT_FLAGELLA
    };
    game->traits[TRAIT_ENDOSPORE] = (TraitDef){
        "Endospores",
        "Dormant shells, delayed drops",
        RES_CALCIUM,
        RES_IRON,
        30.0f,
        1u << TRAIT_ENDOSPORE,
        1u << TRAIT_PILI
    };
    game->traits[TRAIT_LPS] = (TraitDef){
        "LPS Layer",
        "Toxins, pickup pull",
        RES_LIPIDS,
        RES_CALCIUM,
        35.0f,
        1u << TRAIT_LPS,
        1u << TRAIT_ENDOSPORE
    };
    game->traits[TRAIT_PHOTOSYNTH] = (TraitDef){
        "Photosynthesis",
        "Ambient oxygen drip",
        RES_OXYGEN,
        RES_LIPIDS,
        40.0f,
        1u << TRAIT_PHOTOSYNTH,
        1u << TRAIT_LPS
    };
    game->traits[TRAIT_QUORUM] = (TraitDef){
        "Quorum Sensing",
        "Group buffs, pulse waves",
        RES_SIGNALS,
        RES_OXYGEN,
        50.0f,
        1u << TRAIT_QUORUM,
        1u << TRAIT_PHOTOSYNTH
    };

    for (int i = 0; i < TRAIT_COUNT; ++i) {
        game->trait_unlocked[i] = (i == TRAIT_CAPSULE);
    }
}

static void init_upgrades(GameState *game) {
    game->upgrades[RES_SODIUM] = (UpgradeDef){"Chain Radius", "Larger capsule pops", 10.0f, 1.6f};
    game->upgrades[RES_GLUCOSE] = (UpgradeDef){"Hover Damage", "Stronger hover field", 12.0f, 1.6f};
    game->upgrades[RES_IRON] = (UpgradeDef){"Drop Yield", "More per microbe", 14.0f, 1.6f};
    game->upgrades[RES_CALCIUM] = (UpgradeDef){"Spore Armor", "Shorter dormancy", 16.0f, 1.6f};
    game->upgrades[RES_LIPIDS] = (UpgradeDef){"Toxin Reach", "Larger toxin halos", 18.0f, 1.6f};
    game->upgrades[RES_OXYGEN] = (UpgradeDef){"Burn Damage", "Extra ambient DPS", 20.0f, 1.6f};
    game->upgrades[RES_SIGNALS] = (UpgradeDef){"Pulse Cooldown", "Faster pulses", 25.0f, 1.6f};

    for (int i = 0; i < RES_COUNT; ++i) {
        game->upgrade_level[i] = 0;
    }
}

static float upgrade_cost(const GameState *game, ResourceType res) {
    const UpgradeDef *up = &game->upgrades[res];
    return up->base_cost * powf(up->cost_mult, (float)game->upgrade_level[res]);
}

static float trait_drop_amount(const GameState *game, ResourceType res) {
    float base = 1.0f;
    float mult = 1.0f + 0.15f * (float)game->upgrade_level[RES_IRON];
    if (res == RES_SIGNALS) {
        base = 0.6f;
    }
    return base * mult;
}

static bool resource_can_spend(const GameState *game, ResourceType res, float amount) {
    return game->resources[res].unlocked && game->resources[res].amount >= amount;
}

static void resource_spend(GameState *game, ResourceType res, float amount) {
    if (!game->resources[res].unlocked) {
        return;
    }
    game->resources[res].amount = clampf(game->resources[res].amount - amount, 0.0f, 1e9f);
}

static void resource_gain(GameState *game, ResourceType res, float amount) {
    if (!game->resources[res].unlocked) {
        return;
    }
    game->resources[res].amount += amount;
}

static void spawn_zone(GameState *game, Vector3 pos, float radius, float duration, float strength) {
    for (int i = 0; i < MAX_ZONES; ++i) {
        if (!game->zones[i].active) {
            game->zones[i].active = true;
            game->zones[i].pos = pos;
            game->zones[i].radius = radius;
            game->zones[i].timer = duration;
            game->zones[i].strength = strength;
            break;
        }
    }
}

static Microbe make_microbe(GameState *game) {
    Microbe m = {0};
    m.form = (BaseForm)rng_range_i(&game->rng, 0, FORM_COUNT - 1);
    m.size = rng_range(&game->rng, 0.4f, 0.8f);
    m.hp_max = lerpf(6.0f, 12.0f, rng_next_f01(&game->rng));
    m.hp = m.hp_max;
    m.pos = (Vector3){
        rng_range(&game->rng, -12.0f, 12.0f),
        0.0f,
        rng_range(&game->rng, -10.0f, 10.0f)
    };
    m.vel = (Vector3){
        rng_range(&game->rng, -0.5f, 0.5f),
        0.0f,
        rng_range(&game->rng, -0.4f, 0.4f)
    };
    m.wobble = rng_range(&game->rng, 0.0f, PI_F * 2.0f);
    m.phase = rng_range(&game->rng, 0.0f, PI_F * 2.0f);
    m.pulse = rng_range(&game->rng, 0.0f, PI_F * 2.0f);
    m.spin = rng_range(&game->rng, -1.0f, 1.0f);
    m.drift = rng_range(&game->rng, 0.2f, 0.8f);
    m.twist = rng_range(&game->rng, 0.5f, 1.5f);
    m.appendages = rng_range_i(&game->rng, 4, 9);
    m.traits = 0u;
    for (int i = 0; i < TRAIT_COUNT; ++i) {
        if (game->trait_unlocked[i]) {
            float chance = 0.25f + 0.05f * (float)i;
            if (rng_next_f01(&game->rng) < chance || i == TRAIT_CAPSULE) {
                m.traits |= (1u << i);
            }
        }
    }
    if (m.traits == 0u) {
        m.traits |= (1u << TRAIT_CAPSULE);
    }
    return m;
}

static void push_microbe(GameState *game) {
    if (game->microbe_count >= MAX_MICROBES) {
        return;
    }
    game->microbes[game->microbe_count++] = make_microbe(game);
}

static void apply_damage(GameState *game, Microbe *m, float damage) {
    if (m->dormant) {
        return;
    }
    float shield = 1.0f;
    if (m->traits & (1u << TRAIT_QUORUM)) {
        int nearby = 0;
        if (game->microbe_count <= 1500) {
            for (int i = 0; i < game->microbe_count; ++i) {
                Microbe *other = &game->microbes[i];
                if (other == m || other->dormant) {
                    continue;
                }
                if (vec3_dist_sq(other->pos, m->pos) < (2.6f * 2.6f)) {
                    nearby++;
                }
            }
            if (nearby >= 3) {
                shield = 0.65f;
            }
        }
    }
    m->hp -= damage * shield;
}

static void on_microbe_kill(GameState *game, Microbe *m) {
    if (m->traits & (1u << TRAIT_ENDOSPORE)) {
        m->dormant = true;
        m->dormant_timer = 2.5f - 0.15f * (float)game->upgrade_level[RES_CALCIUM];
        m->dormant_timer = clampf(m->dormant_timer, 0.6f, 3.0f);
        m->hp = 1.0f;
        return;
    }

    for (int i = 0; i < TRAIT_COUNT; ++i) {
        if (m->traits & (1u << i)) {
            ResourceType res = game->traits[i].resource;
            resource_gain(game, res, trait_drop_amount(game, res));
        }
    }

    if (m->traits & (1u << TRAIT_CAPSULE)) {
        float radius = 1.6f + 0.2f * (float)game->upgrade_level[RES_SODIUM];
        for (int i = 0; i < game->microbe_count; ++i) {
            Microbe *other = &game->microbes[i];
            if (other == m) {
                continue;
            }
            float d2 = vec3_dist_sq(other->pos, m->pos);
            if (d2 < radius * radius) {
                apply_damage(game, other, 3.0f);
            }
        }
    }

    if (m->traits & (1u << TRAIT_LPS)) {
        float radius = 2.2f + 0.2f * (float)game->upgrade_level[RES_LIPIDS];
        spawn_zone(game, m->pos, radius, 4.0f, 0.7f);
    }

    *m = game->microbes[game->microbe_count - 1];
    game->microbe_count--;
}

static void update_zones(GameState *game, float dt) {
    for (int i = 0; i < MAX_ZONES; ++i) {
        if (!game->zones[i].active) {
            continue;
        }
        game->zones[i].timer -= dt;
        if (game->zones[i].timer <= 0.0f) {
            game->zones[i].active = false;
        }
    }
}

static float hover_damage_multiplier(const GameState *game, Vector3 hover_pos) {
    float mult = 1.0f;
    for (int i = 0; i < MAX_ZONES; ++i) {
        if (!game->zones[i].active) {
            continue;
        }
        float d2 = vec3_dist_sq(hover_pos, game->zones[i].pos);
        if (d2 < game->zones[i].radius * game->zones[i].radius) {
            mult *= game->zones[i].strength;
        }
    }
    return mult;
}

static bool mouse_pick_microbe(GameState *game, Camera3D camera, Vector3 *out_pos) {
    Ray ray = GetMouseRay(GetMousePosition(), camera);
    float best_dist = 1e9f;
    int best_idx = -1;

    for (int i = 0; i < game->microbe_count; ++i) {
        Microbe *m = &game->microbes[i];
        float radius = m->size * 0.9f;
        RayCollision hit = GetRayCollisionSphere(ray, m->pos, radius);
        if (hit.hit && hit.distance < best_dist) {
            best_dist = hit.distance;
            best_idx = i;
        }
    }

    game->hovered_index = best_idx;
    if (best_idx >= 0 && out_pos) {
        *out_pos = game->microbes[best_idx].pos;
        return true;
    }
    return false;
}

void game_init(GameState *game, uint64_t seed) {
    memset(game, 0, sizeof(*game));
    rng_seed(&game->rng, seed);
    init_resources(game);
    init_traits(game);
    init_upgrades(game);

    game->spawn_rate = 1.4f;
    game->hover_damage = 4.0f;
    game->click_damage = 10.0f;
    game->aoe_cooldown = 6.0f;
    game->aoe_timer = 0.0f;
    game->pulse_timer = 0.0f;

    for (int i = 0; i < 20; ++i) {
        push_microbe(game);
    }
}

GameState *game_create(uint64_t seed) {
    GameState *game = (GameState *)malloc(sizeof(GameState));
    if (!game) {
        return NULL;
    }
    game_init(game, seed);
    return game;
}

void game_destroy(GameState *game) {
    free(game);
}

void game_update_fixed(GameState *game, float dt) {
    game->spawn_timer += dt;
    float spawn_interval = 1.0f / game->spawn_rate;
    while (game->spawn_timer >= spawn_interval) {
        game->spawn_timer -= spawn_interval;
        push_microbe(game);
    }

    if (game->aoe_timer > 0.0f) {
        game->aoe_timer -= dt;
    }
    if (game->pulse_timer > 0.0f) {
        game->pulse_timer -= dt;
    }

    int photosynth_count = 0;

    for (int i = 0; i < game->microbe_count; ++i) {
        Microbe *m = &game->microbes[i];
        m->wobble += dt * (1.2f + 0.4f * m->twist);
        m->phase += dt * (0.8f + 0.6f * m->drift);
        m->pulse += dt * (1.4f + 0.3f * m->twist);
        m->squish -= dt * 1.2f;
        if (m->squish < 0.0f) {
            m->squish = 0.0f;
        }

        if (m->dormant) {
            m->dormant_timer -= dt;
            if (m->dormant_timer <= 0.0f) {
                resource_gain(game, RES_CALCIUM, trait_drop_amount(game, RES_CALCIUM));
                *m = game->microbes[game->microbe_count - 1];
                game->microbe_count--;
                i--;
                continue;
            }
        }

        Vector3 vel = m->vel;
        float speed = 1.0f;
        if (m->traits & (1u << TRAIT_FLAGELLA)) {
            speed += 0.6f;
        }
        if (m->traits & (1u << TRAIT_PHOTOSYNTH)) {
            photosynth_count++;
        }

        if (m->traits & (1u << TRAIT_PILI) && game->microbe_count <= 1500) {
            int target = -1;
            float best = 2.6f;
            for (int j = 0; j < game->microbe_count; ++j) {
                if (i == j) {
                    continue;
                }
                float d2 = vec3_dist_sq(m->pos, game->microbes[j].pos);
                if (d2 < best * best) {
                    best = sqrtf(d2);
                    target = j;
                }
            }
            if (target >= 0) {
                Vector3 dir = Vector3Subtract(game->microbes[target].pos, m->pos);
                float len = Vector3Length(dir);
                if (len > 0.001f) {
                    dir = Vector3Scale(dir, 1.0f / len);
                    vel = Vector3Add(vel, Vector3Scale(dir, 0.5f));
                }
            }
        }

        float swirl = sinf(m->phase + m->pos.x * 0.35f) * 0.25f;
        float drift = cosf(m->phase * 0.9f + m->pos.z * 0.3f) * 0.25f;
        vel.x += swirl;
        vel.z += drift;

        float dist = sqrtf(m->pos.x * m->pos.x + m->pos.z * m->pos.z);
        if (dist > 12.0f) {
            Vector3 to_center = Vector3Normalize((Vector3){-m->pos.x, 0.0f, -m->pos.z});
            vel = Vector3Add(vel, Vector3Scale(to_center, 0.5f));
        }

        m->pos.x += vel.x * speed * dt;
        m->pos.z += vel.z * speed * dt;

        if (m->pos.x < -13.0f || m->pos.x > 13.0f) {
            m->vel.x *= -1.0f;
        }
        if (m->pos.z < -11.0f || m->pos.z > 11.0f) {
            m->vel.z *= -1.0f;
        }
    }

    resolve_collisions(game, dt);

    if (game->resources[RES_OXYGEN].unlocked && photosynth_count > 0) {
        float burn = 0.4f + 0.1f * (float)game->upgrade_level[RES_OXYGEN];
        resource_gain(game, RES_OXYGEN, dt * 0.25f * (float)photosynth_count);

        for (int i = 0; i < game->microbe_count; ++i) {
            if (game->microbes[i].traits & (1u << TRAIT_LPS)) {
                continue;
            }
            apply_damage(game, &game->microbes[i], burn * dt);
        }
    }

    update_zones(game, dt);
    cleanup_dead(game);
}

static void draw_internal_granules(const Microbe *m, float t, int detail, bool photosynth) {
    int count = (detail >= 2) ? 6 : 3;
    for (int i = 0; i < count; ++i) {
        float seed = m->phase * 10.0f + (float)i * 3.1f;
        float angle = hashf(seed) * PI_F * 2.0f;
        float radius = hashf(seed + 1.7f) * m->size * 0.5f;
        float bob = sinf(t * 1.8f + seed) * 0.04f;
        Vector3 p = {
            m->pos.x + cosf(angle) * radius,
            m->pos.y + bob,
            m->pos.z + sinf(angle) * radius
        };
        Color c = photosynth ? (Color){120, 220, 140, 160} : (Color){210, 220, 255, 150};
        DrawSphere(p, m->size * 0.06f, c);
    }

    if (detail >= 2) {
        float ring = 0.28f + 0.04f * sinf(t * 2.0f + m->pulse);
        DrawSphereWires(m->pos, m->size * ring, 8, 8, (Color){160, 190, 220, 120});
        float vacuole = 0.22f + 0.03f * sinf(t * 1.4f + m->phase);
        DrawSphere(m->pos, m->size * vacuole, (Color){190, 210, 230, 50});
        Vector3 nucleus = {m->pos.x + m->size * 0.18f, m->pos.y, m->pos.z - m->size * 0.12f};
        DrawSphere(nucleus, m->size * 0.12f, (Color){150, 170, 210, 140});
    }
}

static void draw_disc(Vector3 pos, float radius, float thickness, Color color) {
    Vector3 top = {pos.x, pos.y + thickness * 0.5f, pos.z};
    Vector3 bot = {pos.x, pos.y - thickness * 0.5f, pos.z};
    DrawCylinderEx(top, bot, radius, radius, 16, color);
}

static void draw_outline(Vector3 pos, float radius, Color color) {
    DrawCircle3D(pos, radius, (Vector3){1.0f, 0.0f, 0.0f}, 90.0f, color);
    DrawCircle3D(pos, radius, (Vector3){0.0f, 0.0f, 1.0f}, 90.0f, color);
}

static void draw_capsule_body(Vector3 start, Vector3 end, float radius, Color color) {
    DrawCapsule(start, end, radius, 12, 6, color);
}

static void draw_dna_strand(const Microbe *m, float t) {
    int points = 12;
    float length = m->size * 0.55f;
    float pitch = m->size * 0.12f;
    float phase = t * 1.2f + m->phase;
    Color strand = (Color){200, 220, 255, 180};
    for (int i = 0; i < points - 1; ++i) {
        float step = (float)i / (float)(points - 1);
        float offset = (step - 0.5f) * length;
        float wave = sinf(phase + step * PI_F * 2.0f) * m->size * 0.1f;
        Vector3 a = {m->pos.x + wave, m->pos.y, m->pos.z + offset};
        Vector3 b = {m->pos.x + sinf(phase + (step + 1.0f / (points - 1)) * PI_F * 2.0f) * m->size * 0.14f,
                     m->pos.y, m->pos.z + offset + length / (points - 1)};
        DrawLine3D(a, b, strand);
        DrawSphere(a, m->size * 0.05f, strand);
        if ((i % 2) == 0) {
            Vector3 rung = {m->pos.x - wave, m->pos.y, m->pos.z + offset + pitch};
            DrawLine3D(a, rung, (Color){180, 200, 230, 120});
        }
    }
}

static void draw_microbe_form(const Microbe *m, Color base_color, float t, int detail) {
    Vector3 pos = m->pos;
    float size = m->size;
    bool photosynth = (m->traits & (1u << TRAIT_PHOTOSYNTH)) != 0u;
    float squish = clampf(m->squish, 0.0f, 1.2f);
    float thickness = size * (0.16f + squish * 0.16f);
    Color body = (Color){base_color.r, base_color.g, base_color.b, 90};
    Color edge = (Color){(unsigned char)(base_color.r * 0.6f),
                         (unsigned char)(base_color.g * 0.6f),
                         (unsigned char)(base_color.b * 0.6f), 200};

    if (detail >= 1) {
        (void)photosynth;
        draw_dna_strand(m, t);
    }

    switch (m->form) {
        case FORM_COCCUS:
            draw_disc(pos, size, thickness, body);
            if (detail > 0) draw_outline(pos, size * 1.01f, edge);
            break;
        case FORM_BACILLUS: {
            Vector3 start = {pos.x - size * 0.9f, pos.y, pos.z};
            Vector3 end = {pos.x + size * 0.9f, pos.y, pos.z};
            draw_capsule_body(start, end, size * 0.45f, body);
            if (detail > 0) {
                draw_outline(start, size * 0.48f, edge);
                draw_outline(end, size * 0.48f, edge);
            }
        } break;
        case FORM_VIBRIO: {
            Vector3 start = {pos.x - size * 0.7f, pos.y, pos.z - size * 0.3f};
            Vector3 end = {pos.x + size * 0.7f, pos.y, pos.z + size * 0.3f};
            draw_capsule_body(start, end, size * 0.42f, body);
            if (detail > 0) {
                draw_outline(start, size * 0.48f, edge);
                draw_outline(end, size * 0.48f, edge);
            }
        } break;
        case FORM_SPIRILLUM: {
            Vector3 start = {pos.x - size * 0.9f, pos.y, pos.z};
            Vector3 end = {pos.x + size * 0.9f, pos.y, pos.z};
            draw_capsule_body(start, end, size * 0.38f, body);
            for (int i = 0; i < 6; ++i) {
                float step = (float)i / 5.0f;
                float wave = sinf(t * 2.0f + m->phase + step * PI_F * 2.0f) * size * 0.25f;
                Vector3 a = {pos.x - size * 0.9f + step * size * 1.8f, pos.y, pos.z + wave};
                Vector3 b = {a.x + size * 0.15f, a.y, a.z + wave * 0.25f};
                DrawLine3D(a, b, edge);
            }
        } break;
        case FORM_FILAMENT: {
            Vector3 start = {pos.x - size * 1.2f, pos.y, pos.z};
            Vector3 end = {pos.x + size * 1.2f, pos.y, pos.z};
            draw_capsule_body(start, end, size * 0.28f, body);
            for (int i = 0; i < 5; ++i) {
                float step = (float)i / 4.0f;
                float wave = sinf(t * 1.6f + m->phase + step * PI_F * 2.0f) * size * 0.22f;
                Vector3 a = {pos.x - size * 1.2f + step * size * 2.4f, pos.y, pos.z + wave};
                Vector3 b = {a.x + size * 0.18f, a.y, a.z + wave * 0.2f};
                DrawLine3D(a, b, edge);
            }
        } break;
        case FORM_STELLATE: {
            draw_disc(pos, size * 0.5f, thickness, body);
            for (int i = 0; i < m->appendages; ++i) {
                float angle = (PI_F * 2.0f / (float)m->appendages) * (float)i;
                float length = size * (1.0f + 0.25f * sinf(t * 2.2f + angle));
                Vector3 tip = {pos.x + cosf(angle) * length, pos.y, pos.z + sinf(angle) * length};
                DrawCylinderEx(pos, tip, size * 0.08f, size * 0.02f, 6, body);
            }
        } break;
        case FORM_CLUSTER: {
            draw_disc(pos, size * 0.55f, thickness, body);
            for (int i = 0; i < 4; ++i) {
                float angle = (PI_F * 2.0f / 4.0f) * (float)i + m->phase;
                Vector3 p = {pos.x + cosf(angle) * size * 0.4f, pos.y, pos.z + sinf(angle) * size * 0.4f};
                DrawLine3D(pos, p, edge);
            }
        } break;
        case FORM_DIATOM: {
            draw_disc(pos, size * 0.9f, thickness * 0.6f, body);
            for (int i = 0; i < 8; ++i) {
                float angle = (PI_F * 2.0f / 8.0f) * (float)i + m->phase;
                Vector3 rim = {pos.x + cosf(angle) * size * 0.8f, pos.y, pos.z + sinf(angle) * size * 0.8f};
                DrawLine3D(pos, rim, (Color){200, 210, 230, 120});
            }
            if (detail > 0) draw_outline(pos, size * 0.9f, (Color){180, 170, 120, 220});
        } break;
        case FORM_AMOEBA: {
            draw_disc(pos, size * 0.6f, thickness, body);
            for (int i = 0; i < 6; ++i) {
                float angle = (PI_F * 2.0f / 6.0f) * (float)i + m->phase;
                float stretch = 0.6f + 0.3f * sinf(t * 1.1f + angle);
                Vector3 spike = {pos.x + cosf(angle) * size * 0.6f, pos.y, pos.z + sinf(angle) * size * 0.6f};
                Vector3 tip = {pos.x + cosf(angle) * size * (0.75f + stretch * 0.2f), pos.y, pos.z + sinf(angle) * size * (0.75f + stretch * 0.2f)};
                DrawLine3D(spike, tip, edge);
            }
        } break;
        case FORM_FLAGELLATE: {
            Vector3 head = {pos.x + size * 0.25f, pos.y, pos.z};
            Vector3 tail = {pos.x - size * 0.65f, pos.y, pos.z};
            draw_capsule_body(tail, head, size * 0.42f, body);
            Vector3 prev = {pos.x - size * 0.7f, pos.y, pos.z};
            for (int i = 1; i <= 6; ++i) {
                float step = (float)i / 6.0f;
                float wave = sinf(t * 6.0f + m->phase + step * 8.0f) * size * 0.25f;
                Vector3 next = {pos.x - size * (0.7f + step * 1.2f), pos.y, pos.z + wave};
                DrawLine3D(prev, next, (Color){170, 220, 255, 200});
                prev = next;
            }
            if (detail > 0) {
                Vector3 band = {pos.x + size * 0.1f, pos.y, pos.z};
                DrawLine3D((Vector3){band.x, band.y, band.z - size * 0.3f},
                           (Vector3){band.x, band.y, band.z + size * 0.3f},
                           (Color){120, 200, 140, 160});
            }
        } break;
        default:
            draw_disc(pos, size, thickness, body);
            break;
    }

    (void)photosynth;
}

static void draw_microbe_traits(const Microbe *m, float t) {
    if (m->traits & (1u << TRAIT_CAPSULE)) {
        float pulse = 0.08f * sinf(t * 1.6f + m->pulse);
        draw_outline(m->pos, m->size * (1.2f + pulse), (Color){120, 190, 255, 120});
    }
    if (m->traits & (1u << TRAIT_FLAGELLA)) {
        int segments = 6;
        Vector3 prev = {m->pos.x - m->size * 0.6f, m->pos.y, m->pos.z};
        for (int i = 1; i <= segments; ++i) {
            float step = (float)i / (float)segments;
            float wave = sinf(t * 6.0f + m->phase + step * 6.0f) * 0.25f;
            Vector3 next = {
                m->pos.x - m->size * (0.6f + step * 1.2f),
                m->pos.y + wave * 0.1f,
                m->pos.z + wave * 0.4f
            };
            DrawLine3D(prev, next, (Color){180, 255, 200, 200});
            prev = next;
        }
    }
    if (m->traits & (1u << TRAIT_PILI)) {
        int count = m->appendages;
        for (int i = 0; i < count; ++i) {
            float angle = (PI_F * 2.0f / (float)count) * (float)i + m->phase;
            float twitch = 0.2f * sinf(t * 5.0f + angle * 2.0f);
            float length = m->size * (1.05f + twitch);
            Vector3 spike = {m->pos.x + cosf(angle) * length, m->pos.y, m->pos.z + sinf(angle) * length};
            DrawLine3D(m->pos, spike, (Color){200, 180, 120, 200});
        }
    }
    if (m->traits & (1u << TRAIT_ENDOSPORE)) {
        float shell = 0.4f + 0.05f * sinf(t * 2.0f + m->pulse);
        DrawSphere(m->pos, m->size * shell, (Color){220, 220, 240, 160});
        draw_outline(m->pos, m->size * 0.7f, (Color){180, 190, 210, 120});
    }
    if (m->traits & (1u << TRAIT_LPS)) {
        float haze = 0.2f + 0.05f * sinf(t * 1.3f + m->phase);
        draw_outline(m->pos, m->size * (1.35f + haze), (Color){255, 170, 90, 110});
        for (int i = 0; i < 4; ++i) {
            float angle = (PI_F * 2.0f / 4.0f) * (float)i + m->phase;
            Vector3 tip = {m->pos.x + cosf(angle) * m->size * 1.8f, m->pos.y, m->pos.z + sinf(angle) * m->size * 1.8f};
            DrawLine3D(m->pos, tip, (Color){255, 140, 80, 120});
        }
    }
    if (m->traits & (1u << TRAIT_PHOTOSYNTH)) {
        float glow = 0.65f + 0.1f * sinf(t * 2.4f + m->pulse);
        DrawSphere(m->pos, m->size * glow, (Color){120, 220, 140, 180});
    }
    if (m->traits & (1u << TRAIT_QUORUM)) {
        float pulse = 0.4f + 0.2f * sinf(t * 3.0f);
        draw_outline(m->pos, m->size * (1.1f + pulse), (Color){180, 140, 255, 120});
    }
}

void game_render(const GameState *game, Camera3D camera, float alpha) {
    (void)alpha;
    BeginMode3D(camera);

    DrawPlane((Vector3){0.0f, -0.2f, 0.0f}, (Vector2){30.0f, 24.0f}, (Color){10, 20, 30, 255});
    DrawGrid(20, 1.0f);

    float t = (float)GetTime();
    for (int i = 0; i < 24; ++i) {
        float seed = (float)i * 12.7f;
        float px = sinf(t * 0.2f + seed) * 12.0f;
        float pz = cosf(t * 0.15f + seed * 1.3f) * 9.5f;
        float py = -0.15f + 0.1f * sinf(t * 1.1f + seed);
        DrawSphere((Vector3){px, py, pz}, 0.08f, (Color){60, 90, 120, 120});
    }

    int detail = (game->microbe_count > 300) ? 0 : 1;

    BeginBlendMode(BLEND_ALPHA);
    for (int i = 0; i < game->microbe_count; ++i) {
        const Microbe *m = &game->microbes[i];
        Color base = (Color){80, 130, 200, 255};
        if (m->traits & (1u << TRAIT_LPS)) {
            base = (Color){210, 130, 60, 255};
        } else if (m->traits & (1u << TRAIT_PHOTOSYNTH)) {
            base = (Color){90, 200, 120, 255};
        } else if (m->form == FORM_DIATOM) {
            base = (Color){200, 190, 120, 255};
        } else if (m->form == FORM_AMOEBA) {
            base = (Color){150, 180, 200, 255};
        } else if (m->form == FORM_FLAGELLATE) {
            base = (Color){140, 200, 180, 255};
        } else if (m->form == FORM_STELLATE) {
            base = (Color){190, 170, 220, 255};
        }

        float tint = 0.75f + 0.25f * sinf(t * 0.8f + m->phase);
        base.r = (unsigned char)clampf(base.r * tint, 30.0f, 255.0f);
        base.g = (unsigned char)clampf(base.g * tint, 30.0f, 255.0f);
        base.b = (unsigned char)clampf(base.b * tint, 30.0f, 255.0f);

        if (m->dormant) {
            base = (Color){200, 200, 210, 255};
        }

        draw_microbe_form(m, base, t + m->wobble, detail);
        if (detail >= 2) {
            draw_microbe_traits(m, t + m->wobble);
        }

        if (i == game->hovered_index) {
            DrawSphereWires(m->pos, m->size * 1.5f, 10, 10, (Color){255, 255, 255, 200});
        }

        if (game->show_bounds) {
            float box = m->size * 2.2f;
            DrawCubeWires(m->pos, box, box, box, (Color){110, 255, 140, 160});
            DrawLine3D((Vector3){m->pos.x, m->pos.y - box * 0.5f, m->pos.z},
                       (Vector3){m->pos.x, m->pos.y + box * 0.5f, m->pos.z},
                       (Color){110, 255, 140, 160});
        }
    }
    EndBlendMode();

    for (int i = 0; i < MAX_ZONES; ++i) {
        if (!game->zones[i].active) {
            continue;
        }
        Color c = (Color){255, 120, 80, 90};
        DrawSphere(game->zones[i].pos, game->zones[i].radius, c);
    }

    EndMode3D();
}

static uint32_t unlocked_trait_mask(const GameState *game) {
    uint32_t mask = 0u;
    for (int i = 0; i < TRAIT_COUNT; ++i) {
        if (game->trait_unlocked[i]) {
            mask |= (1u << i);
        }
    }
    return mask;
}

static void try_unlock_trait(GameState *game, int trait_id) {
    if (trait_id < 0 || trait_id >= TRAIT_COUNT) {
        return;
    }
    if (game->trait_unlocked[trait_id]) {
        return;
    }
    TraitDef *trait = &game->traits[trait_id];
    uint32_t unlocked = unlocked_trait_mask(game);
    if ((trait->prereq_mask & ~unlocked) != 0u) {
        return;
    }
    if (!resource_can_spend(game, trait->cost_resource, trait->cost_amount)) {
        return;
    }
    resource_spend(game, trait->cost_resource, trait->cost_amount);
    game->trait_unlocked[trait_id] = true;
    game->resources[trait->resource].unlocked = true;
    game->spawn_rate += 0.2f;
}

static void try_upgrade(GameState *game, ResourceType res) {
    if (!game->resources[res].unlocked) {
        return;
    }
    float cost = upgrade_cost(game, res);
    if (!resource_can_spend(game, res, cost)) {
        return;
    }
    resource_spend(game, res, cost);
    game->upgrade_level[res] += 1;

    if (res == RES_GLUCOSE) {
        game->hover_damage += 0.6f;
        game->click_damage += 0.8f;
    }
    if (res == RES_SIGNALS) {
        game->aoe_cooldown = clampf(game->aoe_cooldown - 0.2f, 2.0f, 10.0f);
    }
}

void game_render_ui(GameState *game, int screen_w, int screen_h) {
    (void)screen_w;
    const int pad = 14;
    int x = pad;
    int y = pad;

    DrawRectangle(x - 8, y - 8, 320, screen_h - pad * 2, (Color){10, 10, 20, 200});

    DrawText("Micro-Idle", x, y, 20, RAYWHITE);
    y += 28;
    DrawText("Hover to damage, click to burst", x, y, 12, GRAY);
    y += 18;

    Rectangle toggle = { (float)x, (float)y, 280.0f, 20.0f };
    DrawRectangleRec(toggle, (Color){20, 20, 30, 200});
    DrawText(TextFormat("Bounds Overlay (boxes): %s", game->show_bounds ? "ON" : "OFF"), x + 6, y + 3, 12, GRAY);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mp = GetMousePosition();
        if (CheckCollisionPointRec(mp, toggle)) {
            game->show_bounds = !game->show_bounds;
        }
    }
    y += 26;

    for (int i = 0; i < RES_COUNT; ++i) {
        Resource *res = &game->resources[i];
        if (!res->unlocked) {
            continue;
        }
        Rectangle row = { (float)x, (float)y, 280.0f, 20.0f };
        Color c = color_for_resource((ResourceType)i);
        DrawRectangleRec(row, (Color){20, 20, 30, 200});
        DrawText(TextFormat("%s: %.1f", res->name, res->amount), x + 6, y + 3, 12, c);

        float cost = upgrade_cost(game, (ResourceType)i);
        DrawText(TextFormat("U: %s (%.1f)", game->upgrades[i].name, cost), x + 150, y + 3, 10, GRAY);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mp = GetMousePosition();
            if (CheckCollisionPointRec(mp, row)) {
                try_upgrade(game, (ResourceType)i);
            }
        }
        y += 24;
    }

    y += 10;
    DrawText("DNA Traits", x, y, 14, RAYWHITE);
    y += 20;

    for (int i = 0; i < TRAIT_COUNT; ++i) {
        TraitDef *trait = &game->traits[i];
        Rectangle row = { (float)x, (float)y, 280.0f, 20.0f };
        bool unlocked = game->trait_unlocked[i];
        Color text_color = unlocked ? color_for_resource(trait->resource) : GRAY;

        DrawRectangleRec(row, (Color){15, 15, 28, 200});
        const char *status = unlocked ? "(unlocked)" : "";
        DrawText(TextFormat("%d) %s %s", i + 1, trait->name, status), x + 6, y + 3, 12, text_color);
        if (!unlocked) {
            DrawText(TextFormat("cost %.1f %s", trait->cost_amount, game->resources[trait->cost_resource].name), x + 150, y + 3, 10, GRAY);
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mp = GetMousePosition();
            if (CheckCollisionPointRec(mp, row)) {
                try_unlock_trait(game, i);
            }
        }

        if (IsKeyPressed((KeyboardKey)(KEY_ONE + i))) {
            try_unlock_trait(game, i);
        }
        y += 24;
    }

    y += 10;
    DrawText(TextFormat("AOE Blast: Space (%.1fs)", game->aoe_timer > 0.0f ? game->aoe_timer : 0.0f), x, y, 12, GRAY);
    y += 16;
    DrawText("Upgrades: click a resource row", x, y, 12, GRAY);
    y += 16;
    DrawText(TextFormat("Microbes: %d", game->microbe_count), x, y, 12, GRAY);

    DrawRectangle(x - 8, y + 26, 320, 2, (Color){60, 60, 90, 200});
    DrawText("Traits reshape visuals + drops; no cosmetics.", x, screen_h - pad - 16, 10, GRAY);
}

void game_handle_input(GameState *game, Camera3D camera, float dt, int screen_w, int screen_h) {
    (void)screen_w;
    const int pad = 14;
    Rectangle panel = { (float)(pad - 8), (float)(pad - 8), 320.0f, (float)(screen_h - pad * 2) };
    if (IsKeyPressed(KEY_B)) {
        game->show_bounds = !game->show_bounds;
    }
    Vector2 mp = GetMousePosition();
    if (CheckCollisionPointRec(mp, panel)) {
        game->hovered_index = -1;
        game->hover_valid = false;
        return;
    }

    game->hover_valid = mouse_pick_microbe(game, camera, &game->hover_pos);

    if (game->hover_valid) {
        float dps = game->hover_damage;
        dps *= hover_damage_multiplier(game, game->hover_pos);
        if (game->hovered_index >= 0) {
            apply_damage(game, &game->microbes[game->hovered_index], dps * dt);
        }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && game->hovered_index >= 0) {
        apply_damage(game, &game->microbes[game->hovered_index], game->click_damage);
    }

    if (IsKeyPressed(KEY_SPACE) && game->aoe_timer <= 0.0f) {
        game->aoe_timer = game->aoe_cooldown;
        float radius = 3.0f + 0.3f * (float)game->upgrade_level[RES_SIGNALS];
        Vector3 center = game->hover_valid ? game->hover_pos : (Vector3){0.0f, 0.0f, 0.0f};
        for (int i = 0; i < game->microbe_count; ++i) {
            float d2 = vec3_dist_sq(game->microbes[i].pos, center);
            if (d2 < radius * radius) {
                apply_damage(game, &game->microbes[i], 8.0f);
            }
        }
    }

    if (game->trait_unlocked[TRAIT_QUORUM]) {
        if (game->pulse_timer <= 0.0f) {
            game->pulse_timer = 5.0f - 0.3f * (float)game->upgrade_level[RES_SIGNALS];
            for (int i = 0; i < game->microbe_count; ++i) {
                apply_damage(game, &game->microbes[i], 2.0f);
            }
        }
    }
}

static void cleanup_dead(GameState *game) {
    for (int i = 0; i < game->microbe_count; ++i) {
        Microbe *m = &game->microbes[i];
        if (!m->dormant && m->hp <= 0.0f) {
            on_microbe_kill(game, m);
            i--;
        }
    }
}
