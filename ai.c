#include "ai.h"
#include "physics.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------------- A* ---------------- */

typedef struct {
    int  x, y;
    int  g, f;
    int  parent;
} Node;

/* Binary heap on open list indices, keyed by f */
typedef struct {
    int   idx[ASTAR_MAX_NODES];
    int   f[ASTAR_MAX_NODES];
    int   size;
} Heap;

static void heap_push(Heap* h, int idx, int f) {
    int i = h->size++;
    h->idx[i] = idx;
    h->f[i] = f;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->f[p] > h->f[i]) {
            int ti = h->idx[p]; h->idx[p] = h->idx[i]; h->idx[i] = ti;
            int tf = h->f[p]; h->f[p] = h->f[i]; h->f[i] = tf;
            i = p;
        } else break;
    }
}
static int heap_pop(Heap* h) {
    int top = h->idx[0];
    h->size--;
    if (h->size > 0) {
        h->idx[0] = h->idx[h->size];
        h->f[0] = h->f[h->size];
        int i = 0;
        while (1) {
            int l = 2*i+1, r = 2*i+2, s = i;
            if (l < h->size && h->f[l] < h->f[s]) s = l;
            if (r < h->size && h->f[r] < h->f[s]) s = r;
            if (s == i) break;
            int ti = h->idx[i]; h->idx[i] = h->idx[s]; h->idx[s] = ti;
            int tf = h->f[i]; h->f[i] = h->f[s]; h->f[s] = tf;
            i = s;
        }
    }
    return top;
}

static int tile_walkable_for(World* w, int x, int y, int mask) {
    TileType t = world_tile(w, x, y);
    if (mask == 0) return t == TILE_ROAD;
    return t == TILE_ROAD || t == TILE_SIDEWALK;
}

int ai_astar(World* w, int sx, int sy, int tx, int ty,
             int walkable_mask, int* path_xy, int cap) {
    if (sx == tx && sy == ty) return 0;
    if (!world_in_bounds(w, sx, sy) || !world_in_bounds(w, tx, ty)) return 0;
    if (!tile_walkable_for(w, tx, ty, walkable_mask)) {
        /* find nearest walkable within small radius */
        int found = 0;
        for (int r = 1; r < 6 && !found; r++) {
            for (int dy = -r; dy <= r && !found; dy++) {
                for (int dx = -r; dx <= r && !found; dx++) {
                    if (tile_walkable_for(w, tx + dx, ty + dy, walkable_mask)) {
                        tx += dx; ty += dy; found = 1;
                    }
                }
            }
        }
        if (!found) return 0;
    }

    static int came_from[WORLD_W * WORLD_H];
    static int gscore[WORLD_W * WORLD_H];
    static uint8_t closed[WORLD_W * WORLD_H];
    static uint8_t opened[WORLD_W * WORLD_H];
    memset(closed, 0, sizeof(closed));
    memset(opened, 0, sizeof(opened));
    for (int i = 0; i < WORLD_W * WORLD_H; i++) { gscore[i] = INT32_MAX; came_from[i] = -1; }

    Heap heap; heap.size = 0;
    int start = sy * w->w + sx;
    int goal  = ty * w->w + tx;
    gscore[start] = 0;
    int h0 = (abs(sx - tx) + abs(sy - ty)) * 10;
    heap_push(&heap, start, h0);
    opened[start] = 1;

    const int dx4[4] = {1, -1, 0, 0};
    const int dy4[4] = {0, 0, 1, -1};

    int iterations = 0;
    int max_iter = WORLD_W * WORLD_H;
    while (heap.size > 0 && iterations++ < max_iter) {
        int cur = heap_pop(&heap);
        if (cur == goal) {
            /* reconstruct */
            int chain[PATH_MAX_LEN];
            int n = 0;
            int c = cur;
            while (c != -1 && n < PATH_MAX_LEN) {
                chain[n++] = c;
                if (c == start) break;
                c = came_from[c];
            }
            /* Reverse into path_xy, skip start */
            int out = 0;
            for (int i = n - 2; i >= 0 && out < cap; i--) {
                int cx = chain[i] % w->w;
                int cy = chain[i] / w->w;
                path_xy[out*2+0] = cx;
                path_xy[out*2+1] = cy;
                out++;
            }
            return out;
        }
        closed[cur] = 1;
        int cx = cur % w->w;
        int cy = cur / w->w;
        for (int k = 0; k < 4; k++) {
            int nx = cx + dx4[k], ny = cy + dy4[k];
            if (!world_in_bounds(w, nx, ny)) continue;
            if (!tile_walkable_for(w, nx, ny, walkable_mask)) continue;
            int nid = ny * w->w + nx;
            if (closed[nid]) continue;
            int tg = gscore[cur] + 10;
            if (tg < gscore[nid]) {
                gscore[nid] = tg;
                came_from[nid] = cur;
                int hh = (abs(nx - tx) + abs(ny - ty)) * 10;
                heap_push(&heap, nid, tg + hh);
                opened[nid] = 1;
            }
        }
    }
    return 0;
}

