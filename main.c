#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include "entity.h"
#include "world.h"
#include "physics.h"
#include "vehicle.h"
#include "ai.h"
#include "wanted.h"
#include "input.h"
#include "renderer.h"
#include "keybinds.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <signal.h>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

#ifdef _WIN32
#ifdef USE_RAYLIB
/* Avoid including windows.h — it conflicts with Raylib (Rectangle, CloseWindow, etc.)
 * Declare only the Win32 functions we actually need. */
typedef union { struct { unsigned long LowPart; long HighPart; }; long long QuadPart; } MY_LARGE_INTEGER;
__declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
__declspec(dllimport) int  __stdcall QueryPerformanceFrequency(MY_LARGE_INTEGER* lpFrequency);
__declspec(dllimport) int  __stdcall QueryPerformanceCounter(MY_LARGE_INTEGER* lpPerformanceCount);
static void sleep_us(long us) { Sleep(us / 1000); }
static long now_us(void) {
    static MY_LARGE_INTEGER freq = {0};
    MY_LARGE_INTEGER counter;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (long)(counter.QuadPart * 1000000L / freq.QuadPart);
}
#else
#include <windows.h>
static void sleep_us(long us) { Sleep(us / 1000); }
static long now_us(void) {
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (long)(counter.QuadPart * 1000000L / freq.QuadPart);
}
#endif
#else
#include <unistd.h>
#include <time.h>
static void sleep_us(long us) {
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000L;
    nanosleep(&ts, NULL);
}
static long now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
}
#endif

/* ============================================================
 *  GAME STATE
 * ============================================================ */
typedef enum {
    STATE_TITLE,
    STATE_MENU,
    STATE_SETTINGS,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_BUSTED
} GameState;

static EntityPool g_pool;
static World      g_world;
static Renderer*  g_renderer;
static volatile int g_running = 1;

/* Game state */
static GameState  g_state = STATE_TITLE;
static float      g_state_timer = 0;
static int        g_menu_sel = 0;
static int        g_pause_sel = 0;
static int        g_settings_sel = 0;
static int        g_settings_waiting = -1; /* -1 = not waiting, else idx of key being rebound */
static GameState  g_settings_from = STATE_MENU; /* where to return from settings */

/* Key bindings (defined in input.c) */
extern KeyBindings g_keybinds;

static int32_t    g_player_id = -1;
static int        g_tick = 0;
static int        g_busted = 0;     /* 1 = died from police */

static void on_sigint(int s) { (void)s; g_running = 0; }

/* ============================================================
 *  GAME INIT / RESET
 * ============================================================ */

static int32_t spawn_player(int rx, int ry) {
    Entity* p = entity_create(&g_pool, AGENT_PLAYER, rx + 0.5f, ry + 0.5f);
    if (!p) return -1;
    wanted_init(p);
    p->gun.ammo_in_mag = PISTOL_MAG_SIZE;
    p->gun.mag_size = PISTOL_MAG_SIZE;
    return p->id;
}

static void spawn_parked_vehicles(int count) {
    int placed = 0;
    int tries = count * 10;
    while (placed < count && tries-- > 0) {
        if (g_world.road_count <= 0) break;
        int r = rand() % g_world.road_count;
        int rx = g_world.road_xs[r], ry = g_world.road_ys[r];
        int32_t nearby[16];
        int n = world_spatial_query(&g_world, rx + 0.5f, ry + 0.5f, 2.5f, nearby, 16);
        int occupied = 0;
        for (int i = 0; i < n; i++) {
            Entity* e = entity_get(&g_pool, nearby[i]);
            if (e && e->type == AGENT_VEHICLE) { occupied = 1; break; }
        }
        if (occupied) continue;
        Entity* v = entity_create(&g_pool, AGENT_VEHICLE, rx + 0.5f, ry + 0.5f);
        if (v) {
            v->t.heading = (rand() % 4) * 1.5708f;
            placed++;
        }
        world_spatial_clear(&g_world);
        for (int i = 0; i < MAX_ENTITIES; i++) {
            Entity* e = &g_pool.entities[i];
            if (e->active) world_spatial_insert(&g_world, e->id, e->t.x, e->t.y);
        }
    }
}

