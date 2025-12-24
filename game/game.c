#include "game/game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raymath.h"
#include "engine/util/rng.h"

#define PI_F 3.14159265358979323846f

typedef enum BaseForm {
    FORM_COCCUS = 0,
    FORM_BACILLUS,
    FORM_VIBRIO,
    FORM_SPIRILLUM,
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

static float vec3_dist(Vector3 a, Vector3 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
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
        for (int i = 0; i < game->microbe_count; ++i) {
            Microbe *other = &game->microbes[i];
            if (other == m || other->dormant) {
                continue;
            }
            if (vec3_dist(other->pos, m->pos) < 2.6f) {
                nearby++;
            }
        }
        if (nearby >= 3) {
            shield = 0.65f;
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
            float d = vec3_dist(other->pos, m->pos);
            if (d < radius) {
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
        float d = vec3_dist(hover_pos, game->zones[i].pos);
        if (d < game->zones[i].radius) {
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
        m->wobble += dt * 1.4f;

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

        if (m->traits & (1u << TRAIT_PILI)) {
            int target = -1;
            float best = 2.6f;
            for (int j = 0; j < game->microbe_count; ++j) {
                if (i == j) {
                    continue;
                }
                float d = vec3_dist(m->pos, game->microbes[j].pos);
                if (d < best) {
                    best = d;
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

        m->pos.x += vel.x * speed * dt;
        m->pos.z += vel.z * speed * dt;

        if (m->pos.x < -13.0f || m->pos.x > 13.0f) {
            m->vel.x *= -1.0f;
        }
        if (m->pos.z < -11.0f || m->pos.z > 11.0f) {
            m->vel.z *= -1.0f;
        }
    }

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

static void draw_microbe_form(const Microbe *m, Color base_color) {
    Vector3 pos = m->pos;
    float size = m->size;

    switch (m->form) {
        case FORM_COCCUS:
            DrawSphere(pos, size, base_color);
            break;
        case FORM_BACILLUS: {
            Vector3 start = {pos.x - size * 0.8f, pos.y, pos.z};
            Vector3 end = {pos.x + size * 0.8f, pos.y, pos.z};
            DrawCylinderEx(start, end, size * 0.55f, size * 0.55f, 12, base_color);
            DrawSphere(start, size * 0.55f, base_color);
            DrawSphere(end, size * 0.55f, base_color);
        } break;
        case FORM_VIBRIO: {
            Vector3 start = {pos.x - size * 0.7f, pos.y, pos.z - size * 0.3f};
            Vector3 end = {pos.x + size * 0.7f, pos.y, pos.z + size * 0.3f};
            DrawCylinderEx(start, end, size * 0.45f, size * 0.45f, 12, base_color);
            DrawSphere(start, size * 0.45f, base_color);
            DrawSphere(end, size * 0.45f, base_color);
        } break;
        case FORM_SPIRILLUM: {
            for (int i = -2; i <= 2; ++i) {
                Vector3 p = {pos.x + i * size * 0.35f, pos.y, pos.z + sinf((float)i) * size * 0.25f};
                DrawSphere(p, size * 0.32f, base_color);
            }
        } break;
        default:
            DrawSphere(pos, size, base_color);
            break;
    }
}

static void draw_microbe_traits(const Microbe *m, float t) {
    if (m->traits & (1u << TRAIT_CAPSULE)) {
        DrawSphereWires(m->pos, m->size * 1.2f, 8, 8, (Color){120, 190, 255, 120});
    }
    if (m->traits & (1u << TRAIT_FLAGELLA)) {
        Vector3 tail = {m->pos.x - m->size * 1.2f, m->pos.y, m->pos.z};
        Vector3 wave = {tail.x - sinf(t * 4.0f) * 0.4f, tail.y, tail.z + cosf(t * 4.0f) * 0.4f};
        DrawLine3D(tail, wave, (Color){180, 255, 200, 200});
    }
    if (m->traits & (1u << TRAIT_PILI)) {
        for (int i = 0; i < 6; ++i) {
            float angle = (PI_F * 2.0f / 6.0f) * (float)i;
            Vector3 spike = {m->pos.x + cosf(angle) * m->size * 1.2f, m->pos.y, m->pos.z + sinf(angle) * m->size * 1.2f};
            DrawLine3D(m->pos, spike, (Color){200, 180, 120, 200});
        }
    }
    if (m->traits & (1u << TRAIT_ENDOSPORE)) {
        DrawSphere(m->pos, m->size * 0.45f, (Color){220, 220, 240, 220});
    }
    if (m->traits & (1u << TRAIT_LPS)) {
        DrawSphereWires(m->pos, m->size * 1.35f, 10, 10, (Color){255, 170, 90, 140});
    }
    if (m->traits & (1u << TRAIT_PHOTOSYNTH)) {
        DrawSphere(m->pos, m->size * 0.7f, (Color){120, 220, 140, 180});
    }
    if (m->traits & (1u << TRAIT_QUORUM)) {
        float pulse = 0.4f + 0.2f * sinf(t * 3.0f);
        DrawSphereWires(m->pos, m->size * (1.1f + pulse), 8, 8, (Color){180, 140, 255, 150});
    }
}

void game_render(const GameState *game, Camera3D camera, float alpha) {
    (void)alpha;
    BeginMode3D(camera);

    DrawPlane((Vector3){0.0f, -0.2f, 0.0f}, (Vector2){30.0f, 24.0f}, (Color){10, 20, 30, 255});
    DrawGrid(20, 1.0f);

    float t = (float)GetTime();

    for (int i = 0; i < game->microbe_count; ++i) {
        const Microbe *m = &game->microbes[i];
        Color base = (Color){80, 130, 200, 255};
        if (m->traits & (1u << TRAIT_LPS)) {
            base = (Color){210, 130, 60, 255};
        } else if (m->traits & (1u << TRAIT_PHOTOSYNTH)) {
            base = (Color){90, 200, 120, 255};
        }

        if (m->dormant) {
            base = (Color){200, 200, 210, 255};
        }

        draw_microbe_form(m, base);
        draw_microbe_traits(m, t + m->wobble);

        if (i == game->hovered_index) {
            DrawSphereWires(m->pos, m->size * 1.5f, 10, 10, (Color){255, 255, 255, 200});
        }
    }

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
            float d = vec3_dist(game->microbes[i].pos, center);
            if (d < radius) {
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