/* ---------------- Steering helpers ---------------- */

static void ped_move_toward(Entity* e, float tx, float ty, float speed, float dt) {
    float dx = tx - e->t.x;
    float dy = ty - e->t.y;
    float d = sqrtf(dx*dx + dy*dy);
    if (d < 1e-4f) return;
    float nx = dx / d, ny = dy / d;
    /* Apply as acceleration */
    e->p.ax += nx * PED_ACCEL;
    e->p.ay += ny * PED_ACCEL;
    /* Cap velocity at desired speed */
    float vx = e->p.vx, vy = e->p.vy;
    float v = sqrtf(vx*vx + vy*vy);
    if (v > speed) {
        e->p.vx = vx / v * speed;
        e->p.vy = vy / v * speed;
    }
    e->t.heading = atan2f(ny, nx);
    (void)dt;
}

/* ---------------- Civilian FSM ---------------- */

static int random_sidewalk(World* w, int* out_x, int* out_y) {
    if (w->sidewalk_count <= 0) return 0;
    int i = rand() % w->sidewalk_count;
    *out_x = w->sidewalk_xs[i];
    *out_y = w->sidewalk_ys[i];
    return 1;
}
static int random_road(World* w, int* out_x, int* out_y) {
    if (w->road_count <= 0) return 0;
    int i = rand() % w->road_count;
    *out_x = w->road_xs[i];
    *out_y = w->road_ys[i];
    return 1;
}

static void tick_civilian(Entity* e, World* w, Entity* player, float dt) {
    /* Check flee condition */
    float dpx = player->t.x - e->t.x;
    float dpy = player->t.y - e->t.y;
    float d2 = dpx*dpx + dpy*dpy;
    int threaten = 0;
    if (player->w.stars >= 2 && d2 < CIV_FLEE_RANGE * CIV_FLEE_RANGE) threaten = 1;
    if (player->vehicle >= 0) {
        Entity* veh = entity_get((EntityPool*)((char*)e - ((char*)e - (char*)e)), player->vehicle);
        (void)veh;
        float vsq = player->p.vx*player->p.vx + player->p.vy*player->p.vy;
        if (vsq > 4.0f && d2 < 25.0f) threaten = 1;
    }

    if (threaten) e->ai.state = AI_FLEE;
    else if (e->ai.state == AI_FLEE) e->ai.state = AI_IDLE;

    switch (e->ai.state) {
        case AI_IDLE: {
            if (random_sidewalk(w, &e->ai.waypoint_x, &e->ai.waypoint_y)) {
                int n = ai_astar(w, (int)e->t.x, (int)e->t.y,
                                 e->ai.waypoint_x, e->ai.waypoint_y,
                                 1, e->ai.path, PATH_MAX_LEN);
                e->ai.path_len = n;
                e->ai.path_idx = 0;
                if (n > 0) e->ai.state = AI_WALK_WAYPOINT;
            }
        } break;
        case AI_WALK_WAYPOINT: {
            if (e->ai.path_idx >= e->ai.path_len) { e->ai.state = AI_IDLE; break; }
            int px = e->ai.path[e->ai.path_idx*2+0];
            int py = e->ai.path[e->ai.path_idx*2+1];
            float tx = px + 0.5f, ty = py + 0.5f;
            ped_move_toward(e, tx, ty, CIV_WALK_SPEED, dt);
            if (phys_dist2(e->t.x, e->t.y, tx, ty) < 0.25f) {
                e->ai.path_idx++;
            }
        } break;
        case AI_FLEE: {
            float d = sqrtf(d2);
            if (d < 1e-4f) d = 1;
            float fx = e->t.x - dpx / d * 4.0f;
            float fy = e->t.y - dpy / d * 4.0f;
            /* perpendicular bias */
            float px = -dpy / d;
            float py = dpx / d;
            fx += px * 2.0f;
            fy += py * 2.0f;
            ped_move_toward(e, fx, fy, CIV_WALK_SPEED * 1.8f, dt);
        } break;
        default: e->ai.state = AI_IDLE; break;
    }
}

/* ---------------- Police FSM ---------------- */

