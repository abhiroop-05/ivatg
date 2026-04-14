#ifndef KEYBINDS_H
#define KEYBINDS_H

#include <stddef.h>  /* for NULL */

#ifdef USE_RAYLIB
#include "raylib.h"
#else
/* Fallback key codes matching ncurses / ASCII */
#define KEY_W 'w'
#define KEY_A 'a'
#define KEY_S 's'
#define KEY_D 'd'
#endif

typedef struct {
    int move_up;
    int move_down;
    int move_left;
    int move_right;
    int enter_exit;
    int shoot;       /* key alternative to mouse — default F */
    int reload;
    int sprint;
    int jump;
    int toggle_view; /* FPP/TPP toggle */
    int brake;       /* in vehicle */
} KeyBindings;

static inline void keybinds_default(KeyBindings* kb) {
#ifdef USE_RAYLIB
    kb->move_up     = KEY_W;
    kb->move_down   = KEY_S;
    kb->move_left   = KEY_A;
    kb->move_right  = KEY_D;
    kb->enter_exit  = KEY_E;
    kb->shoot       = KEY_F;
    kb->reload      = KEY_R;
    kb->sprint      = KEY_LEFT_SHIFT;
    kb->jump        = KEY_SPACE;
    kb->toggle_view = KEY_V;
    kb->brake       = KEY_SPACE;
#else
    kb->move_up     = 'w';
    kb->move_down   = 's';
    kb->move_left   = 'a';
    kb->move_right  = 'd';
    kb->enter_exit  = 'e';
    kb->shoot       = 'f';
    kb->reload      = 'r';
    kb->sprint      = 0;
    kb->jump        = ' ';
    kb->toggle_view = 'v';
    kb->brake       = ' ';
#endif
}

/* Action names for the settings screen */
static inline const char* keybind_action_name(int idx) {
    static const char* names[] = {
        "Move Up", "Move Down", "Move Left", "Move Right",
        "Enter/Exit Vehicle", "Shoot", "Reload",
        "Sprint", "Jump", "Toggle View", "Brake (Vehicle)"
    };
    if (idx < 0 || idx >= 11) return "?";
    return names[idx];
}

/* Get pointer to the idx-th key in the struct for iteration */
static inline int* keybind_get_ptr(KeyBindings* kb, int idx) {
    int* ptrs[] = {
        &kb->move_up, &kb->move_down, &kb->move_left, &kb->move_right,
        &kb->enter_exit, &kb->shoot, &kb->reload,
        &kb->sprint, &kb->jump, &kb->toggle_view, &kb->brake
    };
    if (idx < 0 || idx >= 11) return NULL;
    return ptrs[idx];
}

#define KEYBIND_COUNT 11

#endif
