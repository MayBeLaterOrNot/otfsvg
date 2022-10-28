#include "otfsvg.h"

#include <stdarg.h>
#include <stdio.h>

typedef struct {
    FILE* output;
    int indent;
} render_context_t;

static void incIndent(render_context_t* context)
{
    context->indent += 4;
}

static void decIndent(render_context_t* context)
{
    context->indent -= 4;
}

static void writeChar(render_context_t* context, int ch)
{
    fputc(ch, context->output);
}

static void whiteSpace(render_context_t* context)
{
    writeChar(context, ' ');
}

static void newLine(render_context_t* context)
{
    writeChar(context, '\n');
}

static void openBracket(render_context_t* context)
{
    writeChar(context, '{');
}

static void closeBracket(render_context_t* context)
{
    writeChar(context, '}');
}

static void writeIndent(render_context_t* context)
{
    for(int i = 0; i < context->indent; ++i)
        whiteSpace(context);
}

static void writeString(render_context_t* context, const char* data)
{
    fputs(data, context->output);
}

static void writeF(render_context_t* context, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(context->output, fmt, args);
    va_end(args);
}

static void openBranch(render_context_t* context, const char* name)
{
    writeIndent(context);
    writeString(context, name);
    whiteSpace(context);
    openBracket(context);
    newLine(context);
    incIndent(context);
}

static void closeBranch(render_context_t* context)
{
    decIndent(context);
    writeIndent(context);
    closeBracket(context);
    newLine(context);
}

static void writePath(render_context_t* context, const otfsvg_path_t* path)
{
    writeIndent(context);
    writeString(context, "path : ");
    const otfsvg_path_command_t* elements = path->commands.data;
    const otfsvg_point_t* points = path->points.data;
    for(int i = 0; i < path->commands.size; ++i) {
        switch(elements[i]) {
        case otfsvg_path_command_move_to:
            writeF(context, "M%f %f", points[0].x, points[0].y);
            points += 1;
            break;
        case otfsvg_path_command_line_to:
            writeF(context, "L%f %f", points[0].x, points[0].y);
            points += 1;
            break;
        case otfsvg_path_command_cubic_to:
            writeF(context, "C%f %f %f %f %f %f", points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y);
            points += 3;
            break;
        case otfsvg_path_command_close:
            writeChar(context, 'Z');
            break;
        }
    }

    newLine(context);
}

static void writeTransform(render_context_t* context, const otfsvg_matrix_t* matrix)
{
    writeIndent(context);
    writeF(context, "transform : matrix(%f %f %f %f %f %f)", matrix->m00, matrix->m10, matrix->m01, matrix->m11, matrix->m02, matrix->m12);
    newLine(context);
}

static void writeColor(render_context_t* context, otfsvg_color_t color)
{
    int r = otfsvg_red_channel(color);
    int g = otfsvg_green_channel(color);
    int b = otfsvg_blue_channel(color);
    int a = otfsvg_alpha_channel(color);
    writeIndent(context);
    writeF(context, "color : rgba(%d %d %d %d)", r, g, b, a);
    newLine(context);
}

static void writePaint(render_context_t* context, const otfsvg_paint_t* paint)
{
    if(paint->type == otfsvg_paint_type_color) {
        writeColor(context, paint->color);
        return;
    }

    const otfsvg_gradient_t* gradient = &paint->gradient;
    if(gradient->type == otfsvg_gradient_type_linear) {
        openBranch(context, "linear-gradient");
        writeIndent(context);
        writeF(context, "points : %f %f %f %f", gradient->x1, gradient->y1, gradient->x2, gradient->y2);
    } else {
        openBranch(context, "radial-gradient");
        writeIndent(context);
        writeF(context, "points : %f %f %f %f %f", gradient->cx, gradient->cy, gradient->r, gradient->fx, gradient->fy);
    }

    newLine(context);
    writeTransform(context, &gradient->matrix);
    writeIndent(context);
    writeString(context, "gradient-spread : ");
    switch(gradient->spread) {
    case otfsvg_gradient_spread_pad:
        writeString(context, "pad");
        break;
    case otfsvg_gradient_spread_reflect:
        writeString(context, "reflect");
        break;
    case otfsvg_gradient_spread_repeat:
        writeString(context, "repeat");
        break;
    }

    newLine(context);
    const otfsvg_gradient_stop_t* stops = gradient->stops.data;
    for(int i = 0; i < gradient->stops.size; ++i) {
        openBranch(context, "stop");
        writeIndent(context);
        writeF(context, "offset : %f", stops[i].offset);
        newLine(context);
        writeColor(context, stops[i].color);
        closeBranch(context);
    }

    closeBranch(context);
}

