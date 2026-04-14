#include "renderer.h"
#include "config.h"
#include "keybinds.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static Renderer g_renderer;

#ifdef USE_RAYLIB
/* ============================================================
 *  RAYLIB 3D ISOMETRIC RENDERER
 * ============================================================ */
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#define WIN_W 1440
#define WIN_H 900
#define CAM_DIST  28.0f
#define CAM_HEIGHT 30.0f
#define CAM_FOV_ORTHO  28.0f

static Camera3D g_cam;
static Vector3  g_cam_target = { 0 };
static int      g_camera_mode = CAM_THIRD_PERSON;

/* Tracer system */
#define MAX_TRACERS 8
static struct { float x1, y1, x2, y2; int life; } g_tracers[MAX_TRACERS];

void renderer_set_camera_mode(int mode) { g_camera_mode = mode; }
int  renderer_get_camera_mode(void) { return g_camera_mode; }

void renderer_add_tracer(float x1, float y1, float x2, float y2) {
    for (int i = 0; i < MAX_TRACERS; i++) {
        if (g_tracers[i].life <= 0) {
            g_tracers[i].x1 = x1; g_tracers[i].y1 = y1;
            g_tracers[i].x2 = x2; g_tracers[i].y2 = y2;
            g_tracers[i].life = 4;
            break;
        }
    }
}

/* deterministic per-tile RNG for building variation */
static unsigned hash_uv(int x, int y) {
    unsigned h = (unsigned)(x * 73856093u) ^ (unsigned)(y * 19349663u);
    h ^= h >> 13; h *= 0x85ebca6bu; h ^= h >> 16;
    return h;
}

static Color color_lerp(Color a, Color b, float t) {
    Color c;
    c.r = (unsigned char)((1-t)*a.r + t*b.r);
    c.g = (unsigned char)((1-t)*a.g + t*b.g);
    c.b = (unsigned char)((1-t)*a.b + t*b.b);
    c.a = 255;
    return c;
}

/* -------- ground tile drawing -------- */
static void draw_road_tile(int tx, int ty) {
    Vector3 pos = { tx + 0.5f, 0.02f, ty + 0.5f };
    Vector3 sz  = { 1.0f, 0.04f, 1.0f };
    DrawCubeV(pos, sz, (Color){ 42, 42, 48, 255 });
    /* centerline stripe if this tile is a "middle" road cell */
    unsigned h = hash_uv(tx, ty);
    if ((h & 0xF) < 4) {
        Vector3 stripe_sz = (h & 0x10) ? (Vector3){0.6f, 0.05f, 0.08f}
                                       : (Vector3){0.08f, 0.05f, 0.6f};
        DrawCubeV((Vector3){tx + 0.5f, 0.05f, ty + 0.5f}, stripe_sz,
                  (Color){220, 210, 80, 255});
    }
}
static void draw_sidewalk_tile(int tx, int ty) {
    Vector3 pos = { tx + 0.5f, 0.10f, ty + 0.5f };
    Vector3 sz  = { 0.98f, 0.20f, 0.98f };
    unsigned h = hash_uv(tx, ty) & 0x1F;
    Color base = { (unsigned char)(170 + h/2), (unsigned char)(170 + h/2),
                   (unsigned char)(170 + h/2), 255 };
    DrawCubeV(pos, sz, base);
    DrawCubeWiresV(pos, sz, (Color){120, 120, 120, 255});
}
static void draw_grass_tile(int tx, int ty) {
    unsigned h = hash_uv(tx, ty);
    Color base = (Color){ 48 + (h & 0x1F), 120 + (h & 0x3F), 48 + ((h >> 4) & 0xF), 255 };
    Vector3 pos = { tx + 0.5f, 0.02f, ty + 0.5f };
    Vector3 sz  = { 1.0f, 0.04f, 1.0f };
    DrawCubeV(pos, sz, base);
    if ((h & 0x3F) < 3) {
        DrawCubeV((Vector3){tx + 0.5f, 0.15f, ty + 0.5f},
                  (Vector3){0.15f, 0.25f, 0.15f},
                  (Color){30, 90, 30, 255});
    }
}
static void draw_building_tile(int tx, int ty) {
    unsigned h = hash_uv(tx, ty);
    float height = 1.8f + (((h >> 3) & 0x7) * 0.45f);
    Color palette[5] = {
        (Color){170, 120,  90, 255},
        (Color){120, 130, 150, 255},
        (Color){180, 160, 130, 255},
        (Color){ 90, 100, 120, 255},
        (Color){150,  80,  70, 255},
    };
    Color base = palette[h % 5];
    Vector3 pos = { tx + 0.5f, height * 0.5f, ty + 0.5f };
    Vector3 sz  = { 0.98f, height, 0.98f };
    DrawCubeV(pos, sz, base);
    DrawCubeV((Vector3){tx + 0.5f, height + 0.05f, ty + 0.5f},
              (Vector3){1.0f, 0.1f, 1.0f}, color_lerp(base, BLACK, 0.4f));
    DrawCubeWiresV(pos, sz, color_lerp(base, BLACK, 0.5f));

    int floors = (int)height;
    Color win_lit = (Color){255, 220, 140, 255};
    Color win_dk  = (Color){ 40,  50,  70, 255};
    for (int f = 0; f < floors; f++) {
        float y = f + 0.6f;
        if (y > height - 0.3f) break;
        for (int wx = 0; wx < 2; wx++) {
            float dx = (wx == 0) ? -0.2f : 0.2f;
            unsigned lit = hash_uv(tx*131 + wx + f*7, ty*211 + f*17) & 0x7;
            Color c = (lit < 3) ? win_lit : win_dk;
            DrawCubeV((Vector3){tx + 0.5f + dx, y, ty + 0.98f},
                      (Vector3){0.22f, 0.30f, 0.02f}, c);
            DrawCubeV((Vector3){tx + 0.5f + dx, y, ty + 0.02f},
                      (Vector3){0.22f, 0.30f, 0.02f}, c);
            DrawCubeV((Vector3){tx + 0.98f, y, ty + 0.5f + dx},
                      (Vector3){0.02f, 0.30f, 0.22f}, c);
            DrawCubeV((Vector3){tx + 0.02f, y, ty + 0.5f + dx},
                      (Vector3){0.02f, 0.30f, 0.22f}, c);
        }
    }
}

