#include "otfsvg-private.h"

#include <math.h>

void otfsvg_rect_init(otfsvg_rect_t* rect, float x, float y, float w, float h)
{
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;
}

void otfsvg_rect_unite(otfsvg_rect_t* rect, const otfsvg_rect_t* source)
{
    float l = otfsvg_min(rect->x, source->x);
    float t = otfsvg_min(rect->y, source->y);
    float r = otfsvg_max(rect->x + rect->w, source->x + source->w);
    float b = otfsvg_max(rect->y + rect->h, source->y + source->h);

    otfsvg_rect_init(rect, l, t, r - l, b - t);
}

void otfsvg_rect_intersect(otfsvg_rect_t* rect, const otfsvg_rect_t* source)
{
    float l = otfsvg_max(rect->x, source->x);
    float t = otfsvg_max(rect->y, source->y);
    float r = otfsvg_min(rect->x + rect->w, source->x + source->w);
    float b = otfsvg_min(rect->y + rect->h, source->y + source->h);

    otfsvg_rect_init(rect, l, t, r - l, b - t);
}

void otfsvg_matrix_init(otfsvg_matrix_t* matrix, float m00, float m10, float m01, float m11, float m02, float m12)
{
    matrix->m00 = m00; matrix->m10 = m10;
    matrix->m01 = m01; matrix->m11 = m11;
    matrix->m02 = m02; matrix->m12 = m12;
}

void otfsvg_matrix_init_identity(otfsvg_matrix_t* matrix)
{
    matrix->m00 = 1.0; matrix->m10 = 0.0;
    matrix->m01 = 0.0; matrix->m11 = 1.0;
    matrix->m02 = 0.0; matrix->m12 = 0.0;
}

void otfsvg_matrix_init_translate(otfsvg_matrix_t* matrix, float x, float y)
{
    otfsvg_matrix_init(matrix, 1.0, 0.0, 0.0, 1.0, x, y);
}

void otfsvg_matrix_init_scale(otfsvg_matrix_t* matrix, float x, float y)
{
    otfsvg_matrix_init(matrix, x, 0.0, 0.0, y, 0.0, 0.0);
}

void otfsvg_matrix_init_shear(otfsvg_matrix_t* matrix, float x, float y)
{
    float ytan = tanf(otfsvg_deg2rad(y));
    float xtan = tanf(otfsvg_deg2rad(x));
    otfsvg_matrix_init(matrix, 1.0, ytan, xtan, 1.0, 0.0, 0.0);
}

void otfsvg_matrix_init_rotate(otfsvg_matrix_t* matrix, float angle, float x, float y)
{
    float c = cosf(otfsvg_deg2rad(angle));
    float s = sinf(otfsvg_deg2rad(angle));
    if(x == 0 && y == 0) {
        otfsvg_matrix_init(matrix, c, s, -s, c, 0, 0);
        return;
    }

    float cx = x * (1 - c) + y * s;
    float cy = y * (1 - c) - x * s;
    otfsvg_matrix_init(matrix, c, s, -s, c, cx, cy);
}

void otfsvg_matrix_translate(otfsvg_matrix_t* matrix, float x, float y)
{
    otfsvg_matrix_t m;
    otfsvg_matrix_init_translate(&m, x, y);
    otfsvg_matrix_multiply(matrix, &m, matrix);
}

void otfsvg_matrix_scale(otfsvg_matrix_t* matrix, float x, float y)
{
    otfsvg_matrix_t m;
    otfsvg_matrix_init_scale(&m, x, y);
    otfsvg_matrix_multiply(matrix, &m, matrix);
}

void otfsvg_matrix_shear(otfsvg_matrix_t* matrix, float x, float y)
{
    otfsvg_matrix_t m;
    otfsvg_matrix_init_shear(&m, x, y);
    otfsvg_matrix_multiply(matrix, &m, matrix);
}

void otfsvg_matrix_rotate(otfsvg_matrix_t* matrix, float angle, float x, float y)
{
    otfsvg_matrix_t m;
    otfsvg_matrix_init_rotate(&m, angle, x, y);
    otfsvg_matrix_multiply(matrix, &m, matrix);
}

