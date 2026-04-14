#include "input.h"
#include "renderer.h"
#include "keybinds.h"
#include <string.h>

/* Global keybindings — accessible from settings screen */
KeyBindings g_keybinds;

#ifdef USE_RAYLIB
#include "raylib.h"

void input_init(void) {
    keybinds_default(&g_keybinds);
}
void input_shutdown(void) {}

void input_poll(InputState* s) {
    memset(s, 0, sizeof(*s));
    if (WindowShouldClose()) s->quit = true;

    /* Movement */
    if (IsKeyDown(g_keybinds.move_up)   || IsKeyDown(KEY_UP))    s->up = true;
    if (IsKeyDown(g_keybinds.move_down)  || IsKeyDown(KEY_DOWN))  s->down = true;
    if (IsKeyDown(g_keybinds.move_left)  || IsKeyDown(KEY_LEFT))  s->left = true;
    if (IsKeyDown(g_keybinds.move_right) || IsKeyDown(KEY_RIGHT)) s->right = true;

    /* Actions */
    if (IsKeyPressed(g_keybinds.enter_exit))  s->enter_exit = true;
    if (IsKeyPressed(KEY_ESCAPE))             s->quit = true;

    /* Sprint (hold) */
    if (IsKeyDown(g_keybinds.sprint))         s->sprint = true;

    /* Jump (press, on foot) */
    if (IsKeyPressed(g_keybinds.jump))        s->jump = true;

    /* Shoot — left-click or shoot key */
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { s->shoot = true; s->mouse_left = true; }
    if (IsKeyDown(g_keybinds.shoot))          s->shoot = true;

    /* Reload */
    if (IsKeyPressed(g_keybinds.reload))      s->reload = true;

    /* Brake (in vehicle — uses Space) */
    if (IsKeyDown(g_keybinds.brake))          s->brake = true;

    /* Toggle FPP/TPP */
    if (IsKeyPressed(g_keybinds.toggle_view)) s->toggle_view = true;

    /* Legacy attack (kept for backward compat) */
    s->attack = s->shoot;

    /* Mouse world position */
    float mx = 0, my = 0;
    if (renderer_mouse_world(&mx, &my)) {
        s->mouse_valid = true;
        s->mouse_wx = mx;
        s->mouse_wy = my;
    }
}

#else
#include <ncurses.h>

typedef struct { bool held[12]; int last_tick_seen[12]; } HeldKeys;
static HeldKeys g_keys;
static int g_tick = 0;

void input_init(void) {
    memset(&g_keys, 0, sizeof(g_keys));
    keybinds_default(&g_keybinds);
}
void input_shutdown(void) {}

static void mark(int i) { g_keys.held[i] = true; g_keys.last_tick_seen[i] = g_tick; }

void input_poll(InputState* s) {
    g_tick++;
    memset(s, 0, sizeof(*s));
    int ch;
    while ((ch = getch()) != ERR) {
        switch (ch) {
            case 'w': case 'W': case KEY_UP:    mark(0); break;
            case 's': case 'S': case KEY_DOWN:  mark(1); break;
            case 'a': case 'A': case KEY_LEFT:  mark(2); break;
            case 'd': case 'D': case KEY_RIGHT: mark(3); break;
            case 'e': case 'E':                 mark(4); break;
            case ' ':                           mark(5); mark(7); mark(9); break;
            case 27: case 'q': case 'Q':        mark(6); break;
            case 'f': case 'F':                 mark(7); break;
            case 'r': case 'R':                 mark(8); break;
            case 'v': case 'V':                 mark(10); break;
        }
    }
    for (int i = 0; i < 12; i++)
        if (g_keys.held[i] && (g_tick - g_keys.last_tick_seen[i]) > 4) g_keys.held[i] = false;
    s->up = g_keys.held[0]; s->down = g_keys.held[1];
    s->left = g_keys.held[2]; s->right = g_keys.held[3];
    s->enter_exit = g_keys.held[4]; s->brake = g_keys.held[5];
    s->quit = g_keys.held[6]; s->shoot = g_keys.held[7];
    s->reload = g_keys.held[8]; s->jump = g_keys.held[9];
    s->toggle_view = g_keys.held[10]; s->attack = s->shoot;
    if (s->enter_exit) g_keys.held[4] = false;
    if (s->toggle_view) g_keys.held[10] = false;
}
#endif