/* -------- entity drawing -------- */
static void draw_human(float wx, float wy, float wz, float heading, Color shirt, Color pants, int has_gun, int muzzle_flash) {
    Color skin = { 230, 190, 160, 255 };
    Color hair = {  35,  25,  20, 255 };
    float gz = wz; /* jump offset */

    /* shadow (stays on ground) */
    DrawCylinder((Vector3){wx, 0.02f, wy}, 0.35f, 0.35f, 0.02f, 16,
                 (Color){0, 0, 0, 90});

    /* legs */
    float leg_off = 0.12f;
    float perp_x = -sinf(heading) * leg_off;
    float perp_y =  cosf(heading) * leg_off;

    DrawCylinderEx(
        (Vector3){ wx + perp_x, gz + 0.0f, wy + perp_y },
        (Vector3){ wx + perp_x, gz + 0.55f, wy + perp_y },
        0.12f, 0.11f, 8, pants);
    DrawCylinderEx(
        (Vector3){ wx - perp_x, gz + 0.0f, wy - perp_y },
        (Vector3){ wx - perp_x, gz + 0.55f, wy - perp_y },
        0.12f, 0.11f, 8, pants);

    /* torso */
    DrawCylinderEx(
        (Vector3){ wx, gz + 0.55f, wy },
        (Vector3){ wx, gz + 1.10f, wy },
        0.26f, 0.22f, 10, shirt);

    /* arms */
    float arm_off_x = -sinf(heading) * 0.28f;
    float arm_off_y =  cosf(heading) * 0.28f;

    /* Right arm (gun hand) — extended forward if has_gun */
    if (has_gun) {
        float gun_dir_x = cosf(heading) * 0.45f;
        float gun_dir_y = sinf(heading) * 0.45f;
        /* right arm reaches forward */
        DrawCylinderEx(
            (Vector3){ wx + arm_off_x, gz + 1.05f, wy + arm_off_y },
            (Vector3){ wx + arm_off_x + gun_dir_x, gz + 0.85f, wy + arm_off_y + gun_dir_y },
            0.09f, 0.08f, 8, shirt);
        /* pistol (dark box) */
        float gx = wx + arm_off_x + gun_dir_x;
        float gy = wy + arm_off_y + gun_dir_y;
        rlPushMatrix();
        rlTranslatef(gx, gz + 0.82f, gy);
        rlRotatef(-heading * RAD2DEG, 0, 1, 0);
        DrawCube((Vector3){0.15f, 0, 0}, 0.30f, 0.10f, 0.08f, (Color){30, 30, 35, 255});
        /* barrel */
        DrawCube((Vector3){0.32f, 0.02f, 0}, 0.12f, 0.06f, 0.05f, (Color){50, 50, 55, 255});
        rlPopMatrix();
        /* muzzle flash */
        if (muzzle_flash > 0) {
            float fx = gx + cosf(heading) * 0.45f;
            float fy = gy + sinf(heading) * 0.45f;
            float flash_r = 0.12f + (muzzle_flash * 0.05f);
            DrawSphere((Vector3){fx, gz + 0.85f, fy}, flash_r, (Color){255, 220, 50, 230});
            DrawSphere((Vector3){fx, gz + 0.85f, fy}, flash_r * 0.6f, WHITE);
        }
    } else {
        DrawCylinderEx(
            (Vector3){ wx + arm_off_x, gz + 1.05f, wy + arm_off_y },
            (Vector3){ wx + arm_off_x, gz + 0.60f, wy + arm_off_y },
            0.09f, 0.08f, 8, shirt);
    }

    /* Left arm — always normal */
    DrawCylinderEx(
        (Vector3){ wx - arm_off_x, gz + 1.05f, wy - arm_off_y },
        (Vector3){ wx - arm_off_x, gz + 0.60f, wy - arm_off_y },
        0.09f, 0.08f, 8, shirt);

    /* head */
    DrawSphere((Vector3){ wx, gz + 1.28f, wy }, 0.17f, skin);
    DrawSphere((Vector3){ wx - sinf(heading)*0.0f, gz + 1.34f, wy + cosf(heading)*0.0f }, 0.175f, hair);
    DrawSphere((Vector3){ wx + cosf(heading)*0.17f, gz + 1.27f, wy + sinf(heading)*0.17f }, 0.03f, (Color){210,150,120,255});
}