static void spawn_civilians(int count) {
    int placed = 0;
    int tries = count * 10;
    while (placed < count && tries-- > 0) {
        if (g_world.sidewalk_count <= 0) break;
        int i = rand() % g_world.sidewalk_count;
        int rx = g_world.sidewalk_xs[i], ry = g_world.sidewalk_ys[i];
        Entity* c = entity_create(&g_pool, AGENT_CIVILIAN, rx + 0.5f, ry + 0.5f);
        if (c) placed++;
    }
}

static void game_init(void) {
    entity_pool_init(&g_pool);

    int spawn_x = g_world.w / 2, spawn_y = g_world.h / 2;
    if (g_world.road_count > 0) {
        int r = rand() % g_world.road_count;
        spawn_x = g_world.road_xs[r];
        spawn_y = g_world.road_ys[r];
    }
    g_player_id = spawn_player(spawn_x, spawn_y);
    g_tick = 0;
    g_busted = 0;

    spawn_parked_vehicles(MAX_PARKED_VEHICLES);
    spawn_civilians(MAX_CIVILIANS);
}

/* ============================================================
 *  SHOOTING LOGIC
 * ============================================================ */

static void do_shoot(Entity* player) {
    if (player->gun.shoot_cooldown > 0) return;
    if (player->gun.reloading) return;
    if (player->gun.ammo_in_mag <= 0) {
        /* auto-reload */
        player->gun.reloading = true;
        player->gun.reload_timer = PISTOL_RELOAD_TICKS;
        return;
    }

    player->gun.ammo_in_mag--;
    player->gun.shoot_cooldown = PISTOL_SHOOT_COOLDOWN;
    player->gun.muzzle_flash = 4;

    /* Raycast along heading (use vehicle heading if in car) */
    float shoot_heading = player->t.heading;
    float shoot_x = player->t.x;
    float shoot_y = player->t.y;
    if (player->vehicle >= 0) {
        Entity* veh = entity_get(&g_pool, player->vehicle);
        if (veh) {
            shoot_heading = veh->t.heading;
            shoot_x = veh->t.x;
            shoot_y = veh->t.y;
        }
    }
    float dx = cosf(shoot_heading);
    float dy = sinf(shoot_heading);
    float ox = shoot_x + dx * 0.5f;
    float oy = shoot_y + dy * 0.5f;

    float best_t = PISTOL_RANGE;
    Entity* hit = NULL;

    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity* e = &g_pool.entities[i];
        if (!e->active || !e->h.alive) continue;
        if (e->id == player->id) continue;
        if (e->type == AGENT_VEHICLE) continue;

        /* Point-vs-circle raycast */
        float ex = e->t.x - ox;
        float ey = e->t.y - oy;
        float b = ex * dx + ey * dy;
        float c2 = ex * ex + ey * ey - e->p.radius * e->p.radius;
        float disc = b * b - c2;
        if (disc < 0) continue;
        float t = b - sqrtf(disc);
        if (t > 0 && t < best_t) {
            best_t = t;
            hit = e;
        }
    }

    /* Tracer line */
    float end_x = ox + dx * best_t;
    float end_y = oy + dy * best_t;
    renderer_add_tracer(ox, oy, end_x, end_y);

    if (hit) {
        hit->h.hp -= PISTOL_DAMAGE;
        if (hit->h.hp <= 0) {
            hit->h.alive = false;
            player->gun.npc_kills++;
            if (hit->type == AGENT_CIVILIAN || hit->type == AGENT_POLICE) {
                wanted_report_crime(player, CRIME_SHOOT_NPC, g_tick);
            }
        }
    }

    /* Auto-reload if empty */
    if (player->gun.ammo_in_mag <= 0) {
        player->gun.reloading = true;
        player->gun.reload_timer = PISTOL_RELOAD_TICKS;
    }
}

static void weapon_tick(Entity* player) {
    if (player->gun.shoot_cooldown > 0) player->gun.shoot_cooldown--;
    if (player->gun.muzzle_flash > 0)   player->gun.muzzle_flash--;

    if (player->gun.reloading) {
        player->gun.reload_timer--;
        if (player->gun.reload_timer <= 0) {
            player->gun.reloading = false;
            player->gun.ammo_in_mag = player->gun.mag_size;
        }
    }
}

/* ============================================================
 *  ONE FRAME OF GAME LOGIC (called by main loop or emscripten)
 * ============================================================ */

