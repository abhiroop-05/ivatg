#ifndef PHYSICS_H
#define PHYSICS_H

#include "entity.h"
#include "world.h"

void physics_tick(EntityPool* pool, World* w, float dt);

/* Utility */
float phys_dist2(float x1, float y1, float x2, float y2);
float phys_dist(float x1, float y1, float x2, float y2);

#endif