static void draw_vehicle(float wx, float wy, float heading, Color body) {
    rlPushMatrix();
    rlTranslatef(wx, 0, wy);
    rlRotatef(-heading * RAD2DEG, 0, 1, 0);

    DrawCube((Vector3){0, 0.01f, 0}, 2.6f, 0.02f, 1.3f, (Color){0,0,0,110});

    Color tire = { 20, 20, 20, 255 };
    float wheel_r = 0.22f;
    float wx_off = 0.85f, wy_off = 0.55f;
    for (int i = 0; i < 4; i++) {
        float fx = (i & 1) ? wx_off : -wx_off;
        float fz = (i & 2) ? wy_off : -wy_off;
        DrawCylinderEx(
            (Vector3){ fx, wheel_r, fz - 0.05f },
            (Vector3){ fx, wheel_r, fz + 0.05f },
            wheel_r, wheel_r, 12, tire);
    }

    DrawCube((Vector3){0, 0.38f, 0}, 2.2f, 0.38f, 1.10f, body);
    DrawCubeWires((Vector3){0, 0.38f, 0}, 2.2f, 0.38f, 1.10f, color_lerp(body, BLACK, 0.5f));
    DrawCube((Vector3){0.55f, 0.58f, 0}, 0.9f, 0.05f, 1.0f, color_lerp(body, BLACK, 0.2f));
    Color cabin = color_lerp(body, BLACK, 0.25f);
    DrawCube((Vector3){-0.15f, 0.78f, 0}, 1.2f, 0.45f, 0.95f, cabin);
    DrawCube((Vector3){0.45f, 0.85f, 0}, 0.05f, 0.35f, 0.90f, (Color){60, 110, 160, 200});
    DrawCube((Vector3){-0.75f, 0.85f, 0}, 0.05f, 0.35f, 0.90f, (Color){60, 110, 160, 200});
    DrawSphere((Vector3){1.10f, 0.48f, -0.35f}, 0.08f, (Color){255, 245, 180, 255});
    DrawSphere((Vector3){1.10f, 0.48f,  0.35f}, 0.08f, (Color){255, 245, 180, 255});
    DrawSphere((Vector3){-1.10f, 0.48f, -0.35f}, 0.08f, (Color){200, 30, 30, 255});
    DrawSphere((Vector3){-1.10f, 0.48f,  0.35f}, 0.08f, (Color){200, 30, 30, 255});

    rlPopMatrix();
}

static Color player_shirt  = (Color){  30,  90, 180, 255 };
static Color player_pants  = (Color){  40,  40,  50, 255 };
static Color civ_shirts[5] = {
    (Color){180,  60,  60, 255},
    (Color){ 60, 160, 100, 255},
    (Color){200, 160,  80, 255},
    (Color){140,  90, 160, 255},
    (Color){100, 150, 180, 255},
};
static Color civ_pants[3] = {
    (Color){ 60,  50,  40, 255},
    (Color){ 80,  80,  90, 255},
    (Color){ 40,  60,  40, 255},
};
static Color police_shirt = (Color){ 20,  40, 120, 255 };
static Color police_pants = (Color){ 20,  20,  30, 255 };

static Color vehicle_colors[6] = {
    (Color){200,  40,  40, 255},
    (Color){ 40,  80, 180, 255},
    (Color){220, 220, 220, 255},
    (Color){ 40,  40,  50, 255},
    (Color){200, 140,  40, 255},
    (Color){ 60, 140,  90, 255},
};

static void draw_entity_3d(const Entity* e) {
    if (!e->h.alive && e->type != AGENT_VEHICLE) {
        DrawCube((Vector3){e->t.x, 0.15f, e->t.y}, 0.8f, 0.2f, 0.4f,
                 (Color){90, 20, 20, 255});
        return;
    }
    switch (e->type) {
        case AGENT_PLAYER:
            /* In FPP, don't draw player model */
            if (g_camera_mode == CAM_FIRST_PERSON) break;
            draw_human(e->t.x, e->t.y, e->t.z, e->t.heading, player_shirt, player_pants,
                       1, e->gun.muzzle_flash);
            /* yellow marker above head */
            DrawSphere((Vector3){e->t.x, e->t.z + 1.75f, e->t.y}, 0.12f, (Color){255, 220, 40, 255});
            break;
        case AGENT_CIVILIAN: {
            unsigned h = hash_uv(e->id, 7);
            draw_human(e->t.x, e->t.y, e->t.z, e->t.heading,
                       civ_shirts[h % 5], civ_pants[(h >> 3) % 3], 0, 0);
        } break;
        case AGENT_POLICE:
            draw_human(e->t.x, e->t.y, e->t.z, e->t.heading, police_shirt, police_pants, 0, 0);
            DrawSphere((Vector3){e->t.x, e->t.z + 1.45f, e->t.y}, 0.10f, (Color){30, 50, 140, 255});
            break;
        case AGENT_VEHICLE: {
            unsigned h = hash_uv(e->id, 11);
            Color c = e->v.driver >= 0 ? vehicle_colors[0] : vehicle_colors[h % 6];
            draw_vehicle(e->t.x, e->t.y, e->t.heading, c);
        } break;
        default: break;
    }
}

/* -------- draw tracers -------- */
static void draw_tracers(void) {
    for (int i = 0; i < MAX_TRACERS; i++) {
        if (g_tracers[i].life > 0) {
            float alpha = (float)g_tracers[i].life / 4.0f;
            Color c = { 255, 255, 100, (unsigned char)(255 * alpha) };
            DrawLine3D(
                (Vector3){g_tracers[i].x1, 0.85f, g_tracers[i].y1},
                (Vector3){g_tracers[i].x2, 0.85f, g_tracers[i].y2}, c);
            /* thicker line via second pass */
            DrawLine3D(
                (Vector3){g_tracers[i].x1, 0.87f, g_tracers[i].y1},
                (Vector3){g_tracers[i].x2, 0.87f, g_tracers[i].y2}, c);
            g_tracers[i].life--;
        }
    }
}

