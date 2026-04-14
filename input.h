#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

typedef struct {
    bool up, down, left, right;
    bool enter_exit;   /* E */
    bool brake;        /* Space (in vehicle) */
    bool quit;         /* ESC */
    bool attack;       /* Legacy: Space or left-click on foot (kept for compat) */

    /* New inputs */
    bool shoot;        /* Left-click or shoot key */
    bool reload;       /* R */
    bool sprint;       /* Shift */
    bool jump;         /* Space (on foot) */
    bool toggle_view;  /* V — FPP/TPP toggle (pressed this frame) */

    /* Mouse (world-space ground intersection) */
    bool  mouse_valid;
    float mouse_wx, mouse_wy;
    bool  mouse_left;
} InputState;

void input_init(void);
void input_shutdown(void);
void input_poll(InputState* s);

#endif
