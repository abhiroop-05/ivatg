#include "wanted.h"
#include "physics.h"
#include <stdlib.h>
#include <math.h>

void wanted_init(Entity* player) {
    player->w.stars = 0;
    player->w.timer = 0;
    player->w.last_crime_tick = -1000000;
    player->w.last_sighted_tick = -1000000;
    player->w.last_known_x = player->t.x;
    player->w.last_known_y = player->t.y;
}

void wanted_report_crime(Entity* player, CrimeType c, int tick) {
    int delta = 0;
    switch (c) {
        case CRIME_RAM_PED:    delta = WANTED_RAM_DELTA; break;
        case CRIME_ATTACK_PED: delta = WANTED_ATTACK_DELTA; break;
        case CRIME_KILL_PED:   delta = WANTED_KILL_DELTA; break;
        case CRIME_SHOOT_NPC:  delta = WANTED_KILL_DELTA; break;
        default: break;
    }
    player->w.stars += delta;
    if (player->w.stars > WANTED_MAX) player->w.stars = WANTED_MAX;
    if (delta > 0) player->w.last_crime_tick = tick;
}

static void detect_ram_crimes(EntityPool* pool, Entity* player, int tick) {
    /* Any dead civilian near player vehicle within last tick counts as ram */
    /* Simpler: if player is in a vehicle moving fast and any civ is overlapping, charge */
    if (player->vehicle < 0) return;
    Entity* veh = entity_get(pool, player->vehicle);
    if (!veh) return;
    float sp = sqrtf(veh->p.vx*veh->p.vx + veh->p.vy*veh->p.vy);
    if (sp < 2.0f) return;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* e = &pool->entities[i];
        if (!e->active) continue;
        if (e->type != AGENT_CIVILIAN && e->type != AGENT_POLICE) continue;
        float d2 = phys_dist2(e->t.x, e->t.y, veh->t.x, veh->t.y);
        float rr = veh->p.radius + e->p.radius + 0.1f;
        if (d2 < rr * rr) {
            if (e->h.alive) {
                wanted_report_crime(player, CRIME_RAM_PED, tick);
            } else if (tick - player->w.last_crime_tick > 5) {
                wanted_report_crime(player, CRIME_KILL_PED, tick);
            }
        }
    }
}

void wanted_tick(EntityPool* pool, World* w, Entity* player, int tick) {
    (void)w;
    detect_ram_crimes(pool, player, tick);

    /* Auto-trigger wanted when kill threshold reached */
    if (player->gun.npc_kills >= WANTED_SHOOT_KILL_THRESHOLD && player->w.stars < 2) {
        player->w.stars = 2;
        player->w.last_crime_tick = tick;
    }

    /* Check if any police can see player (updates last_known_pos + last_sighted) */
    int seen = 0;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* p = &pool->entities[i];
        if (!p->active || p->type != AGENT_POLICE) continue;
        float d2 = phys_dist2(p->t.x, p->t.y, player->t.x, player->t.y);
        if (d2 < POLICE_SIGHT_RANGE * POLICE_SIGHT_RANGE) {
            seen = 1;
            player->w.last_known_x = player->t.x;
            player->w.last_known_y = player->t.y;
            player->w.last_sighted_tick = tick;
            break;
        }
    }

    /* Decay: if not seen for WANTED_DECAY_TICKS, drop a star */
    if (!seen && player->w.stars > 0) {
        if (tick - player->w.last_sighted_tick > WANTED_DECAY_TICKS) {
            player->w.stars--;
            player->w.last_sighted_tick = tick; /* reset decay timer */
        }
    }
}

void wanted_spawn_police(EntityPool* pool, World* w, Entity* player, int tick) {
    static int last_spawn_tick = -100000;
    if (player->w.stars <= 0) return;
    if (tick - last_spawn_tick < POLICE_SPAWN_INTERVAL_TICKS) return;

    int target = player->w.stars * MAX_POLICE_PER_STAR;
    int current = 0;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* e = &pool->entities[i];
        if (e->active && e->type == AGENT_POLICE && e->h.alive) current++;
    }
    if (current >= target) return;

    /* Pick a road tile far from player but in bounds */
    int tries = 64;
    while (tries-- > 0) {
        if (w->road_count <= 0) return;
        int r = rand() % w->road_count;
        int rx = w->road_xs[r];
        int ry = w->road_ys[r];
        float d2 = phys_dist2(player->t.x, player->t.y, rx + 0.5f, ry + 0.5f);
        if (d2 > 25.0f * 25.0f && d2 < 60.0f * 60.0f) {
            Entity* cop = entity_create(pool, AGENT_POLICE, rx + 0.5f, ry + 0.5f);
            if (cop) {
                last_spawn_tick = tick;
            }
            return;
        }
    }
}