/* -------- HUD -------- */
static void draw_hud_raylib(int hp, int stars, int in_veh) {
    int hud_y = WIN_H - 80;
    DrawRectangle(0, hud_y, WIN_W, 80, (Color){0, 0, 0, 180});
    /* HP */
    DrawText("HEALTH", 20, hud_y + 10, 14, (Color){220, 220, 220, 255});
    DrawRectangle(20, hud_y + 30, 260, 24, (Color){60, 20, 20, 255});
    int hpw = hp * 260 / 100; if (hpw < 0) hpw = 0;
    /* Gradient health bar: green->yellow->red */
    Color hp_col;
    if (hp > 60) hp_col = (Color){40, 200, 60, 255};
    else if (hp > 30) hp_col = (Color){220, 180, 40, 255};
    else hp_col = (Color){220, 40, 40, 255};
    DrawRectangle(20, hud_y + 30, hpw, 24, hp_col);
    DrawRectangleLines(20, hud_y + 30, 260, 24, WHITE);
    char hptxt[32]; snprintf(hptxt, sizeof(hptxt), "%d", hp);
    DrawText(hptxt, 140, hud_y + 34, 16, WHITE);

    /* WANTED */
    DrawText("WANTED", 320, hud_y + 10, 14, (Color){220, 220, 220, 255});
    for (int i = 0; i < WANTED_MAX; i++) {
        int cx = 330 + i * 36, cy = hud_y + 42;
        Color c = (i < stars) ? (Color){255, 210, 40, 255}
                              : (Color){70, 70, 70, 255};
        Vector2 p[5];
        for (int k = 0; k < 5; k++) {
            float ang = -PI/2 + k * (2*PI/5);
            p[k].x = cx + cosf(ang) * 14;
            p[k].y = cy + sinf(ang) * 14;
        }
        DrawTriangle((Vector2){(float)cx, (float)cy}, p[0], p[1], c);
        DrawTriangle((Vector2){(float)cx, (float)cy}, p[1], p[2], c);
        DrawTriangle((Vector2){(float)cx, (float)cy}, p[2], p[3], c);
        DrawTriangle((Vector2){(float)cx, (float)cy}, p[3], p[4], c);
        DrawTriangle((Vector2){(float)cx, (float)cy}, p[4], p[0], c);
    }

    /* Mode pill */
    DrawText(in_veh ? "[ CAR ]" : "[ FOOT ]", 540, hud_y + 30, 22,
             in_veh ? (Color){255, 120, 120, 255} : (Color){200, 230, 255, 255});

    /* View mode */
    const char* view_txt = (g_camera_mode == CAM_FIRST_PERSON) ? "FPP" : "TPP";
    DrawText(view_txt, 680, hud_y + 30, 22, (Color){180, 220, 255, 255});

    /* Controls */
    DrawText("WASD move  SHIFT run  SPACE jump  E car  LMB shoot  R reload  V view  ESC quit",
             WIN_W - 750, hud_y + 56, 12, (Color){150, 150, 150, 255});

    DrawFPS(WIN_W - 90, 10);
}

/* Extended HUD with ammo display */
void renderer_draw_hud_extended(int hp, int max_hp, int stars, int in_veh,
                                int ammo, int mag_size, int reloading,
                                float reload_progress) {
    (void)max_hp;
    draw_hud_raylib(hp, stars, in_veh);

    int hud_y = WIN_H - 80;

    /* Ammo display */
    int ammo_x = WIN_W - 280;
    DrawText("AMMO", ammo_x, hud_y + 10, 14, (Color){220, 220, 220, 255});

    if (reloading) {
        /* Reload progress bar */
        DrawRectangle(ammo_x, hud_y + 30, 200, 20, (Color){40, 40, 40, 255});
        int prog_w = (int)(reload_progress * 200);
        DrawRectangle(ammo_x, hud_y + 30, prog_w, 20, (Color){255, 180, 40, 255});
        DrawRectangleLines(ammo_x, hud_y + 30, 200, 20, WHITE);
        DrawText("RELOADING...", ammo_x + 50, hud_y + 33, 14, WHITE);
    } else {
        /* Bullet indicators */
        for (int i = 0; i < mag_size; i++) {
            int bx = ammo_x + i * 26;
            Color bc = (i < ammo) ? (Color){255, 220, 80, 255} : (Color){60, 60, 60, 255};
            DrawRectangle(bx, hud_y + 30, 20, 20, bc);
            DrawRectangleLines(bx, hud_y + 30, 20, 20, (Color){120, 120, 120, 255});
        }
        char ammo_txt[16];
        snprintf(ammo_txt, sizeof(ammo_txt), "%d / INF", ammo);
        DrawText(ammo_txt, ammo_x + mag_size * 26 + 10, hud_y + 33, 14, WHITE);
    }

    /* Crosshair */
    int cx = WIN_W / 2, cy = WIN_H / 2 - 40;
    DrawLine(cx - 12, cy, cx - 4, cy, (Color){255, 255, 255, 200});
    DrawLine(cx + 4, cy, cx + 12, cy, (Color){255, 255, 255, 200});
    DrawLine(cx, cy - 12, cx, cy - 4, (Color){255, 255, 255, 200});
    DrawLine(cx, cy + 4, cx, cy + 12, (Color){255, 255, 255, 200});
    DrawCircleLines(cx, cy, 2, (Color){255, 255, 255, 180});
}