static void game_frame(void) {
#ifndef PLATFORM_WEB
    long frame_start = now_us();
#endif
    float dt = TICK_DT;

#ifdef USE_RAYLIB
    if (WindowShouldClose()) { g_running = 0; return; }
#endif

    switch (g_state) {

    /* ======================== TITLE SCREEN ======================== */
    case STATE_TITLE: {
        g_state_timer += dt;
        renderer_draw_title(g_state_timer);
        if (g_state_timer > 2.0f) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
                IsKeyPressed(KEY_SPACE)) {
                g_state = STATE_MENU;
                g_menu_sel = 0;
            }
        }
    } break;

    /* ======================== MAIN MENU ======================== */
    case STATE_MENU: {
        renderer_draw_menu(g_menu_sel);
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            g_menu_sel--;
            if (g_menu_sel < 0) g_menu_sel = 2;
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
            g_menu_sel++;
            if (g_menu_sel > 2) g_menu_sel = 0;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            if (g_menu_sel == 0) {
                game_init();
                g_state = STATE_PLAYING;
            } else if (g_menu_sel == 1) {
                g_state = STATE_SETTINGS;
                g_settings_sel = 0;
                g_settings_waiting = -1;
                g_settings_from = STATE_MENU;
            } else {
                g_running = 0;
            }
        }
    } break;

    /* ======================== SETTINGS ======================== */
    case STATE_SETTINGS: {
        renderer_draw_settings(g_settings_sel, g_settings_waiting, &g_keybinds);
        if (g_settings_waiting >= 0) {
            int key = GetKeyPressed();
            if (key > 0 && key != KEY_ESCAPE) {
                int* ptr = keybind_get_ptr(&g_keybinds, g_settings_waiting);
                if (ptr) *ptr = key;
                g_settings_waiting = -1;
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                g_settings_waiting = -1;
            }
        } else {
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                g_settings_sel--;
                if (g_settings_sel < 0) g_settings_sel = KEYBIND_COUNT - 1;
            }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                g_settings_sel++;
                if (g_settings_sel >= KEYBIND_COUNT) g_settings_sel = 0;
            }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                g_settings_waiting = g_settings_sel;
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                g_state = g_settings_from;
            }
        }
    } break;

    /* ======================== PLAYING ======================== */
    case STATE_PLAYING: {
        InputState inp;
        input_poll(&inp);
        if (inp.quit) {
            g_state = STATE_PAUSED;
            g_pause_sel = 0;
            break;
        }

        Entity* player = entity_get(&g_pool, g_player_id);
        if (!player || !player->h.alive) {
            g_state = STATE_BUSTED;
            g_state_timer = 0;
            break;
        }

        if (inp.toggle_view) {
            int mode = renderer_get_camera_mode();
            renderer_set_camera_mode(mode == CAM_THIRD_PERSON ? CAM_FIRST_PERSON : CAM_THIRD_PERSON);
        }

        /* --- Player control --- */
        if (player->vehicle >= 0) {
            Entity* veh = entity_get(&g_pool, player->vehicle);
            if (veh) {
                float thr = 0.0f, steer = 0.0f;
                if (inp.up)    thr += 1.0f;
                if (inp.down)  thr -= 1.0f;
                if (inp.left)  steer -= 1.0f;
                if (inp.right) steer += 1.0f;
                veh->throttle_input = thr;
                veh->steer_input = steer;
                veh->brake_input = inp.brake;
            }
            if (inp.enter_exit) vehicle_exit(&g_pool, player);
            if (inp.shoot) do_shoot(player);
            if (inp.reload && !player->gun.reloading &&
                player->gun.ammo_in_mag < player->gun.mag_size) {
                player->gun.reloading = true;
                player->gun.reload_timer = PISTOL_RELOAD_TICKS;
            }
        } else {
            const float INV_SQRT2 = 0.70710678f;
            float fwd_x = -INV_SQRT2, fwd_y = -INV_SQRT2;
            float rgt_x =  INV_SQRT2, rgt_y = -INV_SQRT2;
            float ax = 0, ay = 0;
            if (inp.up)    { ax += fwd_x; ay += fwd_y; }
            if (inp.down)  { ax -= fwd_x; ay -= fwd_y; }
            if (inp.right) { ax += rgt_x; ay += rgt_y; }
            if (inp.left)  { ax -= rgt_x; ay -= rgt_y; }
            float mag = sqrtf(ax*ax + ay*ay);

            float move_speed = inp.sprint ? PED_SPRINT_SPEED : PED_MAX_SPEED;
            float move_accel = inp.sprint ? PED_SPRINT_ACCEL : PED_ACCEL;

            if (mag > 0.001f) {
                ax /= mag; ay /= mag;
                player->p.ax = ax * move_accel;
                player->p.ay = ay * move_accel;
                float vx = player->p.vx, vy = player->p.vy;
                float v = sqrtf(vx*vx + vy*vy);
                if (v > move_speed) {
                    player->p.vx = vx / v * move_speed;
                    player->p.vy = vy / v * move_speed;
                }
            }

            if (inp.mouse_valid) {
                float ddx = inp.mouse_wx - player->t.x;
                float ddy = inp.mouse_wy - player->t.y;
                if (ddx*ddx + ddy*ddy > 0.01f) player->t.heading = atan2f(ddy, ddx);
            } else if (mag > 0.001f) {
                player->t.heading = atan2f(ay, ax);
            }

            if (inp.jump && player->t.z < 0.01f) player->t.vz = PED_JUMP_VEL;
            if (inp.enter_exit) vehicle_try_enter(&g_pool, player, 1.6f);
            if (inp.shoot) do_shoot(player);
            if (inp.reload && !player->gun.reloading &&
                player->gun.ammo_in_mag < player->gun.mag_size) {
                player->gun.reloading = true;
                player->gun.reload_timer = PISTOL_RELOAD_TICKS;
            }
        }

        weapon_tick(player);
        vehicle_tick(&g_pool, &g_world, TICK_DT);
        ai_tick(&g_pool, &g_world, g_player_id, g_tick, player->w.stars);
        physics_tick(&g_pool, &g_world, TICK_DT);
        wanted_tick(&g_pool, &g_world, player, g_tick);
        wanted_spawn_police(&g_pool, &g_world, player, g_tick);

        if (!player->h.alive) g_busted = 1;

        float cam_x = player->t.x;
        float cam_y = player->t.y;
        renderer_render_world(&g_world, &g_pool, player, cam_x, cam_y);
        g_tick++;
    } break;

    /* ======================== PAUSED ======================== */
    case STATE_PAUSED: {
        renderer_draw_pause(g_pause_sel);
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            g_pause_sel--;
            if (g_pause_sel < 0) g_pause_sel = 2;
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
            g_pause_sel++;
            if (g_pause_sel > 2) g_pause_sel = 0;
        }
        if (IsKeyPressed(KEY_ESCAPE)) g_state = STATE_PLAYING;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            if (g_pause_sel == 0) {
                g_state = STATE_PLAYING;
            } else if (g_pause_sel == 1) {
                g_state = STATE_SETTINGS;
                g_settings_sel = 0;
                g_settings_waiting = -1;
                g_settings_from = STATE_PAUSED;
            } else {
                g_state = STATE_MENU;
                g_menu_sel = 0;
            }
        }
    } break;

    /* ======================== BUSTED ======================== */
    case STATE_BUSTED: {
        g_state_timer += dt;
        renderer_draw_busted(g_state_timer);
        if (g_state_timer > 4.0f) {
            game_init();
            g_state = STATE_PLAYING;
        }
    } break;

    } /* end switch */

#ifndef PLATFORM_WEB
    long elapsed = now_us() - frame_start;
    long to_sleep = 1000000L / TICK_HZ - elapsed;
    if (to_sleep > 0) sleep_us(to_sleep);
#endif
}

/* ============================================================
 *  MAIN
 * ============================================================ */

int main(int argc, char** argv) {
    srand((unsigned)time(NULL));
    signal(SIGINT, on_sigint);

    const char* map_path = "map_default.txt";
    if (argc > 1) map_path = argv[1];

    /* Load world once (survives restarts) */
    if (!world_load_ascii(&g_world, map_path)) {
        world_generate_default(&g_world);
    }

    g_renderer = renderer_get();
    g_renderer->init(g_renderer->viewport_w, g_renderer->viewport_h);
    input_init();

    g_state = STATE_TITLE;
    g_state_timer = 0;

#ifdef PLATFORM_WEB
    emscripten_set_main_loop(game_frame, 60, 1);
#else
    while (g_running) {
        game_frame();
    }
#endif

    g_renderer->shutdown();
    input_shutdown();
    return 0;
}
