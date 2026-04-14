#include "world.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static void build_graphs(World* w) {
    w->road_count = 0;
    w->sidewalk_count = 0;
    for (int i = 0; i < WORLD_W * WORLD_H; i++) w->road_node[i] = -1;
    for (int y = 0; y < w->h; y++) {
        for (int x = 0; x < w->w; x++) {
            TileType t = w->tiles[y * w->w + x];
            if (t == TILE_ROAD) {
                w->road_node[y * w->w + x] = w->road_count;
                w->road_xs[w->road_count] = x;
                w->road_ys[w->road_count] = y;
                w->road_count++;
            } else if (t == TILE_SIDEWALK) {
                w->sidewalk_xs[w->sidewalk_count] = x;
                w->sidewalk_ys[w->sidewalk_count] = y;
                w->sidewalk_count++;
            }
        }
    }
}

void world_init(World* w) {
    memset(w, 0, sizeof(*w));
    w->w = WORLD_W;
    w->h = WORLD_H;
    for (int i = 0; i < WORLD_W * WORLD_H; i++) w->tiles[i] = TILE_GRASS;
}

static TileType char_to_tile(char c) {
    switch (c) {
        case '.': return TILE_GRASS;
        case '#': return TILE_BUILDING;
        case 'R':
        case '=':
        case '|':
        case '+': return TILE_ROAD;
        case 's':
        case '-': return TILE_SIDEWALK;
        case ' ': return TILE_EMPTY;
        default:  return TILE_GRASS;
    }
}

bool world_load_ascii(World* w, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return false;
    world_init(w);
    char line[WORLD_W + 4];
    int y = 0;
    while (y < WORLD_H && fgets(line, sizeof(line), f)) {
        int len = (int)strlen(line);
        for (int x = 0; x < WORLD_W && x < len; x++) {
            char c = line[x];
            if (c == '\n' || c == '\r') break;
            w->tiles[y * w->w + x] = char_to_tile(c);
        }
        y++;
    }
    fclose(f);
    build_graphs(w);
    return true;
}

void world_generate_default(World* w) {
    world_init(w);
    /* Fill with grass, then draw a grid of roads with sidewalks and buildings */
    for (int y = 0; y < w->h; y++) {
        for (int x = 0; x < w->w; x++) {
            w->tiles[y * w->w + x] = TILE_GRASS;
        }
    }
    /* Roads every 24 tiles (4-wide), sidewalks 2-wide, building interior 16x16 */
    for (int y = 0; y < w->h; y++) {
        for (int x = 0; x < w->w; x++) {
            int mx = x % 24;
            int my = y % 24;
            bool road_h = (my >= 10 && my <= 13);
            bool road_v = (mx >= 10 && mx <= 13);
            bool sw_h   = (my == 8 || my == 9 || my == 14 || my == 15);
            bool sw_v   = (mx == 8 || mx == 9 || mx == 14 || mx == 15);
            if (road_h || road_v) w->tiles[y * w->w + x] = TILE_ROAD;
            else if (sw_h || sw_v) w->tiles[y * w->w + x] = TILE_SIDEWALK;
            else {
                /* Each block has 4 quadrants of buildings with a small inner courtyard */
                int bx = mx, by = my;
                bool in_block = (bx <= 7 || bx >= 16) && (by <= 7 || by >= 16);
                bool edge = (bx == 0 || bx == 7 || bx == 16 || bx == 23 ||
                             by == 0 || by == 7 || by == 16 || by == 23);
                if (in_block && !edge) w->tiles[y * w->w + x] = TILE_BUILDING;
                else w->tiles[y * w->w + x] = TILE_GRASS;
            }
        }
    }
    /* Border buildings to keep world bounded */
    for (int x = 0; x < w->w; x++) {
        w->tiles[0 * w->w + x] = TILE_BUILDING;
        w->tiles[(w->h - 1) * w->w + x] = TILE_BUILDING;
    }
    for (int y = 0; y < w->h; y++) {
        w->tiles[y * w->w + 0] = TILE_BUILDING;
        w->tiles[y * w->w + (w->w - 1)] = TILE_BUILDING;
    }
    build_graphs(w);
}

TileType world_tile(const World* w, int x, int y) {
    if (!world_in_bounds(w, x, y)) return TILE_BUILDING;
    return w->tiles[y * w->w + x];
}

bool world_in_bounds(const World* w, int x, int y) {
    return x >= 0 && y >= 0 && x < w->w && y < w->h;
}

bool world_is_walkable(const World* w, int x, int y) {
    TileType t = world_tile(w, x, y);
    return t != TILE_BUILDING && t != TILE_EMPTY;
}

bool world_is_drivable(const World* w, int x, int y) {
    TileType t = world_tile(w, x, y);
    return t != TILE_BUILDING && t != TILE_EMPTY;
}

bool world_is_blocking(const World* w, int x, int y) {
    TileType t = world_tile(w, x, y);
    return t == TILE_BUILDING || t == TILE_EMPTY;
}

void world_spatial_clear(World* w) {
    for (int i = 0; i < SPATIAL_GRID_W * SPATIAL_GRID_H; i++) {
        w->grid[i].count = 0;
    }
}

void world_spatial_insert(World* w, int32_t id, float x, float y) {
    int gx = (int)(x / SPATIAL_CELL_SIZE);
    int gy = (int)(y / SPATIAL_CELL_SIZE);
    if (gx < 0 || gy < 0 || gx >= SPATIAL_GRID_W || gy >= SPATIAL_GRID_H) return;
    SpatialBucket* b = &w->grid[gy * SPATIAL_GRID_W + gx];
    if (b->count < SPATIAL_BUCKET_CAP) {
        b->ids[b->count++] = id;
    }
}

int world_spatial_query(const World* w, float cx, float cy, float radius,
                        int32_t* out, int cap) {
    int count = 0;
    int r_cells = (int)(radius / SPATIAL_CELL_SIZE) + 1;
    int gcx = (int)(cx / SPATIAL_CELL_SIZE);
    int gcy = (int)(cy / SPATIAL_CELL_SIZE);
    for (int dy = -r_cells; dy <= r_cells; dy++) {
        for (int dx = -r_cells; dx <= r_cells; dx++) {
            int gx = gcx + dx, gy = gcy + dy;
            if (gx < 0 || gy < 0 || gx >= SPATIAL_GRID_W || gy >= SPATIAL_GRID_H) continue;
            const SpatialBucket* b = &w->grid[gy * SPATIAL_GRID_W + gx];
            for (int i = 0; i < b->count; i++) {
                if (count < cap) out[count++] = b->ids[i];
            }
        }
    }
    return count;
}