/* -------- lifecycle -------- */
static void ray_init(int w, int h) {
    (void)w; (void)h;
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(WIN_W, WIN_H, "IVATG");
    SetExitKey(0);  /* Disable ESC closing window — we handle it in game */
    SetTargetFPS(60);
    g_cam.position   = (Vector3){ CAM_DIST, CAM_HEIGHT, CAM_DIST };
    g_cam.target     = (Vector3){ 0, 0, 0 };
    g_cam.up         = (Vector3){ 0, 1, 0 };
    g_cam.fovy       = CAM_FOV_ORTHO;
    g_cam.projection = CAMERA_ORTHOGRAPHIC;
    memset(g_tracers, 0, sizeof(g_tracers));
}

static void ray_begin(float cx, float cy) {
    g_cam_target = (Vector3){ cx, 0, cy };
    g_cam.target = g_cam_target;

    if (g_camera_mode == CAM_FIRST_PERSON) {
        /* FPP handled externally; this sets up a default */
        g_cam.projection = CAMERA_PERSPECTIVE;
        g_cam.fovy = 70.0f;
    } else {
        g_cam.position = (Vector3){ cx + CAM_DIST, CAM_HEIGHT, cy + CAM_DIST };
        g_cam.projection = CAMERA_ORTHOGRAPHIC;
        g_cam.fovy = CAM_FOV_ORTHO;
    }

    BeginDrawing();
    ClearBackground((Color){90, 130, 170, 255});
    DrawRectangleGradientV(0, 0, WIN_W, WIN_H,
        (Color){80, 110, 150, 255}, (Color){160, 180, 200, 255});
    BeginMode3D(g_cam);
}

static void ray_draw_tile_cb(int x, int y, TileType t, float cx, float cy) {
    (void)cx; (void)cy;
    switch (t) {
        case TILE_ROAD:     draw_road_tile(x, y); break;
        case TILE_SIDEWALK: draw_sidewalk_tile(x, y); break;
        case TILE_BUILDING: draw_building_tile(x, y); break;
        case TILE_GRASS:    draw_grass_tile(x, y); break;
        default: break;
    }
}
static void ray_draw_entity_cb(const Entity* e, float cx, float cy) {
    (void)cx; (void)cy;
    draw_entity_3d(e);
}
static void ray_draw_hud_cb(int hp, int stars, int wmax, int in_veh) {
    (void)wmax;
    draw_hud_raylib(hp, stars, in_veh);
}
static void ray_present(void) {
    draw_tracers();
    EndMode3D();
    /* EndDrawing is called by renderer_render_world after HUD */
}
static void ray_shutdown(void) { CloseWindow(); }

int renderer_mouse_world(float* wx, float* wy) {
    if (!IsWindowReady()) return 0;
    Vector2 mp = GetMousePosition();
    Ray ray = GetMouseRay(mp, g_cam);
    if (fabsf(ray.direction.y) < 1e-5f) return 0;
    float t = -ray.position.y / ray.direction.y;
    if (t < 0) return 0;
    Vector3 p = {
        ray.position.x + ray.direction.x * t,
        0.0f,
        ray.position.z + ray.direction.z * t,
    };
    *wx = p.x;
    *wy = p.z;
    return 1;
}

/* ============================================================
 *  TITLE / MENU / SETTINGS / BUSTED SCREENS
 * ============================================================ */

void renderer_draw_title(float timer) {
    BeginDrawing();
    /* Dark city gradient background */
    DrawRectangleGradientV(0, 0, WIN_W, WIN_H,
        (Color){10, 10, 20, 255}, (Color){30, 15, 40, 255});

    /* City silhouette at bottom */
    for (int i = 0; i < WIN_W; i += 20) {
        unsigned h = hash_uv(i, 42);
        int bh = 80 + (h % 180);
        DrawRectangle(i, WIN_H - bh, 18, bh, (Color){15, 15, 25, 255});
        /* windows */
        for (int wy = WIN_H - bh + 10; wy < WIN_H - 10; wy += 16) {
            for (int wx = i + 3; wx < i + 16; wx += 6) {
                unsigned wh = hash_uv(wx, wy);
                Color wc = (wh & 3) ? (Color){255, 220, 120, (unsigned char)(80 + (wh & 0x7F))}
                                    : (Color){20, 25, 35, 255};
                DrawRectangle(wx, wy, 4, 6, wc);
            }
        }
    }

    /* Title fade-in */
    float alpha = timer < 2.0f ? timer / 2.0f : 1.0f;
    unsigned char a = (unsigned char)(alpha * 255);

    /* Glow effect behind title */
    for (int glow = 3; glow >= 0; glow--) {
        int offset = glow * 3;
        unsigned char ga = (unsigned char)(a * 0.15f / (glow + 1));
        DrawText("IVATG", WIN_W/2 - 180 + offset, 200 + offset, 120, (Color){255, 80, 40, ga});
    }

    /* Title text */
    DrawText("IVATG", WIN_W/2 - 180, 200, 120, (Color){255, 120, 40, a});
    DrawText("IVATG", WIN_W/2 - 178, 198, 120, (Color){255, 200, 80, (unsigned char)(a * 0.5f)});

    /* Subtitle */
    if (timer > 1.5f) {
        float sub_a = (timer - 1.5f) / 1.0f;
        if (sub_a > 1.0f) sub_a = 1.0f;
        DrawText("THE SIMULATION", WIN_W/2 - 120, 330, 24,
                 (Color){200, 200, 220, (unsigned char)(sub_a * 255)});
    }

    /* Pulsing "press enter" */
    if (timer > 2.5f) {
        float pulse = sinf(timer * 3.0f) * 0.3f + 0.7f;
        unsigned char pa = (unsigned char)(pulse * 255);
        DrawText("PRESS ENTER", WIN_W/2 - 100, 500, 28, (Color){200, 200, 200, pa});
    }

    /* Version */
    DrawText("v1.0", WIN_W - 80, WIN_H - 30, 16, (Color){100, 100, 100, 200});

    EndDrawing();
}

