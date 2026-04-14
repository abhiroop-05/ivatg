#ifndef AI_H
#define AI_H

#include "entity.h"
#include "world.h"

void ai_tick(EntityPool* pool, World* w, int32_t player_id, int tick, int player_wanted);

/* A* pathfinding on road/walkable tiles.
 * walkable_mask: 0 = road-only, 1 = road+sidewalk.
 * Fills path_xy with interleaved x,y; returns number of waypoints. */
int  ai_astar(World* w, int sx, int sy, int tx, int ty,
              int walkable_mask, int* path_xy, int cap);

#endif
