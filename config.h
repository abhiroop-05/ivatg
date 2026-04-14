#ifndef CONFIG_H
#define CONFIG_H

#define TICK_HZ              60
#define TICK_DT              (1.0f / (float)TICK_HZ)

#define WORLD_W              128
#define WORLD_H              128

#define MAX_ENTITIES         1024
#define SPATIAL_CELL_SIZE    4
#define SPATIAL_GRID_W       (WORLD_W / SPATIAL_CELL_SIZE)
#define SPATIAL_GRID_H       (WORLD_H / SPATIAL_CELL_SIZE)
#define SPATIAL_BUCKET_CAP   32

#define VIEWPORT_W           80
#define VIEWPORT_H           30

/* Physics */
#define PED_ACCEL            40.0f
#define PED_MAX_SPEED        4.0f
#define PED_FRICTION         8.0f
#define PED_RADIUS           0.45f

/* Sprint */
#define PED_SPRINT_SPEED     8.0f
#define PED_SPRINT_ACCEL     80.0f

/* Jump */
#define PED_JUMP_VEL         6.0f
#define PED_GRAVITY          20.0f

#define VEH_ACCEL            12.0f
#define VEH_BRAKE            24.0f
#define VEH_MAX_SPEED        14.0f
#define VEH_REVERSE_SPEED    4.0f
#define VEH_FRICTION         1.2f
#define VEH_TURN_RATE        2.2f
#define VEH_RADIUS           0.9f
#define VEH_MASS             1200.0f

/* AI */
#define POLICE_SIGHT_RANGE   20.0f
#define POLICE_ATTACK_RANGE  1.6f
#define POLICE_ATTACK_DMG    8
#define POLICE_ATTACK_COOLDOWN_TICKS 20
#define CIV_FLEE_RANGE       8.0f
#define CIV_WALK_SPEED       1.8f
#define POLICE_PATROL_SPEED  2.2f
#define POLICE_CHASE_SPEED   5.5f

/* Wanted */
#define WANTED_MAX           5
#define WANTED_DECAY_TICKS   (TICK_HZ * 12)
#define WANTED_RAM_DELTA     1
#define WANTED_ATTACK_DELTA  1
#define WANTED_KILL_DELTA    2
#define WANTED_SHOOT_KILL_THRESHOLD 3  /* kills before police chase */

/* Weapon */
#define PISTOL_MAG_SIZE      7
#define PISTOL_RELOAD_TICKS  (int)(TICK_HZ * 1.5f)
#define PISTOL_SHOOT_COOLDOWN 8       /* ticks between shots */
#define PISTOL_DAMAGE        35
#define PISTOL_RANGE         15.0f

/* Spawning */
#define MAX_CIVILIANS        40
#define MAX_POLICE_PER_STAR  3
#define MAX_PARKED_VEHICLES  20
#define POLICE_SPAWN_INTERVAL_TICKS (TICK_HZ * 3)

/* A* */
#define ASTAR_MAX_NODES      (WORLD_W * WORLD_H)
#define PATH_MAX_LEN         256

#endif
