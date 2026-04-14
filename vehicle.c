#include "vehicle.h"
#include "physics.h"
#include <math.h>

void vehicle_tick(EntityPool* pool, World* w, float dt) {
    (void)w;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* e = &pool->entities[i];
        if (!e->active || e->type != AGENT_VEHICLE) continue;

        /* Steering: heading changes proportionally to speed (Ackermann simplification) */
        float sp = e->t.speed;
        float speed_ratio = sp / VEH_MAX_SPEED;
        if (speed_ratio < 0) speed_ratio = -speed_ratio;
        e->t.heading += e->steer_input * VEH_TURN_RATE * dt * (0.3f + 0.7f * fminf(1.0f, speed_ratio + 0.2f));

        /* Throttle: throttle_input -1..1 */
        float accel = 0.0f;
        if (e->throttle_input > 0) accel = e->throttle_input * VEH_ACCEL;
        else if (e->throttle_input < 0) accel = e->throttle_input * VEH_BRAKE;

        if (e->brake_input) {
            if (sp > 0) accel -= VEH_BRAKE;
            else if (sp < 0) accel += VEH_BRAKE;
        }

        e->t.speed += accel * dt;
        /* Friction on forward speed */
        float f = 1.0f - VEH_FRICTION * dt;
        if (f < 0) f = 0;
        e->t.speed *= f;

        /* Clamp */
        if (e->t.speed > VEH_MAX_SPEED) e->t.speed = VEH_MAX_SPEED;
        if (e->t.speed < -VEH_REVERSE_SPEED) e->t.speed = -VEH_REVERSE_SPEED;

        /* Convert forward speed + heading to velocity */
        e->p.vx = cosf(e->t.heading) * e->t.speed;
        e->p.vy = sinf(e->t.heading) * e->t.speed;

        /* Clear one-shot inputs */
        e->brake_input = false;
    }
}

int32_t vehicle_try_enter(EntityPool* pool, Entity* ped, float radius) {
    int32_t best = -1;
    float best_d2 = radius * radius;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* v = &pool->entities[i];
        if (!v->active || v->type != AGENT_VEHICLE) continue;
        if (v->v.driver >= 0) continue;
        float d2 = phys_dist2(ped->t.x, ped->t.y, v->t.x, v->t.y);
        if (d2 < best_d2) {
            best_d2 = d2;
            best = v->id;
        }
    }
    if (best >= 0) {
        Entity* v = entity_get(pool, best);
        v->v.driver = ped->id;
        ped->vehicle = v->id;
    }
    return best;
}

void vehicle_exit(EntityPool* pool, Entity* ped) {
    if (ped->vehicle < 0) return;
    Entity* v = entity_get(pool, ped->vehicle);
    if (!v) { ped->vehicle = -1; return; }
    v->v.driver = -1;
    ped->vehicle = -1;
    /* place ped to side of vehicle */
    float side = v->t.heading + 1.5708f; /* +90 deg */
    ped->t.x = v->t.x + cosf(side) * 1.2f;
    ped->t.y = v->t.y + sinf(side) * 1.2f;
    ped->p.vx = 0;
    ped->p.vy = 0;
    ped->t.speed = 0;
}