void renderer_draw_menu(int selected) {
    BeginDrawing();
    DrawRectangleGradientV(0, 0, WIN_W, WIN_H,
        (Color){10, 10, 20, 255}, (Color){30, 15, 40, 255});

    /* City silhouette */
    for (int i = 0; i < WIN_W; i += 20) {
        unsigned h = hash_uv(i, 42);
        int bh = 80 + (h % 180);
        DrawRectangle(i, WIN_H - bh, 18, bh, (Color){15, 15, 25, 255});
    }

    /* Title (smaller) */
    DrawText("IVATG", WIN_W/2 - 120, 80, 80, (Color){255, 120, 40, 255});

    /* Menu items */
    const char* items[] = { "START GAME", "SETTINGS", "QUIT" };
    for (int i = 0; i < 3; i++) {
        int y = 300 + i * 80;
        int w = MeasureText(items[i], 36);
        int x = WIN_W/2 - w/2;

        if (i == selected) {
            /* Selected highlight */
            DrawRectangle(x - 30, y - 10, w + 60, 56, (Color){255, 120, 40, 40});
            DrawRectangleLinesEx((Rectangle){(float)(x - 30), (float)(y - 10), (float)(w + 60), 56.0f}, 2,
                                (Color){255, 140, 60, 200});
            DrawText(items[i], x, y, 36, (Color){255, 200, 80, 255});
            /* Arrow indicator */
            DrawText(">", x - 25, y, 36, (Color){255, 160, 40, 255});
        } else {
            DrawText(items[i], x, y, 36, (Color){180, 180, 200, 200});
        }
    }

    /* Nav hint */
    DrawText("UP/DOWN to navigate   ENTER to select", WIN_W/2 - 200, WIN_H - 50, 16,
             (Color){120, 120, 140, 200});

    EndDrawing();
}

void renderer_draw_settings(int selected, int waiting_key, void* keybinds_ptr) {
    KeyBindings* kb = (KeyBindings*)keybinds_ptr;
    BeginDrawing();
    DrawRectangleGradientV(0, 0, WIN_W, WIN_H,
        (Color){15, 15, 25, 255}, (Color){25, 20, 35, 255});

    DrawText("SETTINGS - KEY BINDINGS", WIN_W/2 - 220, 40, 36, (Color){255, 160, 60, 255});
    DrawText("Select an action and press any key to rebind", WIN_W/2 - 250, 90, 18,
             (Color){160, 160, 180, 200});

    for (int i = 0; i < KEYBIND_COUNT; i++) {
        int y = 140 + i * 48;
        int* key_ptr = keybind_get_ptr(kb, i);
        const char* name = keybind_action_name(i);

        if (i == selected) {
            DrawRectangle(200, y - 5, WIN_W - 400, 42, (Color){255, 120, 40, 30});
            DrawRectangleLinesEx((Rectangle){200, (float)(y - 5), (float)(WIN_W - 400), 42}, 1,
                                (Color){255, 140, 60, 150});
        }

        Color txt_col = (i == selected) ? (Color){255, 200, 80, 255} : (Color){200, 200, 210, 220};
        DrawText(name, 220, y + 5, 22, txt_col);

        /* Current key */
        if (waiting_key >= 0 && i == waiting_key) {
            DrawText("[ PRESS A KEY... ]", WIN_W - 450, y + 5, 22, (Color){255, 80, 80, 255});
        } else {
            char kbuf[32];
            if (key_ptr) {
                int k = *key_ptr;
                if (k >= 32 && k < 127) snprintf(kbuf, sizeof(kbuf), "[ %c ]", (char)k);
                else if (k == 340) snprintf(kbuf, sizeof(kbuf), "[ L-SHIFT ]");
                else if (k == 341) snprintf(kbuf, sizeof(kbuf), "[ L-CTRL ]");
                else if (k == 342) snprintf(kbuf, sizeof(kbuf), "[ L-ALT ]");
                else if (k == 32)  snprintf(kbuf, sizeof(kbuf), "[ SPACE ]");
                else if (k == 265) snprintf(kbuf, sizeof(kbuf), "[ UP ]");
                else if (k == 264) snprintf(kbuf, sizeof(kbuf), "[ DOWN ]");
                else if (k == 263) snprintf(kbuf, sizeof(kbuf), "[ LEFT ]");
                else if (k == 262) snprintf(kbuf, sizeof(kbuf), "[ RIGHT ]");
                else snprintf(kbuf, sizeof(kbuf), "[ KEY_%d ]", k);
            } else {
                snprintf(kbuf, sizeof(kbuf), "[ ? ]");
            }
            DrawText(kbuf, WIN_W - 450, y + 5, 22, (Color){150, 200, 255, 220});
        }
    }

    DrawText("ENTER to rebind   ESC to go back", WIN_W/2 - 180, WIN_H - 50, 16,
             (Color){120, 120, 140, 200});

    EndDrawing();
}

