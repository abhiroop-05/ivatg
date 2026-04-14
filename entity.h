#ifndef ENTITY_H
#define ENTITY_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef enum {
    AGENT_NONE = 0,
    AGENT_PLAYER,
    AGENT_CIVILIAN,
    AGENT_POLICE,
    AGENT_VEHICLE
} AgentType;

typedef enum {
    AI_IDLE = 0,
    AI_WALK_WAYPOINT,
    AI_FLEE,
    AI_PATROL,
    AI_ALERT,
    AI_PURSUE,
    AI_ENGAGE,
    AI_RETURN
} AIState;

typedef struct {
    float x, y;
    float z;             /* vertical position (jump) */
    float vz;            /* vertical velocity */
    float heading;       /* radians */
    float speed;         /* forward scalar speed (vehicles) */
} Xform;

typedef struct {
    float vx, vy;
    float ax, ay;
    float friction;
    float radius;
} Physics;

typedef struct {
    int hp;
    int max_hp;
    bool alive;
} Health;

typedef struct {
    int32_t driver;             /* entity id or -1 */
    int32_t passengers[3];
    int     passenger_count;
} VehicleOccupancy;

typedef struct {
    int   stars;
    int   timer;
    int   last_crime_tick;
    int   last_sighted_tick;
    float last_known_x, last_known_y;
} WantedLevel;

typedef struct {
    AIState state;
    int32_t target;             /* entity id or -1 */
    int     waypoint_x, waypoint_y;
    int     path[PATH_MAX_LEN * 2]; /* pairs of x,y */
    int     path_len;
    int     path_idx;
    int     replan_cooldown;
    int     attack_cooldown;
} AIBehavior;

/* Weapon state for player */
typedef struct {
    int   ammo_in_mag;       /* 0..PISTOL_MAG_SIZE */
    int   mag_size;          /* PISTOL_MAG_SIZE */
    bool  reloading;
    int   reload_timer;      /* ticks remaining */
    int   shoot_cooldown;    /* ticks until next shot allowed */
    int   npc_kills;         /* total NPC kills for wanted trigger */
    int   muzzle_flash;      /* frame counter for flash effect */
} WeaponState;

typedef struct {
    int32_t   id;
    bool      active;
    AgentType type;

    Xform             t;
    Physics           p;
    Health            h;
    VehicleOccupancy  v;   /* meaningful if type == AGENT_VEHICLE */
    WantedLevel       w;   /* meaningful for PLAYER */
    AIBehavior        ai;  /* for CIVILIAN/POLICE */
    WeaponState       gun; /* meaningful for PLAYER */

    int32_t   vehicle;          /* -1 if on foot; otherwise vehicle entity id (for PLAYER/NPC driver occupying) */
    int       color;            /* renderer hint */
    char      glyph;            /* terminal glyph */

    /* Vehicle control state (used when this entity IS a vehicle) */
    float steer_input;          /* -1..1 */
    float throttle_input;       /* -1..1 */
    bool  brake_input;
} Entity;

typedef struct {
    Entity entities[MAX_ENTITIES];
    int    count;
    int    next_free_hint;
} EntityPool;

void entity_pool_init(EntityPool* pool);
Entity* entity_create(EntityPool* pool, AgentType type, float x, float y);
void    entity_destroy(EntityPool* pool, int32_t id);
Entity* entity_get(EntityPool* pool, int32_t id);

#endif
