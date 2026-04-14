#include "physics.h"
#include <math.h>
#include <stdlib.h>

float phys_dist2(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1, dy = y2 - y1;
    return dx * dx + dy * dy;
}
float phys_dist(float x1, float y1, float x2, float y2) {
    return sqrtf(phys_dist2(x1, y1, x2, y2));
}

static void integrate(Entity* e, float dt) {
    e->p.vx += e->p.ax * dt;
    e->p.vy += e->p.ay * dt;
    /* friction */
    float f = 1.0f - e->p.friction * dt;
    if (f < 0) f = 0;
    e->p.vx *= f;
    e->p.vy *= f;
    e->t.x += e->p.vx * dt;
    e->t.y += e->p.vy * dt;
    /* reset acceleration each tick (applied by controllers) */
    e->p.ax = 0;
    e->p.ay = 0;

    /* Z-axis (jump / gravity) */
    if (e->t.z > 0 || e->t.vz > 0) {
        e->t.z += e->t.vz * dt;
        e->t.vz -= PED_GRAVITY * dt;
        if (e->t.z <= 0) {
            e->t.z = 0;
            e->t.vz = 0;
        }
    }
}

static void resolve_tile_collision(Entity* e, World* w) {
    float r = e->p.radius;
    /* Check 3x3 neighborhood of tiles for blocking and AABB-resolve */
    int cx = (int)e->t.x;
    int cy = (int)e->t.y;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int tx = cx + dx, ty = cy + dy;
            if (!world_is_blocking(w, tx, ty)) continue;
            /* AABB of tile: [tx, tx+1] x [ty, ty+1] */
            float nearest_x = e->t.x < tx ? tx : (e->t.x > tx + 1 ? tx + 1 : e->t.x);
            float nearest_y = e->t.y < ty ? ty : (e->t.y > ty + 1 ? ty + 1 : e->t.y);
            float dxp = e->t.x - nearest_x;
            float dyp = e->t.y - nearest_y;
            float d2 = dxp * dxp + dyp * dyp;
            if (d2 < r * r) {
                float d = sqrtf(d2);
                if (d < 1e-5f) {
                    /* push out along whichever axis is less penetrated */
                    e->t.x += (e->t.x < tx + 0.5f) ? -0.1f : 0.1f;
                    continue;
                }
                float push = (r - d);
                float nx = dxp / d, ny = dyp / d;
                e->t.x += nx * push;
                e->t.y += ny * push;
                /* reflect/cancel velocity along normal */
                float vn = e->p.vx * nx + e->p.vy * ny;
                if (vn < 0) {
                    e->p.vx -= vn * nx;
                    e->p.vy -= vn * ny;
                    /* cancel forward speed for vehicle */
                    e->t.speed *= 0.3f;
                }
            }
        }
    }
}

static void resolve_entity_collisions(EntityPool* pool, World* w) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* a = &pool->entities[i];
        if (!a->active || !a->h.alive) continue;
        int32_t nearby[64];
        int n = world_spatial_query(w, a->t.x, a->t.y, 2.5f, nearby, 64);
        for (int k = 0; k < n; k++) {
            int32_t jid = nearby[k];
            if (jid <= i) continue;
            Entity* b = &pool->entities[jid];
            if (!b->active || !b->h.alive) continue;
            /* Skip if one entity is inside the other as driver/passenger */
            if (a->vehicle == b->id || b->vehicle == a->id) continue;

            float dx = b->t.x - a->t.x;
            float dy = b->t.y - a->t.y;
            float rr = a->p.radius + b->p.radius;
            float d2 = dx * dx + dy * dy;
            if (d2 < rr * rr && d2 > 1e-6f) {
                float d = sqrtf(d2);
                float push = (rr - d) * 0.5f;
                float nx = dx / d, ny = dy / d;

                /* mass-weighted: vehicles push peds */
                float wa = (a->type == AGENT_VEHICLE) ? 0.1f : 1.0f;
                float wb = (b->type == AGENT_VEHICLE) ? 0.1f : 1.0f;
                float wsum = wa + wb;
                a->t.x -= nx * push * (wa / wsum) * 2.0f;
                a->t.y -= ny * push * (wa / wsum) * 2.0f;
                b->t.x += nx * push * (wb / wsum) * 2.0f;
                b->t.y += ny * push * (wb / wsum) * 2.0f;

                /* Collision: vehicle ramming a pedestrian damages pedestrian. */
                if (a->type == AGENT_VEHICLE && b->type != AGENT_VEHICLE) {
                    float sp = sqrtf(a->p.vx * a->p.vx + a->p.vy * a->p.vy);
                    if (sp > 2.0f) {
                        b->h.hp -= (int)(sp * 4.0f);
                        if (b->h.hp <= 0) { b->h.alive = false; }
                    }
                } else if (b->type == AGENT_VEHICLE && a->type != AGENT_VEHICLE) {
                    float sp = sqrtf(b->p.vx * b->p.vx + b->p.vy * b->p.vy);
                    if (sp > 2.0f) {
                        a->h.hp -= (int)(sp * 4.0f);
                        if (a->h.hp <= 0) { a->h.alive = false; }
                    }
                }
            }
        }
    }
}

void physics_tick(EntityPool* pool, World* w, float dt) {
    /* Build spatial index first */
    world_spatial_clear(w);
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* e = &pool->entities[i];
        if (!e->active) continue;
        world_spatial_insert(w, e->id, e->t.x, e->t.y);
    }
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* e = &pool->entities[i];
        if (!e->active) continue;
        /* Passenger entities don't integrate — they inherit vehicle pos */
        if (e->vehicle >= 0) {
            Entity* veh = entity_get(pool, e->vehicle);
            if (veh) {
                e->t.x = veh->t.x;
                e->t.y = veh->t.y;
                e->t.heading = veh->t.heading;
                e->p.vx = veh->p.vx;
                e->p.vy = veh->p.vy;
            }
            continue;
        }
        integrate(e, dt);
        resolve_tile_collision(e, w);
    }
    resolve_entity_collisions(pool, w);
}