void renderer_draw_busted(float timer) {
    BeginDrawing();

    /* Red flash + dark overlay */
    float flash = (timer < 0.5f) ? (1.0f - timer * 2.0f) : 0.0f;
    DrawRectangle(0, 0, WIN_W, WIN_H, (Color){180, 20, 20, (unsigned char)(flash * 200)});
    DrawRectangle(0, 0, WIN_W, WIN_H, (Color){0, 0, 0, 160});

    /* Cinematic letterbox */
    DrawRectangle(0, 0, WIN_W, 80, BLACK);
    DrawRectangle(0, WIN_H - 80, WIN_W, 80, BLACK);

    /* BUSTED text with glow */
    const char* txt = "BUSTED";
    int tw = MeasureText(txt, 100);
    int tx = WIN_W/2 - tw/2;
    int ty = WIN_H/2 - 50;

    /* Glow layers */
    for (int g = 4; g >= 0; g--) {
        unsigned char ga = (unsigned char)(30 / (g + 1));
        DrawText(txt, tx + g*2, ty + g*2, 100, (Color){255, 40, 40, ga});
    }
    DrawText(txt, tx, ty, 100, (Color){255, 60, 60, 255});
    DrawText(txt, tx + 2, ty - 2, 100, (Color){255, 150, 120, 100});

    /* Subtitle */
    if (timer > 1.0f) {
        float sa = (timer - 1.0f) / 0.5f;
        if (sa > 1.0f) sa = 1.0f;
        DrawText("You have been neutralized by police",
                 WIN_W/2 - 200, ty + 120, 20, (Color){200, 200, 200, (unsigned char)(sa * 255)});
    }

    /* Restart countdown */
    if (timer > 1.5f) {
        float remain = 3.0f - timer;
        if (remain < 0) remain = 0;
        char buf[64];
        snprintf(buf, sizeof(buf), "Restarting in %.0f...", remain + 1);
        DrawText(buf, WIN_W/2 - 100, ty + 160, 20, (Color){180, 180, 180, 220});
    }

    EndDrawing();
}

void renderer_draw_pause(int selected) {
    BeginDrawing();

    /* Semi-transparent dark overlay */
    DrawRectangle(0, 0, WIN_W, WIN_H, (Color){0, 0, 0, 160});

    /* Title */
    DrawText("PAUSED", WIN_W/2 - 100, 180, 60, (Color){255, 200, 80, 255});

    /* Menu items */
    const char* items[] = { "RESUME", "SETTINGS", "QUIT TO MENU" };
    for (int i = 0; i < 3; i++) {
        int y = 320 + i * 70;
        int w = MeasureText(items[i], 32);
        int x = WIN_W/2 - w/2;

        if (i == selected) {
            DrawRectangle(x - 25, y - 8, w + 50, 48, (Color){255, 120, 40, 40});
            DrawRectangleLinesEx((Rectangle){(float)(x - 25), (float)(y - 8), (float)(w + 50), 48.0f}, 2,
                                (Color){255, 140, 60, 200});
            DrawText(items[i], x, y, 32, (Color){255, 200, 80, 255});
            DrawText(">", x - 22, y, 32, (Color){255, 160, 40, 255});
        } else {
            DrawText(items[i], x, y, 32, (Color){180, 180, 200, 200});
        }
    }

    DrawText("ESC to resume   UP/DOWN + ENTER to select", WIN_W/2 - 230, WIN_H - 50, 16,
             (Color){120, 120, 140, 200});

    EndDrawing();
}

#else
/* ============================================================
 *  NCURSES FALLBACK
 * ============================================================ */
#include <ncurses.h>
static int g_vw = 80, g_vh = 24;
static void nc_init(int w, int h) {
    initscr(); noecho(); cbreak(); curs_set(0);
    keypad(stdscr, TRUE); nodelay(stdscr, TRUE);
    if (has_colors()) { start_color(); use_default_colors();
        init_pair(1, COLOR_WHITE, -1); init_pair(2, COLOR_GREEN, -1);
        init_pair(3, COLOR_YELLOW, -1); init_pair(4, COLOR_BLUE, -1);
        init_pair(5, COLOR_RED, -1);    init_pair(6, COLOR_RED, -1);
    }
    int r, c; getmaxyx(stdscr, r, c);
    g_vw = c < w ? c : w; g_vh = (r - 2) < h ? (r - 2) : h;
}
static void nc_begin(float cx, float cy) { (void)cx; (void)cy; erase(); }
static void nc_draw_tile(int x, int y, TileType t, float cx, float cy) {
    int sx = (int)(x - cx + g_vw*0.5f), sy = (int)(y - cy + g_vh*0.5f);
    if (sx<0||sy<0||sx>=g_vw||sy>=g_vh) return;
    char ch = '.'; int col = 1;
    if (t == TILE_BUILDING) { ch = '#'; col = 5; }
    else if (t == TILE_SIDEWALK) { ch = ':'; col = 1; }
    else if (t == TILE_GRASS) { ch = ','; col = 2; }
    attron(COLOR_PAIR(col)); mvaddch(sy, sx, ch); attroff(COLOR_PAIR(col));
}
static void nc_draw_entity(const Entity* e, float cx, float cy) {
    int sx = (int)(e->t.x - cx + g_vw*0.5f), sy = (int)(e->t.y - cy + g_vh*0.5f);
    if (sx<0||sy<0||sx>=g_vw||sy>=g_vh) return;
    int col = 1; char g = '?';
    switch (e->type) {
        case AGENT_PLAYER: col = 3; g = '@'; break;
        case AGENT_CIVILIAN: col = 2; g = 'c'; break;
        case AGENT_POLICE: col = 4; g = 'P'; break;
        case AGENT_VEHICLE: col = 6; g = 'V'; break;
        default: break;
    }
    attron(COLOR_PAIR(col) | A_BOLD); mvaddch(sy, sx, g); attroff(COLOR_PAIR(col) | A_BOLD);
}
static void nc_draw_hud(int hp, int stars, int wmax, int in_veh) {
    (void)wmax;
    mvprintw(g_vh, 0, " HP:%3d  Wanted:", hp);
    for (int i = 0; i < stars; i++) mvaddch(g_vh, 16 + i, '*');
    for (int i = stars; i < WANTED_MAX; i++) mvaddch(g_vh, 16 + i, '-');
    mvprintw(g_vh, 22, "  %s", in_veh ? "[CAR]" : "[FOOT]");
}
static void nc_present(void) { refresh(); }
static void nc_shutdown(void) { endwin(); }

