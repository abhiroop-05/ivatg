#ifndef WANTED_H
#define WANTED_H

#include "entity.h"
#include "world.h"

typedef enum {
    CRIME_NONE = 0,
    CRIME_RAM_PED,
    CRIME_ATTACK_PED,
    CRIME_KILL_PED,
    CRIME_SHOOT_NPC,
    CRIME_SEEN_BY_POLICE
} CrimeType;

void wanted_init(Entity* player);
void wanted_tick(EntityPool* pool, World* w, Entity* player, int tick);
void wanted_report_crime(Entity* player, CrimeType c, int tick);

/* Spawns police entities if current count < target_count based on stars.
 * Uses world road tiles for spawn points. */
void wanted_spawn_police(EntityPool* pool, World* w, Entity* player, int tick);

#endif
