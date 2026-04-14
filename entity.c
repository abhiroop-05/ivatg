#include "entity.h"
#include <string.h>

void entity_pool_init(EntityPool* pool) {
    memset(pool, 0, sizeof(*pool));
    for (int i = 0; i < MAX_ENTITIES; i++) {
        pool->entities[i].id = i;
        pool->entities[i].active = false;
        pool->entities[i].vehicle = -1;
    }
    pool->count = 0;
    pool->next_free_hint = 0;
}

Entity* entity_create(EntityPool* pool, AgentType type, float x, float y) {
    int start = pool->next_free_hint;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        int idx = (start + i) % MAX_ENTITIES;
        Entity* e = &pool->entities[idx];
        if (!e->active) {
            memset(&e->t, 0, sizeof(e->t));
            memset(&e->p, 0, sizeof(e->p));
            memset(&e->h, 0, sizeof(e->h));
            memset(&e->v, 0, sizeof(e->v));
            memset(&e->w, 0, sizeof(e->w));
            memset(&e->ai, 0, sizeof(e->ai));
            memset(&e->gun, 0, sizeof(e->gun));

            e->id = idx;
            e->active = true;
            e->type = type;
            e->t.x = x;
            e->t.y = y;
            e->t.z = 0;
            e->t.vz = 0;
            e->t.heading = 0;
            e->t.speed = 0;
            e->p.friction = PED_FRICTION;
            e->p.radius = PED_RADIUS;
            e->h.alive = true;
            e->h.hp = e->h.max_hp = 100;
            e->v.driver = -1;
            for (int k = 0; k < 3; k++) e->v.passengers[k] = -1;
            e->v.passenger_count = 0;
            e->ai.state = AI_IDLE;
            e->ai.target = -1;
            e->ai.path_len = 0;
            e->ai.path_idx = 0;
            e->ai.replan_cooldown = 0;
            e->ai.attack_cooldown = 0;
            e->vehicle = -1;
            e->color = 7;
            e->glyph = '?';

            /* Weapon state (player only, but init for all) */
            e->gun.ammo_in_mag = PISTOL_MAG_SIZE;
            e->gun.mag_size = PISTOL_MAG_SIZE;
            e->gun.reloading = false;
            e->gun.reload_timer = 0;
            e->gun.shoot_cooldown = 0;
            e->gun.npc_kills = 0;
            e->gun.muzzle_flash = 0;

            if (type == AGENT_VEHICLE) {
                e->p.friction = VEH_FRICTION;
                e->p.radius = VEH_RADIUS;
                e->h.hp = e->h.max_hp = 200;
                e->glyph = 'V';
                e->color = 6;
            } else if (type == AGENT_PLAYER) {
                e->glyph = '@';
                e->color = 3;
            } else if (type == AGENT_CIVILIAN) {
                e->glyph = 'c';
                e->color = 2;
            } else if (type == AGENT_POLICE) {
                e->glyph = 'P';
                e->color = 4;
                e->ai.state = AI_PATROL;
            }

            pool->count++;
            pool->next_free_hint = (idx + 1) % MAX_ENTITIES;
            return e;
        }
    }
    return NULL;
}

void entity_destroy(EntityPool* pool, int32_t id) {
    if (id < 0 || id >= MAX_ENTITIES) return;
    if (!pool->entities[id].active) return;
    pool->entities[id].active = false;
    pool->count--;
}

Entity* entity_get(EntityPool* pool, int32_t id) {
    if (id < 0 || id >= MAX_ENTITIES) return NULL;
    if (!pool->entities[id].active) return NULL;
    return &pool->entities[id];
}