int renderer_mouse_world(float* wx, float* wy) { (void)wx; (void)wy; return 0; }
void renderer_set_camera_mode(int mode) { (void)mode; }
int  renderer_get_camera_mode(void) { return CAM_THIRD_PERSON; }
void renderer_draw_title(float timer) { (void)timer; }
void renderer_draw_menu(int selected) { (void)selected; }
void renderer_draw_settings(int selected, int waiting_key, void* keybinds) { (void)selected; (void)waiting_key; (void)keybinds; }
void renderer_draw_busted(float timer) { (void)timer; }
void renderer_draw_pause(int selected) { (void)selected; }
void renderer_draw_hud_extended(int hp, int max_hp, int stars, int in_veh,
                                int ammo, int mag_size, int reloading,
                                float reload_progress) {
    (void)max_hp; (void)ammo; (void)mag_size; (void)reloading; (void)reload_progress;
    nc_draw_hud(hp, stars, WANTED_MAX, in_veh);
}
void renderer_add_tracer(float x1, float y1, float x2, float y2) { (void)x1; (void)y1; (void)x2; (void)y2; }
#endif

Renderer* renderer_get(void) {
#ifdef USE_RAYLIB
    g_renderer.init        = ray_init;
    g_renderer.begin       = ray_begin;
    g_renderer.draw_tile   = ray_draw_tile_cb;
    g_renderer.draw_entity = ray_draw_entity_cb;
    g_renderer.draw_hud    = ray_draw_hud_cb;
    g_renderer.present     = ray_present;
    g_renderer.shutdown    = ray_shutdown;
    g_renderer.viewport_w  = 32;
    g_renderer.viewport_h  = 32;
#else
    g_renderer.init        = nc_init;
    g_renderer.begin       = nc_begin;
    g_renderer.draw_tile   = nc_draw_tile;
    g_renderer.draw_entity = nc_draw_entity;
    g_renderer.draw_hud    = nc_draw_hud;
    g_renderer.present     = nc_present;
    g_renderer.shutdown    = nc_shutdown;
    g_renderer.viewport_w  = 80;
    g_renderer.viewport_h  = 24;
#endif
    return &g_renderer;
}

void renderer_render_world(const World* w, EntityPool* pool,
                           const Entity* player, float cam_x, float cam_y) {
    Renderer* r = renderer_get();

#ifdef USE_RAYLIB
    /* Set up FPP camera if needed */
    if (g_camera_mode == CAM_FIRST_PERSON && player) {
        float head = player->t.heading;
        g_cam.position = (Vector3){ player->t.x, player->t.z + 1.3f, player->t.y };
        g_cam.target   = (Vector3){ player->t.x + cosf(head)*5.0f, player->t.z + 1.3f, player->t.y + sinf(head)*5.0f };
        g_cam.projection = CAMERA_PERSPECTIVE;
        g_cam.fovy = 70.0f;
    }
#endif

    r->begin(cam_x, cam_y);

    int half = r->viewport_w;
    int x0 = (int)cam_x - half; if (x0 < 0) x0 = 0;
    int y0 = (int)cam_y - half; if (y0 < 0) y0 = 0;
    int x1 = (int)cam_x + half; if (x1 > w->w) x1 = w->w;
    int y1 = (int)cam_y + half; if (y1 > w->h) y1 = w->h;

    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            r->draw_tile(x, y, world_tile(w, x, y), cam_x, cam_y);
        }
    }
    for (int i = 0; i < MAX_ENTITIES; i++) {
        const Entity* e = &pool->entities[i];
        if (!e->active) continue;
        if (e->vehicle >= 0) continue;
        if (e->t.x < x0 - 2 || e->t.x > x1 + 2) continue;
        if (e->t.y < y0 - 2 || e->t.y > y1 + 2) continue;
        r->draw_entity(e, cam_x, cam_y);
    }

    /* present() calls EndMode3D but NOT EndDrawing, so we can draw 2D HUD */
    r->present();

#ifdef USE_RAYLIB
    /* Draw extended HUD in 2D after EndMode3D */
    if (player) {
        renderer_draw_hud_extended(
            player->h.hp, player->h.max_hp, player->w.stars,
            player->vehicle >= 0 ? 1 : 0,
            player->gun.ammo_in_mag, player->gun.mag_size,
            player->gun.reloading ? 1 : 0,
            player->gun.reloading
                ? 1.0f - (float)player->gun.reload_timer / PISTOL_RELOAD_TICKS
                : 0.0f
        );
    }
    EndDrawing();
#endif
}
