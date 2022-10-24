#ifndef OTFSVG_PRIVATE_H
#define OTFSVG_PRIVATE_H

#include "otfsvg.h"

void otfsvg_rect_init(otfsvg_rect_t* rect, float x, float y, float w, float h);
void otfsvg_rect_unite(otfsvg_rect_t* rect, const otfsvg_rect_t* source);
void otfsvg_rect_intersect(otfsvg_rect_t* rect, const otfsvg_rect_t* source);

void otfsvg_matrix_init(otfsvg_matrix_t* matrix, float m00, float m10, float m01, float m11, float m02, float m12);
void otfsvg_matrix_init_identity(otfsvg_matrix_t* matrix);
void otfsvg_matrix_init_translate(otfsvg_matrix_t* matrix, float x, float y);
void otfsvg_matrix_init_scale(otfsvg_matrix_t* matrix, float x, float y);
void otfsvg_matrix_init_shear(otfsvg_matrix_t* matrix, float x, float y);
void otfsvg_matrix_init_rotate(otfsvg_matrix_t* matrix, float angle, float x, float y);
void otfsvg_matrix_translate(otfsvg_matrix_t* matrix, float x, float y);
void otfsvg_matrix_scale(otfsvg_matrix_t* matrix, float x, float y);
void otfsvg_matrix_shear(otfsvg_matrix_t* matrix, float x, float y);
void otfsvg_matrix_rotate(otfsvg_matrix_t* matrix, float angle, float x, float y);
void otfsvg_matrix_multiply(otfsvg_matrix_t* matrix, const otfsvg_matrix_t* a, const otfsvg_matrix_t* b);
bool otfsvg_matrix_invert(otfsvg_matrix_t* matrix);
void otfsvg_matrix_map(const otfsvg_matrix_t* matrix, float x, float y, float* _x, float* _y);
void otfsvg_matrix_map_point(const otfsvg_matrix_t* matrix, const otfsvg_point_t* src, otfsvg_point_t* dst);
void otfsvg_matrix_map_rect(const otfsvg_matrix_t* matrix, const otfsvg_rect_t* src, otfsvg_rect_t* dst);

void otfsvg_path_init(otfsvg_path_t* path);
void otfsvg_path_destroy(otfsvg_path_t* path);
void otfsvg_path_move_to(otfsvg_path_t* path, float x, float y);
void otfsvg_path_line_to(otfsvg_path_t* path, float x, float y);
void otfsvg_path_quad_to(otfsvg_path_t* path, float x1, float y1, float x2, float y2, float x3, float y3);
void otfsvg_path_cubic_to(otfsvg_path_t* path, float x1, float y1, float x2, float y2, float x3, float y3);
void otfsvg_path_arc_to(otfsvg_path_t* path, float cx, float cy, float rx, float ry, float angle, bool large_arc_flag, bool sweep_flag, float x, float y);
void otfsvg_path_close(otfsvg_path_t* path);
void otfsvg_path_add_rect(otfsvg_path_t* path, float x, float y, float w, float h);
void otfsvg_path_add_round_rect(otfsvg_path_t* path, float x, float y, float w, float h, float rx, float ry);
void otfsvg_path_add_ellipse(otfsvg_path_t* path, float cx, float cy, float rx, float ry);
void otfsvg_path_bounding_box(otfsvg_path_t* path, otfsvg_rect_t *bbox);
void otfsvg_path_clear(otfsvg_path_t* path);

#define otfsvg_sqrt2 1.41421356237309504880f
#define otfsvg_pi 3.14159265358979323846f
#define otfsvg_kappa 0.55228474983079339840f

#define otfsvg_min(a, b) ((a) < (b) ? (a) : (b))
#define otfsvg_max(a, b) ((a) > (b) ? (a) : (b))
#define otfsvg_clamp(v, lo, hi) ((v) < (lo) ? (lo) : (hi) < (v) ? (hi) : (v))

#define otfsvg_deg2rad(a) ((a) * otfsvg_pi / 180.f)
#define otfsvg_rad2deg(a) ((a) * 180.f / otfsvg_pi)

#define otfsvg_array_clear(array) (array.size = 0)
#define otfsvg_array_destroy(array) free(array.data)

#define otfsvg_array_init(array) \
    do { \
        array.data = NULL; \
        array.size = 0; \
        array.capacity = 0; \
    } while(0)

#define otfsvg_array_ensure(array, count) \
    do { \
    if(array.size + count > array.capacity) { \
        int capacity = array.size + count; \
        int newcapacity = array.capacity == 0 ? 8 : array.capacity; \
        while(newcapacity < capacity) { newcapacity *= 2; } \
        array.data = realloc(array.data, (size_t)(newcapacity) * sizeof(array.data[0])); \
        array.capacity = newcapacity; \
    } \
    } while(0)

#endif // OTFSVG_PRIVATE_H