void otfsvg_matrix_multiply(otfsvg_matrix_t* matrix, const otfsvg_matrix_t* a, const otfsvg_matrix_t* b)
{
    float m00 = a->m00 * b->m00 + a->m10 * b->m01;
    float m10 = a->m00 * b->m10 + a->m10 * b->m11;
    float m01 = a->m01 * b->m00 + a->m11 * b->m01;
    float m11 = a->m01 * b->m10 + a->m11 * b->m11;
    float m02 = a->m02 * b->m00 + a->m12 * b->m01 + b->m02;
    float m12 = a->m02 * b->m10 + a->m12 * b->m11 + b->m12;
    otfsvg_matrix_init(matrix, m00, m10, m01, m11, m02, m12);
}

bool otfsvg_matrix_invert(otfsvg_matrix_t* matrix)
{
    float det = (matrix->m00 * matrix->m11 - matrix->m10 * matrix->m01);
    if(det == 0.0)
        return false;

    float inv_det = 1.0 / det;
    float m00 = matrix->m00 * inv_det;
    float m10 = matrix->m10 * inv_det;
    float m01 = matrix->m01 * inv_det;
    float m11 = matrix->m11 * inv_det;
    float m02 = (matrix->m01 * matrix->m12 - matrix->m11 * matrix->m02) * inv_det;
    float m12 = (matrix->m10 * matrix->m02 - matrix->m00 * matrix->m12) * inv_det;
    otfsvg_matrix_init(matrix, m11, -m10, -m01, m00, m02, m12);
    return true;
}

void otfsvg_matrix_map(const otfsvg_matrix_t* matrix, float x, float y, float* _x, float* _y)
{
    *_x = x * matrix->m00 + y * matrix->m01 + matrix->m02;
    *_y = x * matrix->m10 + y * matrix->m11 + matrix->m12;
}

void otfsvg_matrix_map_point(const otfsvg_matrix_t* matrix, const otfsvg_point_t* src, otfsvg_point_t* dst)
{
    otfsvg_matrix_map(matrix, src->x, src->y, &dst->x, &dst->y);
}

void otfsvg_matrix_map_rect(const otfsvg_matrix_t* matrix, const otfsvg_rect_t* src, otfsvg_rect_t* dst)
{
    otfsvg_point_t p[4];
    p[0].x = src->x;
    p[0].y = src->y;
    p[1].x = src->x + src->w;
    p[1].y = src->y;
    p[2].x = src->x + src->w;
    p[2].y = src->y + src->h;
    p[3].x = src->x;
    p[3].y = src->y + src->h;

    otfsvg_matrix_map_point(matrix, &p[0], &p[0]);
    otfsvg_matrix_map_point(matrix, &p[1], &p[1]);
    otfsvg_matrix_map_point(matrix, &p[2], &p[2]);
    otfsvg_matrix_map_point(matrix, &p[3], &p[3]);

    float l = p[0].x;
    float t = p[0].y;
    float r = p[0].x;
    float b = p[0].y;
    for(int i = 1; i < 4; i++) {
        if(p[i].x < l) l = p[i].x;
        if(p[i].x > r) r = p[i].x;
        if(p[i].y < t) t = p[i].y;
        if(p[i].y > b) b = p[i].y;
    }

    dst->x = l;
    dst->y = t;
    dst->w = r - l;
    dst->h = b - t;
}

void otfsvg_path_init(otfsvg_path_t* path) {
    otfsvg_array_init(path->commands);
    otfsvg_array_init(path->points);
}

void otfsvg_path_destroy(otfsvg_path_t* path) {
    otfsvg_array_destroy(path->commands);
    otfsvg_array_destroy(path->points);
}

void otfsvg_path_move_to(otfsvg_path_t* path, float x, float y)
{
    otfsvg_array_ensure(path->commands, 1);
    otfsvg_array_ensure(path->points, 1);

    path->commands.data[path->commands.size] = otfsvg_path_command_move_to;
    path->commands.size += 1;

    path->points.data[path->points.size].x = x;
    path->points.data[path->points.size].y = y;
    path->points.size += 1;
}

void otfsvg_path_line_to(otfsvg_path_t* path, float x, float y)
{
    otfsvg_array_ensure(path->commands, 1);
    otfsvg_array_ensure(path->points, 1);

    path->commands.data[path->commands.size] = otfsvg_path_command_line_to;
    path->commands.size += 1;

    path->points.data[path->points.size].x = x;
    path->points.data[path->points.size].y = y;
    path->points.size += 1;
}

