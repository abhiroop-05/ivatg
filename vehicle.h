#ifndef VEHICLE_H
#define VEHICLE_H

#include "entity.h"
#include "world.h"

/* Apply accel/brake/steering inputs on a vehicle entity (type == AGENT_VEHICLE).
 * Inputs are expected to have been stored in e->throttle_input, steer_input, brake_input. */
void vehicle_tick(EntityPool* pool, World* w, float dt);

/* Attempt to have `ped` enter nearest vehicle. Returns vehicle id or -1. */
int32_t vehicle_try_enter(EntityPool* pool, Entity* ped, float radius);

/* Exit vehicle: place ped adjacent. */
void    vehicle_exit(EntityPool* pool, Entity* ped);

#endif