static void tick_police(Entity* e, World* w, Entity* player, float dt, int tick, int player_wanted) {
    float d = phys_dist(e->t.x, e->t.y, player->t.x, player->t.y);

    if (e->ai.attack_cooldown > 0) e->ai.attack_cooldown--;
    if (e->ai.replan_cooldown > 0) e->ai.replan_cooldown--;

    /* Transitions */
    if (player_wanted <= 0) {
        e->ai.state = AI_RETURN;
    } else if (d < POLICE_ATTACK_RANGE) {
        e->ai.state = AI_ENGAGE;
    } else if (d < POLICE_SIGHT_RANGE) {
        e->ai.state = AI_PURSUE;
    } else if (player_wanted >= 1 && e->ai.state != AI_ALERT && e->ai.state != AI_PURSUE) {
        e->ai.state = AI_ALERT;
    }

    switch (e->ai.state) {
        case AI_PATROL: {
            if (e->ai.path_idx >= e->ai.path_len) {
                if (random_road(w, &e->ai.waypoint_x, &e->ai.waypoint_y)) {
                    int n = ai_astar(w, (int)e->t.x, (int)e->t.y,
                                     e->ai.waypoint_x, e->ai.waypoint_y,
                                     0, e->ai.path, PATH_MAX_LEN);
                    e->ai.path_len = n;
                    e->ai.path_idx = 0;
                }
            } else {
                int px = e->ai.path[e->ai.path_idx*2+0];
                int py = e->ai.path[e->ai.path_idx*2+1];
                float tx = px + 0.5f, ty = py + 0.5f;
                ped_move_toward(e, tx, ty, POLICE_PATROL_SPEED, dt);
                if (phys_dist2(e->t.x, e->t.y, tx, ty) < 0.25f) e->ai.path_idx++;
            }
        } break;
        case AI_ALERT: {
            if (e->ai.replan_cooldown == 0 || e->ai.path_len == 0) {
                int n = ai_astar(w, (int)e->t.x, (int)e->t.y,
                                 (int)player->w.last_known_x, (int)player->w.last_known_y,
                                 1, e->ai.path, PATH_MAX_LEN);
                e->ai.path_len = n;
                e->ai.path_idx = 0;
                e->ai.replan_cooldown = TICK_HZ;
            }
            if (e->ai.path_idx < e->ai.path_len) {
                int px = e->ai.path[e->ai.path_idx*2+0];
                int py = e->ai.path[e->ai.path_idx*2+1];
                float tx = px + 0.5f, ty = py + 0.5f;
                ped_move_toward(e, tx, ty, POLICE_PATROL_SPEED * 1.4f, dt);
                if (phys_dist2(e->t.x, e->t.y, tx, ty) < 0.25f) e->ai.path_idx++;
            }
        } break;
        case AI_PURSUE: {
            /* Replan every 30 ticks */
            if (e->ai.replan_cooldown == 0) {
                int n = ai_astar(w, (int)e->t.x, (int)e->t.y,
                                 (int)player->t.x, (int)player->t.y,
                                 1, e->ai.path, PATH_MAX_LEN);
                e->ai.path_len = n;
                e->ai.path_idx = 0;
                e->ai.replan_cooldown = TICK_HZ / 2;
            }
            /* If line-of-sight close, direct chase */
            if (d < 6.0f) {
                ped_move_toward(e, player->t.x, player->t.y, POLICE_CHASE_SPEED, dt);
            } else if (e->ai.path_idx < e->ai.path_len) {
                int px = e->ai.path[e->ai.path_idx*2+0];
                int py = e->ai.path[e->ai.path_idx*2+1];
                float tx = px + 0.5f, ty = py + 0.5f;
                ped_move_toward(e, tx, ty, POLICE_CHASE_SPEED, dt);
                if (phys_dist2(e->t.x, e->t.y, tx, ty) < 0.25f) e->ai.path_idx++;
            } else {
                ped_move_toward(e, player->t.x, player->t.y, POLICE_CHASE_SPEED, dt);
            }
        } break;
        case AI_ENGAGE: {
            /* attack */
            if (e->ai.attack_cooldown == 0) {
                player->h.hp -= POLICE_ATTACK_DMG;
                e->ai.attack_cooldown = POLICE_ATTACK_COOLDOWN_TICKS;
                if (player->h.hp <= 0) player->h.alive = false;
            }
        } break;
        case AI_RETURN: {
            e->ai.state = AI_PATROL;
            e->ai.path_len = 0;
            e->ai.path_idx = 0;
        } break;
        default: e->ai.state = AI_PATROL; break;
    }
    (void)tick;
}

/* ---------------- Entry ---------------- */

void ai_tick(EntityPool* pool, World* w, int32_t player_id, int tick, int player_wanted) {
    Entity* player = entity_get(pool, player_id);
    if (!player) return;
    float dt = TICK_DT;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* e = &pool->entities[i];
        if (!e->active || !e->h.alive) continue;
        if (e->vehicle >= 0) continue; /* riding a vehicle: controlled elsewhere */
        switch (e->type) {
            case AGENT_CIVILIAN:
                tick_civilian(e, w, player, dt);
                break;
            case AGENT_POLICE:
                tick_police(e, w, player, dt, tick, player_wanted);
                break;
            default: break;
        }
    }
}