void otfsvg_path_quad_to(otfsvg_path_t* path, float x1, float y1, float x2, float y2, float x3, float y3)
{
    float cx1 = 2.f / 3.f * x2 + 1.f / 3.f * x1;
    float cy1 = 2.f / 3.f * y2 + 1.f / 3.f * y1;
    float cx2 = 2.f / 3.f * x2 + 1.f / 3.f * x3;
    float cy2 = 2.f / 3.f * y2 + 1.f / 3.f * y3;
    otfsvg_path_cubic_to(path, cx1, cy1, cx2, cy2, x1, y1);
}

void otfsvg_path_cubic_to(otfsvg_path_t* path, float x1, float y1, float x2, float y2, float x3, float y3)
{
    otfsvg_array_ensure(path->commands, 1);
    otfsvg_array_ensure(path->points, 3);

    path->commands.data[path->commands.size] = otfsvg_path_command_cubic_to;
    path->commands.size += 1;

    path->points.data[path->points.size].x = x1;
    path->points.data[path->points.size].y = y1;
    path->points.size += 1;

    path->points.data[path->points.size].x = x2;
    path->points.data[path->points.size].y = y2;
    path->points.size += 1;

    path->points.data[path->points.size].x = x3;
    path->points.data[path->points.size].y = y3;
    path->points.size += 1;
}

void otfsvg_path_close(otfsvg_path_t* path)
{
    if(path->commands.size == 0)
        return;

    if(path->commands.data[path->commands.size - 1] == otfsvg_path_command_close)
        return;

    otfsvg_array_ensure(path->commands, 1);
    path->commands.data[path->commands.size] = otfsvg_path_command_close;
    path->commands.size += 1;
}

void otfsvg_path_clear(otfsvg_path_t* path)
{
    otfsvg_array_clear(path->commands);
    otfsvg_array_clear(path->points);
}

void otfsvg_path_arc_to(otfsvg_path_t* path, float x1, float y1, float rx, float ry, float angle, bool large_arc_flag, bool sweep_flag, float x2, float y2)
{
    if(rx < 0) rx = -rx;
    if(ry < 0) ry = -ry;

    float dx = x1 - x2;
    float dy = y1 - y2;
    if(!rx || !ry || !dx || !dy)
        return;

    dx *= 0.5f;
    dy *= 0.5f;

    otfsvg_matrix_t matrix;
    otfsvg_matrix_init_rotate(&matrix, -angle, 0, 0);
    otfsvg_matrix_map(&matrix, dx, dy, &dx, &dy);

    float rxrx = rx * rx;
    float ryry = ry * ry;
    float dxdx = dx * dx;
    float dydy = dy * dy;
    float radius = dxdx / rxrx + dydy / ryry;
    if(radius > 1) {
        rx *= sqrtf(radius);
        ry *= sqrtf(radius);
    }

    otfsvg_matrix_init_scale(&matrix, 1 / rx, 1 / ry);
    otfsvg_matrix_rotate(&matrix, -angle, 0, 0);
    otfsvg_matrix_map(&matrix, x1, y1, &x1, &y1);
    otfsvg_matrix_map(&matrix, x2, y2, &x2, &y2);

    float dx1 = x2 - x1;
    float dy1 = y2 - y1;
    float d = dx1 * dx1 + dy1 * dy1;
    float scale_sq = 1 / d - 0.25f;
    if(scale_sq < 0) scale_sq = 0;
    float scale = sqrtf(scale_sq);
    if(sweep_flag == large_arc_flag) scale = -scale;
    dx1 *= scale;
    dy1 *= scale;

    float cx1 = 0.5f * (x1 + x2) - dy1;
    float cy1 = 0.5f * (y1 + y2) + dx1;

    float th1 = atan2f(y1 - cy1, x1 - cx1);
    float th2 = atan2f(y2 - cy1, x2 - cx1);
    float th_arc = th2 - th1;
    if(th_arc < 0 && sweep_flag)
        th_arc += 2.0f * otfsvg_pi;
    else if(th_arc > 0 && !sweep_flag)
        th_arc -= 2.0f * otfsvg_pi;
    otfsvg_matrix_init_rotate(&matrix, angle, 0, 0);
    otfsvg_matrix_scale(&matrix, rx, ry);
    int segments = ceilf(fabsf(th_arc / (otfsvg_pi * 0.5f + 0.001f)));
    for(int i = 0; i < segments; i++) {
        float th_start = th1 + i * th_arc / segments;
        float th_end = th1 + (i + 1) * th_arc / segments;
        float t = (8.f / 6.f) * tanf(0.25f * (th_end - th_start));

        float x3 = cosf(th_end) + cx1;
        float y3 = sinf(th_end) + cy1;

        float x2 = x3 + t * sinf(th_end);
        float y2 = y3 - t * cosf(th_end);

        float x1 = cosf(th_start) - t * sinf(th_start);
        float y1 = sinf(th_start) + t * cosf(th_start);

        x1 += cx1;
        y1 += cy1;

        otfsvg_matrix_map(&matrix, x1, y1, &x1, &y1);
        otfsvg_matrix_map(&matrix, x2, y2, &x2, &y2);
        otfsvg_matrix_map(&matrix, x3, y3, &x3, &y3);
        otfsvg_path_cubic_to(path, x1, y1, x2, y2, x3, y3);
    }
}

