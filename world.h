#ifndef GTA_WORLD_H
#define GTA_WORLD_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef enum {
    TILE_EMPTY = 0,
    TILE_ROAD,
    TILE_SIDEWALK,
    TILE_BUILDING,
    TILE_GRASS,
    TILE_COUNT
} TileType;

typedef struct {
    int32_t ids[SPATIAL_BUCKET_CAP];
    int     count;
} SpatialBucket;

typedef struct {
    int       w, h;
    TileType  tiles[WORLD_W * WORLD_H];
    /* Road-tile index graph for A*: road_node[y*W+x] = node index or -1 */
    int       road_node[WORLD_W * WORLD_H];
    int       road_count;
    int       road_xs[WORLD_W * WORLD_H];
    int       road_ys[WORLD_W * WORLD_H];
    /* Sidewalk waypoint list */
    int       sidewalk_count;
    int       sidewalk_xs[WORLD_W * WORLD_H];
    int       sidewalk_ys[WORLD_W * WORLD_H];

    SpatialBucket grid[SPATIAL_GRID_W * SPATIAL_GRID_H];
} World;

void world_init(World* w);
bool world_load_ascii(World* w, const char* path);
void world_generate_default(World* w);

TileType world_tile(const World* w, int x, int y);
bool     world_in_bounds(const World* w, int x, int y);
bool     world_is_walkable(const World* w, int x, int y);
bool     world_is_drivable(const World* w, int x, int y);
bool     world_is_blocking(const World* w, int x, int y);

/* Spatial hash */
void world_spatial_clear(World* w);
void world_spatial_insert(World* w, int32_t id, float x, float y);
/* Fill out[] with entity ids within radius of (cx,cy). Returns count. */
int  world_spatial_query(const World* w, float cx, float cy, float radius,
                         int32_t* out, int cap);

#endif
