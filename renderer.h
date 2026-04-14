#ifndef RENDERER_H
#define RENDERER_H

#include "world.h"
#include "entity.h"

typedef struct {
    void (*init)(int w, int h);
    void (*begin)(float cam_x, float cam_y);
    void (*draw_tile)(int x, int y, TileType t, float cam_x, float cam_y);
    void (*draw_entity)(const Entity* e, float cam_x, float cam_y);
    void (*draw_hud)(int hp, int stars, int wanted_max, int in_vehicle);
    void (*present)(void);
    void (*shutdown)(void);
    int  viewport_w, viewport_h;
} Renderer;

Renderer* renderer_get(void);

/* Returns 1 if mouse hit the ground plane, filling wx/wy world coords. */
int       renderer_mouse_world(float* wx, float* wy);
void      renderer_render_world(const World* w, EntityPool* pool,
                                const Entity* player, float cam_x, float cam_y);

/* Camera modes */
#define CAM_THIRD_PERSON 0
#define CAM_FIRST_PERSON 1

void renderer_set_camera_mode(int mode);
int  renderer_get_camera_mode(void);

/* Game state screens (Raylib only) */
void renderer_draw_title(float timer);
void renderer_draw_menu(int selected);
void renderer_draw_settings(int selected, int waiting_key, void* keybinds);
void renderer_draw_busted(float timer);
void renderer_draw_pause(int selected);

/* Extended HUD for weapon state */
void renderer_draw_hud_extended(int hp, int max_hp, int stars, int in_veh,
                                int ammo, int mag_size, int reloading,
                                float reload_progress);

/* Bullet tracer for shooting feedback */
void renderer_add_tracer(float x1, float y1, float x2, float y2);

#endif