static bool writeFill(void* userdata, const otfsvg_path_t* path, const otfsvg_matrix_t* matrix, otfsvg_fill_rule_t winding, const otfsvg_paint_t* paint)
{
    render_context_t* context = userdata;
    openBranch(context, "fill");
    writePath(context, path);
    writeTransform(context, matrix);
    writeIndent(context);
    writeString(context, "fill-rule : ");
    if(winding == otfsvg_fill_rule_non_zero)
        writeString(context, "non-zero");
    else
        writeString(context, "even-odd");
    newLine(context);
    writePaint(context, paint);
    closeBranch(context);
    return true;
}

static bool writeStroke(void* userdata, const otfsvg_path_t* path, const otfsvg_matrix_t* matrix, const otfsvg_stroke_data_t* strokedata, const otfsvg_paint_t* paint)
{
    render_context_t* context = userdata;
    openBranch(context, "stroke");
    writePath(context, path);
    writeTransform(context, matrix);
    writeIndent(context);
    writeF(context, "line-width : %f", strokedata->linewidth);
    newLine(context);
    writeIndent(context);
    writeString(context, "line-cap : ");
    switch(strokedata->linecap) {
    case otfsvg_line_cap_butt:
        writeString(context, "butt");
        break;
    case otfsvg_line_cap_round:
        writeString(context, "round");
        break;
    case otfsvg_line_cap_square:
        writeString(context, "square");
        break;
    }

    newLine(context);
    writeIndent(context);
    writeString(context, "line-join : ");
    switch(strokedata->linejoin) {
    case otfsvg_line_join_miter:
        writeString(context, "miter");
        break;
    case otfsvg_line_cap_round:
        writeString(context, "round");
        break;
    case otfsvg_line_join_bevel:
        writeString(context, "bevel");
        break;
    }

    newLine(context);
    writeIndent(context);
    writeF(context, "miter-limit : %f", strokedata->miterlimit);
    if(strokedata->dasharray.size > 0) {
        newLine(context);
        writeIndent(context);
        writeF(context, "dash-offset : %f", strokedata->dashoffset);
        newLine(context);
        writeString(context, "dash-array : ");
        for(int i = 0; i < strokedata->dasharray.size; ++i) {
            writeF(context, "%f ", strokedata->dasharray.data[i]);
        }
    }

    newLine(context);
    writePaint(context, paint);
    closeBranch(context);
    return true;
}

static bool pushGroup(void* userdata, float opacity, otfsvg_blend_mode_t mode)
{
    render_context_t* context = userdata;
    openBranch(context, "group");
    writeIndent(context);
    writeF(context, "opacity : %f", opacity);
    newLine(context);
    writeIndent(context);
    writeString(context, "blend-mode : ");
    if(mode == otfsvg_blend_mode_dst_in)
        writeString(context, "dst-in");
    else
        writeString(context, "src-over");

    newLine(context);
    return true;
}

static bool popGroup(void* userdata, float opacity, otfsvg_blend_mode_t mode)
{
    render_context_t* context = userdata;
    closeBranch(context);
    return true;
}

int main(int argc, char* argv[])
{
    if(argc != 3 && argc != 4) {
        printf("Usage : otfsvg-dump input output [id]\n");
        return -1;
    }

    FILE* input = fopen(argv[1], "r");
    if(input == NULL) {
        printf("Unable to open input file (%s)\n", argv[1]);
        return -1;
    }

    FILE* output = fopen(argv[2], "w");
    if(output == NULL) {
        printf("Unable to open output file (%s)\n", argv[2]);
        fclose(input);
        return -1;
    }

    fseek(input, 0, SEEK_END);
    size_t length = ftell(input);
    char* data = malloc(length);

    fseek(input, 0, SEEK_SET);
    fread(data, length, 1, input);
    fclose(input);

    otfsvg_document_t* document = otfsvg_document_create();
    if(!otfsvg_document_load(document, data, length, 150, 300, 96)) {
        printf("Unable to load (%s)\n", argv[1]);
        goto cleanup;
    }

    const char* id = NULL;
    if(argc == 4)
        id = argv[3];

    otfsvg_rect_t rect = {0, 0, 0, 0};
    if(!otfsvg_document_rect(document, &rect, id)) {
        printf("Unable to locate (%s)\n", id);
        goto cleanup;
    }

    render_context_t context = {output, 0};
    if(id == NULL) {
        openBranch(&context, "document");
    } else {
        openBranch(&context, "element");
        writeIndent(&context);
        writeF(&context, "id : %s", id);
        newLine(&context);
    }

    writeIndent(&context);
    writeF(&context, "rect : %f %f %f %f", rect.x, rect.y, rect.w, rect.h);
    newLine(&context);

    otfsvg_canvas_t canvas = {writeFill, writeStroke, pushGroup, popGroup, NULL, NULL};
    otfsvg_document_render(document, &canvas, &context, NULL, NULL, otfsvg_black_color, id);

    closeBranch(&context);

cleanup:
    otfsvg_document_destory(document);
    fclose(output);
    free(data);
    return 0;
}