void otfsvg_path_add_rect(otfsvg_path_t* path, float x, float y, float w, float h)
{
    otfsvg_path_move_to(path, x, y);
    otfsvg_path_line_to(path, x + w, y);
    otfsvg_path_line_to(path, x + w, y + h);
    otfsvg_path_line_to(path, x, y + h);
    otfsvg_path_line_to(path, x, y);
    otfsvg_path_close(path);
}

void otfsvg_path_add_round_rect(otfsvg_path_t* path, float x, float y, float w, float h, float rx, float ry)
{
    rx = otfsvg_min(rx, w * 0.5);
    ry = otfsvg_min(ry, h * 0.5);
    if(rx == 0.f && ry == 0.f) {
        otfsvg_path_add_rect(path, x, y, w, h);
        return;
    }

    float right = x + w;
    float bottom = y + h;

    float cpx = rx * otfsvg_kappa;
    float cpy = ry * otfsvg_kappa;

    otfsvg_path_move_to(path, x, y+ry);
    otfsvg_path_cubic_to(path, x, y+ry-cpy, x+rx-cpx, y, x+rx, y);
    otfsvg_path_line_to(path, right-rx, y);
    otfsvg_path_cubic_to(path, right-rx+cpx, y, right, y+ry-cpy, right, y+ry);
    otfsvg_path_line_to(path, right, bottom-ry);
    otfsvg_path_cubic_to(path, right, bottom-ry+cpy, right-rx+cpx, bottom, right-rx, bottom);
    otfsvg_path_line_to(path, x+rx, bottom);
    otfsvg_path_cubic_to(path, x+rx-cpx, bottom, x, bottom-ry+cpy, x, bottom-ry);
    otfsvg_path_line_to(path, x, y+ry);
    otfsvg_path_close(path);
}

void otfsvg_path_add_ellipse(otfsvg_path_t* path, float cx, float cy, float rx, float ry)
{
    float left = cx - rx;
    float top = cy - ry;
    float right = cx + rx;
    float bottom = cy + ry;

    float cpx = rx * otfsvg_kappa;
    float cpy = ry * otfsvg_kappa;

    otfsvg_path_move_to(path, cx, top);
    otfsvg_path_cubic_to(path, cx+cpx, top, right, cy-cpy, right, cy);
    otfsvg_path_cubic_to(path, right, cy+cpy, cx+cpx, bottom, cx, bottom);
    otfsvg_path_cubic_to(path, cx-cpx, bottom, left, cy+cpy, left, cy);
    otfsvg_path_cubic_to(path, left, cy-cpy, cx-cpx, top, cx, top);
    otfsvg_path_close(path);
}

void otfsvg_path_bounding_box(otfsvg_path_t* path, otfsvg_rect_t* bbox)
{
    const otfsvg_point_t* p = path->points.data;
    int count = path->points.size;
    if(count == 0) {
        bbox->x = 0;
        bbox->y = 0;
        bbox->w = 0;
        bbox->h = 0;
        return;
    }

    float l = p[0].x;
    float t = p[0].y;
    float r = p[0].x;
    float b = p[0].y;

    for(int i = 0; i < count; i++) {
        if(p[i].x < l) l = p[i].x;
        if(p[i].x > r) r = p[i].x;
        if(p[i].y < t) t = p[i].y;
        if(p[i].y > b) b = p[i].y;
    }

    bbox->x = l;
    bbox->y = t;
    bbox->w = r - l;
    bbox->h = b - t;
}
