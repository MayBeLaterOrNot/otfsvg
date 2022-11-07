/*
 * Copyright (c) 2022 Samuel Ugochukwu <sammycage8051@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef OTFSVG_H
#define OTFSVG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float x;
    float y;
} otfsvg_point_t;

typedef struct {
    float x;
    float y;
    float w;
    float h;
} otfsvg_rect_t;

typedef enum {
    otfsvg_path_command_move_to,
    otfsvg_path_command_line_to,
    otfsvg_path_command_cubic_to,
    otfsvg_path_command_close
} otfsvg_path_command_t;

/**
 * Path iteration example
 *
 * const otfsvg_path_command_t* commands = path->commands.data;
 * const otfsvg_point_t* points = path->points.data;
 * for(int i = 0; i < path->commands.size; ++i) {
 *     switch(commands[i]) {
 *     case otfsvg_path_command_move_to:
 *         printf("M%f %f", points[0].x, points[0].y);
 *         points += 1;
 *         break;
 *     case otfsvg_path_command_line_to:
 *         printf("L%f %f", points[0].x, points[0].y);
 *         points += 1;
 *         break;
 *     case otfsvg_path_command_cubic_to:
 *         printf("C%f %f %f %f %f %f", points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y);
 *         points += 3;
 *         break;
 *     case otfsvg_path_command_close:
 *         putchar('Z');
 *         break;
 *     }
 * }
 *
 **/
typedef struct {
    struct {
        otfsvg_path_command_t* data;
        int size;
        int capacity;
    } commands;
    struct {
        otfsvg_point_t* data;
        int size;
        int capacity;
    } points;
} otfsvg_path_t;

/**
 * otfsvg_matrix_t defines an affine transformation matrix
 * @m00 - horizontal scaling
 * @m10 - vertical skewing
 * @m01 - horizontal skewing
 * @m11 - vertical scaling
 * @m02 - horizontal translation
 * @m12 - vertical translation
 **/
typedef struct {
    float m00; float m10;
    float m01; float m11;
    float m02; float m12;
} otfsvg_matrix_t;

/**
 * otfsvg_color_t defines a 32-bit RGBA color (8-bit per component) stored as 0xAARRGGBB
 **/
typedef unsigned int otfsvg_color_t;

#define otfsvg_blue_channel(c) (((c) >> 0) & 0xFF)
#define otfsvg_green_channel(c) (((c) >> 8) & 0xFF)
#define otfsvg_red_channel(c) (((c) >> 16) & 0xFF)
#define otfsvg_alpha_channel(c) (((c) >> 24) & 0xFF)

#define otfsvg_black_color 0xFF000000
#define otfsvg_white_color 0xFFFFFFFF
#define otfsvg_transparent_color 0x00000000

typedef enum {
    otfsvg_gradient_type_linear,
    otfsvg_gradient_type_radial
} otfsvg_gradient_type_t;

typedef enum {
    otfsvg_gradient_spread_pad,
    otfsvg_gradient_spread_reflect,
    otfsvg_gradient_spread_repeat
} otfsvg_gradient_spread_t;

typedef struct {
    float offset;
    otfsvg_color_t color;
} otfsvg_gradient_stop_t;

typedef struct {
    otfsvg_gradient_type_t type;
    otfsvg_gradient_spread_t spread;
    otfsvg_matrix_t matrix;
    float x1, y1, x2, y2;
    float cx, cy, r, fx, fy;
    struct {
        otfsvg_gradient_stop_t* data;
        int size;
        int capacity;
    } stops;
} otfsvg_gradient_t;

typedef enum {
    otfsvg_paint_type_color,
    otfsvg_paint_type_gradient
} otfsvg_paint_type_t;

typedef struct {
    otfsvg_paint_type_t type;
    otfsvg_color_t color;
    otfsvg_gradient_t gradient;
} otfsvg_paint_t;

typedef enum {
    otfsvg_blend_mode_src_over,
    otfsvg_blend_mode_dst_in
} otfsvg_blend_mode_t;

typedef enum {
    otfsvg_line_cap_butt,
    otfsvg_line_cap_round,
    otfsvg_line_cap_square
} otfsvg_line_cap_t;

typedef enum {
    otfsvg_line_join_miter,
    otfsvg_line_join_round,
    otfsvg_line_join_bevel
} otfsvg_line_join_t;

typedef enum {
    otfsvg_fill_rule_non_zero,
    otfsvg_fill_rule_even_odd
} otfsvg_fill_rule_t;

typedef struct {
    otfsvg_line_cap_t linecap;
    otfsvg_line_join_t linejoin;
    float linewidth;
    float miterlimit;
    float dashoffset;
    struct {
        float* data;
        int size;
        int capacity;
    } dasharray;
} otfsvg_stroke_data_t;

typedef struct {
    void* userdata;
    int width;
    int height;
} otfsvg_image_t;

typedef bool(*otfsvg_palette_func_t)(void* userdata, const char* name, size_t length, otfsvg_color_t* color);
typedef bool(*otfsvg_fill_path_func_t)(void* userdata, const otfsvg_path_t* path, const otfsvg_matrix_t* matrix, otfsvg_fill_rule_t winding, const otfsvg_paint_t* paint);
typedef bool(*otfsvg_stroke_path_func_t)(void* userdata, const otfsvg_path_t* path, const otfsvg_matrix_t* matrix, const otfsvg_stroke_data_t* strokedata, const otfsvg_paint_t* paint);
typedef bool(*otfsvg_push_group_func_t)(void* userdata, float opacity, otfsvg_blend_mode_t mode);
typedef bool(*otfsvg_pop_group_func_t)(void* userdata, float opacity, otfsvg_blend_mode_t mode);
typedef bool(*otfsvg_decode_image_func_t)(void* userdata, const char* data, size_t length, otfsvg_image_t* image);
typedef bool(*otfsvg_draw_image_func_t)(void* userdata, const otfsvg_image_t* image, const otfsvg_matrix_t* matrix, const otfsvg_rect_t* clip, float opacity);

typedef struct {
    otfsvg_fill_path_func_t fill_path;
    otfsvg_stroke_path_func_t stroke_path;
    otfsvg_push_group_func_t push_group;
    otfsvg_pop_group_func_t pop_group;
    otfsvg_decode_image_func_t decode_image;
    otfsvg_draw_image_func_t draw_image;
} otfsvg_canvas_t;

typedef struct otfsvg_document otfsvg_document_t;

otfsvg_document_t* otfsvg_document_create(void);
void otfsvg_document_clear(otfsvg_document_t* document);
void otfsvg_document_destory(otfsvg_document_t* document);

bool otfsvg_document_load(otfsvg_document_t* document, const char* data, size_t length, float width, float height, float dpi);
float otfsvg_document_width(const otfsvg_document_t* document);
float otfsvg_document_height(const otfsvg_document_t* document);
void otfsvg_document_set_matrix(otfsvg_document_t* document, const otfsvg_matrix_t* matrix);
void otfsvg_document_get_matrix(const otfsvg_document_t* document, otfsvg_matrix_t* matrix);
bool otfsvg_document_rect(otfsvg_document_t* document, otfsvg_rect_t* rect, const char* id);
bool otfsvg_document_render(otfsvg_document_t* document, otfsvg_canvas_t* canvas, void* canvas_data, otfsvg_palette_func_t palette_func, void* palette_data, otfsvg_color_t current_color, const char* id);

#ifdef __cplusplus
}
#endif

#endif // OTFSVG_H
