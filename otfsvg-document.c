#include "otfsvg-private.h"

#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>

#define IS_ALPHA(c) (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
#define IS_NUM(c) (c >= '0' && c <= '9')
#define IS_WS(c) (c == ' ' || c == '\t' || c == '\n' || c == '\r')
#define IS_STARTNAMECHAR(c) (IS_ALPHA(c) ||  c == '_' || c == ':')
#define IS_NAMECHAR(c) (IS_STARTNAMECHAR(c) || IS_NUM(c) || c == '-' || c == '.')

static inline bool skip_string(const char** begin, const char* end, const char* data)
{
    const char* it = *begin;
    while(it < end && *data && *it == *data) {
        ++data;
        ++it;
    }

    if(*data == '\0') {
        *begin = it;
        return true;
    }

    return false;
}

static inline bool skip_delim(const char** begin, const char* end, const char delim)
{
    const char* it = *begin;
    if(it < end && *it == delim) {
        *begin = it + 1;
        return true;
    }

    return false;
}

static inline bool string_eq(const char* it, const char* end, const char* data)
{
    if(skip_string(&it, end, data))
        return it == end;
    return false;
}

static const char* string_find(const char* it, const char* end, const char* data)
{
    while(it < end) {
        const char* begin = it;
        if(skip_string(&it, end, data))
            return begin;
        ++it;
    }

    return NULL;
}

static inline bool skip_ws(const char** begin, const char* end)
{
    const char* it = *begin;
    while(it < end && IS_WS(*it))
        ++it;
    *begin = it;
    return it < end;
}

static inline const char* rtrim(const char* begin, const char* end)
{
    while(end > begin && IS_WS(end[-1]))
        --end;
    return end;
}

static inline const char* ltrim(const char* begin, const char* end)
{
    while(begin < end && IS_WS(*begin))
        ++begin;
    return begin;
}

static inline bool skip_ws_delim(const char** begin, const char* end, char delim)
{
    const char* it = *begin;
    if(it < end && !IS_WS(*it) && *it != delim)
        return false;
    if(skip_ws(&it, end)) {
        if(it < end && *it == delim) {
            ++it;
            skip_ws(&it, end);
        }
    }

    *begin = it;
    return it < end;
}

static inline bool skip_ws_comma(const char** begin, const char* end)
{
    return skip_ws_delim(begin, end, ',');
}

typedef struct {
    const char* name;
    int id;
} name_entry_t;

static int name_entry_compare(const void* a, const void* b)
{
    const char* name = a;
    const name_entry_t* entry = b;
    return strcmp(name, entry->name);
}

enum {
    ID_UNKNOWN = 0,
    ID_CLIP_PATH,
    ID_CLIP_RULE,
    ID_CLIP_PATH_UNITS,
    ID_COLOR,
    ID_CX,
    ID_CY,
    ID_D,
    ID_DISPLAY,
    ID_FILL,
    ID_FILL_OPACITY,
    ID_FILL_RULE,
    ID_FX,
    ID_FY,
    ID_GRADIENT_TRANSFORM,
    ID_GRADIENT_UNITS,
    ID_HEIGHT,
    ID_ID,
    ID_OFFSET,
    ID_OPACITY,
    ID_OVERFLOW,
    ID_POINTS,
    ID_PRESERVE_ASPECT_RATIO,
    ID_R,
    ID_RX,
    ID_RY,
    ID_SOLID_COLOR,
    ID_SOLID_OPACITY,
    ID_SPREAD_METHOD,
    ID_STOP_COLOR,
    ID_STOP_OPACITY,
    ID_STROKE,
    ID_STROKE_DASHARRAY,
    ID_STROKE_DASHOFFSET,
    ID_STROKE_LINECAP,
    ID_STROKE_LINEJOIN,
    ID_STROKE_MITERLIMIT,
    ID_STROKE_OPACITY,
    ID_STROKE_WIDTH,
    ID_TRANSFORM,
    ID_VIEWBOX,
    ID_VISIBILITY,
    ID_WIDTH,
    ID_X,
    ID_X1,
    ID_X2,
    ID_XLINK_HREF,
    ID_Y,
    ID_Y1,
    ID_Y2
};

static const name_entry_t propertymap[] = {
    {"clip-path", ID_CLIP_PATH},
    {"clip-rule", ID_CLIP_RULE},
    {"clipPathUnits", ID_CLIP_PATH_UNITS},
    {"color", ID_COLOR},
    {"cx", ID_CX},
    {"cy", ID_CY},
    {"d", ID_D},
    {"display", ID_DISPLAY},
    {"fill", ID_FILL},
    {"fill-opacity", ID_FILL_OPACITY},
    {"fill-rule", ID_FILL_RULE},
    {"fx", ID_FX},
    {"fy", ID_FY},
    {"gradientTransform", ID_GRADIENT_TRANSFORM},
    {"gradientUnits", ID_GRADIENT_UNITS},
    {"height", ID_HEIGHT},
    {"id", ID_ID},
    {"offset", ID_OFFSET},
    {"opacity", ID_OPACITY},
    {"overflow", ID_OVERFLOW},
    {"points", ID_POINTS},
    {"preserveAspectRatio", ID_PRESERVE_ASPECT_RATIO},
    {"r", ID_R},
    {"rx", ID_RX},
    {"ry", ID_RY},
    {"solid-color", ID_SOLID_COLOR},
    {"solid-opacity", ID_SOLID_OPACITY},
    {"spreadMethod", ID_SPREAD_METHOD},
    {"stop-color", ID_STOP_COLOR},
    {"stop-opacity", ID_STOP_OPACITY},
    {"stroke", ID_STROKE},
    {"stroke-dasharray", ID_STROKE_DASHARRAY},
    {"stroke-dashoffset", ID_STROKE_DASHOFFSET},
    {"stroke-linecap", ID_STROKE_LINECAP},
    {"stroke-linejoin", ID_STROKE_LINEJOIN},
    {"stroke-miterlimit", ID_STROKE_MITERLIMIT},
    {"stroke-opacity", ID_STROKE_OPACITY},
    {"stroke-width", ID_STROKE_WIDTH},
    {"transform", ID_TRANSFORM},
    {"viewBox", ID_VIEWBOX},
    {"visibility", ID_VISIBILITY},
    {"width", ID_WIDTH},
    {"x", ID_X},
    {"x1", ID_X1},
    {"x2", ID_X2},
    {"xlink:href", ID_XLINK_HREF},
    {"y", ID_Y},
    {"y1", ID_Y1},
    {"y2", ID_Y2}
};

static int propertyid(const char* data, size_t length)
{
    if(length == 0 || length >= 24)
        return ID_UNKNOWN;
    char name[24];
    for(int i = 0; i < length; i++)
        name[i] = data[i];
    name[length] = 0;

    size_t count = sizeof(propertymap);
    size_t size = sizeof(name_entry_t);
    name_entry_t* entry = bsearch(name, propertymap, count / size, size, name_entry_compare);
    if(entry == NULL)
        return ID_UNKNOWN;
    return entry->id;
}

enum {
    TAG_UNKNOWN = 0,
    TAG_CIRCLE,
    TAG_CLIP_PATH,
    TAG_DEFS,
    TAG_ELLIPSE,
    TAG_G,
    TAG_LINE,
    TAG_LINEAR_GRADIENT,
    TAG_PATH,
    TAG_POLYGON,
    TAG_POLYLINE,
    TAG_RADIAL_GRADIENT,
    TAG_RECT,
    TAG_SOLID_COLOR,
    TAG_STOP,
    TAG_SVG,
    TAG_USE
};

static const name_entry_t elementmap[] = {
    {"circle", TAG_CIRCLE},
    {"clipPath", TAG_CLIP_PATH},
    {"defs", TAG_DEFS},
    {"ellipse", TAG_ELLIPSE},
    {"g", TAG_G},
    {"line", TAG_LINE},
    {"linearGradient", TAG_LINEAR_GRADIENT},
    {"path", TAG_PATH},
    {"polygon", TAG_POLYGON},
    {"polyline", TAG_POLYLINE},
    {"radialGradient", TAG_RADIAL_GRADIENT},
    {"rect", TAG_RECT},
    {"solidColor", TAG_SOLID_COLOR},
    {"stop", TAG_STOP},
    {"svg", TAG_SVG},
    {"use", TAG_USE}
};

static int elementid(const char* data, size_t length)
{
    if(length == 0 || length >= 16)
        return TAG_UNKNOWN;
    char name[16];
    for(int i = 0; i < length; i++)
        name[i] = data[i];
    name[length] = 0;

    size_t count = sizeof(elementmap);
    size_t size = sizeof(name_entry_t);
    name_entry_t* entry = bsearch(name, elementmap, count / size, size, name_entry_compare);
    if(entry == NULL)
        return TAG_UNKNOWN;
    return entry->id;
}

typedef struct {
    const char* data;
    size_t length;
} string_t;

typedef struct property {
    int id;
    string_t value;
    struct property* next;
} property_t;

typedef struct element {
    int id;
    struct element* parent;
    struct element* nextchild;
    struct element* lastchild;
    struct element* firstchild;
    struct property* property;
} element_t;

typedef struct heap_chunk {
    struct heap_chunk* next;
} heap_chunk_t;

typedef struct {
    heap_chunk_t* chunk;
    heap_chunk_t* freedchunk;
    size_t size;
} heap_t;

static heap_t* heap_create(void)
{
    heap_t* heap = malloc(sizeof(heap_t));
    heap->chunk = NULL;
    heap->freedchunk = NULL;
    heap->size = 0;
    return heap;
}

#define CHUNK_SIZE 4096
#define ALIGN_SIZE(size) (((size) + 7ul) & ~7ul)
static void* heap_alloc(heap_t* heap, size_t size)
{
    if(heap->chunk == NULL || heap->size + ALIGN_SIZE(size) > CHUNK_SIZE) {
        heap_chunk_t* chunk = heap->freedchunk;
        if(chunk == NULL) {
            chunk = malloc(sizeof(heap_chunk_t) + CHUNK_SIZE);
        } else {
            heap->freedchunk = chunk->next;
        }

        chunk->next = heap->chunk;
        heap->chunk = chunk;
        heap->size = 0;
    }

    void* data = (char*)(heap->chunk) + sizeof(heap_chunk_t) + heap->size;
    heap->size += ALIGN_SIZE(size);
    return data;
}

static void heap_clear(heap_t* heap)
{
    heap->freedchunk = heap->chunk;
    heap->chunk = NULL;
    heap->size = 0;
}

static void heap_destroy(heap_t* heap)
{
    while(heap->chunk) {
        heap_chunk_t* chunk = heap->chunk;
        heap->chunk = chunk->next;
        free(chunk);
    }

    while(heap->freedchunk) {
        heap_chunk_t* chunk = heap->freedchunk;
        heap->freedchunk = chunk->next;
        free(chunk);
    }

    free(heap);
}

typedef struct hashmap_entry {
    size_t hash;
    string_t name;
    void* value;
    struct hashmap_entry* next;
} hashmap_entry_t;

typedef struct {
    hashmap_entry_t** buckets;
    size_t size;
    size_t capacity;
} hashmap_t;

static hashmap_t* hashmap_create(void)
{
    hashmap_t* map = malloc(sizeof(hashmap_t));
    map->buckets = calloc(16, sizeof(hashmap_entry_t*));
    map->size = 0;
    map->capacity = 16;
    return map;
}

static size_t hashmap_hash(const char* data, size_t length)
{
    size_t h = length;
    for(int i = 0; i < length; i++) {
        h = h * 31 + *data;
        ++data;
    }

    return h;
}

static bool hashmap_eq(const hashmap_entry_t* entry, const char* data, size_t length)
{
    const string_t* name = &entry->name;
    if(name->length != length)
        return false;
    for(int i = 0; i < length; i++) {
        if(data[i] != name->data[i]) {
            return false;
        }
    }

    return true;
}

static void hashmap_expand(hashmap_t* map)
{
    if(map->size > (map->capacity * 3 / 4)) {
        size_t newcapacity = map->capacity << 1;
        hashmap_entry_t** newbuckets = calloc(newcapacity, sizeof(hashmap_entry_t*));
        for(int i = 0; i < map->capacity; i++) {
            hashmap_entry_t* entry = map->buckets[i];
            while(entry) {
                hashmap_entry_t* next = entry->next;
                size_t index = entry->hash & (newcapacity - 1);
                entry->next = newbuckets[index];
                newbuckets[index] = entry;
                entry = next;
            }
        }

        free(map->buckets);
        map->buckets = newbuckets;
        map->capacity = newcapacity;
    }
}

static void hashmap_put(hashmap_t* map, heap_t* heap, const char* data, size_t length, void* value)
{
    size_t hash = hashmap_hash(data, length);
    size_t index = hash & (map->capacity - 1);

    hashmap_entry_t** p = &map->buckets[index];
    while(true) {
        hashmap_entry_t* current = *p;
        if(current == NULL) {
            hashmap_entry_t* entry = heap_alloc(heap, sizeof(hashmap_entry_t));
            entry->name.data = data;
            entry->name.length = length;
            entry->hash = hash;
            entry->value = value;
            entry->next = NULL;
            *p = entry;
            map->size += 1;
            hashmap_expand(map);
            break;
        }

        if(current->hash == hash && hashmap_eq(current, data, length)) {
            current->value = value;
            break;
        }

        p = &current->next;
    }
}

static void* hashmap_get(const hashmap_t* map, const char* data, size_t length)
{
    size_t hash = hashmap_hash(data, length);
    size_t index = hash & (map->capacity - 1);

    hashmap_entry_t* entry = map->buckets[index];
    while(entry) {
        if(entry->hash == hash && hashmap_eq(entry, data, length))
            return entry->value;
        entry = entry->next;
    }

    return NULL;
}

static void hashmap_clear(hashmap_t* map)
{
    memset(map->buckets, 0, map->capacity * sizeof(hashmap_entry_t*));
    map->size = 0;
}

static void hashmap_destroy(hashmap_t* map)
{
    free(map->buckets);
    free(map);
}

struct otfsvg_document {
    element_t* root;
    hashmap_t* idcache;
    heap_t* heap;
    otfsvg_canvas_t* canvas;
    void* canvas_data;
    otfsvg_palette_func_t palette_func;
    void* palette_data;
    otfsvg_path_t path;
    otfsvg_paint_t paint;
    otfsvg_stroke_data_t strokedata;
    otfsvg_color_t current_color;
    float width;
    float height;
    float dpi;
};

static inline const string_t* property_get(element_t* element, int id)
{
    const property_t* property = element->property;
    while(property != NULL) {
        if(property->id == id)
            return &property->value;
        property = property->next;
    }

    return NULL;
}

static inline const string_t* property_find(element_t* element, int id)
{
    while(element != NULL) {
        const string_t* value = property_get(element, id);
        if(value != NULL)
            return value;
        element = element->parent;
    }

    return NULL;
}

static inline bool property_has(element_t* element, int id)
{
    const property_t* property = element->property;
    while(property != NULL) {
        if(property->id == id)
            return true;
        property = property->next;
    }

    return false;
}

static inline const string_t* property_search(element_t* element, int id, bool inherit)
{
    const string_t* value = property_get(element, id);
    if(value == NULL && inherit)
        return property_find(element->parent, id);
    return value;
}

static inline bool parse_float(const char** begin, const char* end, float* number)
{
    const char* it = *begin;
    float fraction = 0;
    float integer = 0;
    float exponent = 0;
    int sign = 1;
    int expsign = 1;

    if(it < end && *it == '+')
        ++it;
    else if(it < end && *it == '-') {
        ++it;
        sign = -1;
    }

    if(it >= end || (!IS_NUM(*it) && *it != '.'))
        return false;

    if(*it != '.') {
        while(*it && IS_NUM(*it))
            integer = 10.0 * integer + (*it++ - '0');
    }

    if(it < end && *it == '.') {
        ++it;
        if(it >= end || !IS_NUM(*it))
            return false;
        float div = 1.0;
        while(it < end && IS_NUM(*it)) {
            fraction = 10.0 * fraction + (*it++ - '0');
            div *= 10.0;
        }

        fraction /= div;
    }

    if(it < end && (*it == 'e' || *it == 'E') && (it[1] != 'x' && it[1] != 'm')) {
        ++it;
        if(it < end && *it == '+')
            ++it;
        else if(it < end && *it == '-') {
            ++it;
            expsign = -1;
        }

        if(it >= end || !IS_NUM(*it))
            return false;

        while(it < end && IS_NUM(*it))
            exponent = 10 * exponent + (*it++ - '0');
    }

    *begin = it;
    *number = sign * (integer + fraction);
    if(exponent)
        *number *= powf(10.0, expsign * exponent);
    return *number >= -FLT_MAX && *number <= FLT_MAX;
}

static bool parse_number(element_t* element, int id, float* number, bool percent, bool inherit)
{
    const string_t* value = property_search(element, id, inherit);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(!parse_float(&it, end, number))
        return false;

    if(percent) {
        if(skip_delim(&it, end, '%'))
            *number /= 100.0;
        *number = otfsvg_clamp(*number, 0.0, 1.0);
    }

    return true;
}

typedef enum {
    length_type_unknown,
    length_type_percent,
    length_type_number,
    length_type_px,
    length_type_pt,
    length_type_pc,
    length_type_in,
    length_type_cm,
    length_type_mm,
} length_type_t;

typedef struct {
    float value;
    length_type_t type;
} length_t;

#define is_length_zero(length) (length.value == 0)
#define is_length_relative(length) (length.type == length_type_percent)
#define is_length_valid(length) (length.type != length_type_unknown)
static bool parse_length_value(const char** begin, const char* end, length_t* length, bool negative)
{
    const char* it = *begin;
    if(!parse_float(&it, end, &length->value))
        return false;

    if(!negative && length->value < 0.0)
        return false;
    length->type = length_type_number;
    if(skip_delim(&it, end, '%'))
        length->type = length_type_percent;
    else if(skip_string(&it, end, "px"))
        length->type = length_type_px;
    else if(skip_string(&it, end, "pt"))
        length->type = length_type_pt;
    else if(skip_string(&it, end, "pc"))
        length->type = length_type_pc;
    else if(skip_string(&it, end, "in"))
        length->type = length_type_in;
    else if(skip_string(&it, end, "cm"))
        length->type = length_type_cm;
    else if(skip_string(&it, end, "mm"))
        length->type = length_type_mm;

    *begin = it;
    skip_ws(begin, end);
    return true;
}

static bool parse_length(element_t* element, int id, length_t* length, bool negative, bool inherit)
{
    const string_t* value = property_search(element, id, inherit);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(parse_length_value(&it, end, length, negative))
        return it == end;
    return false;
}

static inline float convert_length(const length_t* length, float max, float dpi)
{
    switch(length->type) {
    case length_type_number:
    case length_type_px:
        return length->value;
    case length_type_in:
        return length->value * dpi;
    case length_type_cm:
        return length->value * dpi / 2.54;
    case length_type_mm:
        return length->value * dpi / 25.4;
    case length_type_pt:
        return length->value * dpi / 72.0;
    case length_type_pc:
        return length->value * dpi / 6.0;
    case length_type_percent:
        return length->value * max / 100.0;
    default:
        return 0.0;
    }
}

typedef enum {
    color_type_fixed,
    color_type_current
} color_type_t;

typedef struct {
    color_type_t type;
    uint32_t value;
} color_t;

typedef enum {
    paint_type_none,
    paint_type_color,
    paint_type_url,
    paint_type_var
} paint_type_t;

typedef struct {
    paint_type_t type;
    color_t color;
    string_t id;
} paint_t;

typedef struct {
    const char* name;
    uint32_t value;
} color_entry_t;

static int color_entry_compare(const void* a, const void* b)
{
    const char* name = a;
    const color_entry_t* entry = b;
    return strcmp(name, entry->name);
}

static color_entry_t colormap[] = {
    {"aliceblue", 0xF0F8FF},
    {"antiquewhite", 0xFAEBD7},
    {"aqua", 0x00FFFF},
    {"aquamarine", 0x7FFFD4},
    {"azure", 0xF0FFFF},
    {"beige", 0xF5F5DC},
    {"bisque", 0xFFE4C4},
    {"black", 0x000000},
    {"blanchedalmond", 0xFFEBCD},
    {"blue", 0x0000FF},
    {"blueviolet", 0x8A2BE2},
    {"brown", 0xA52A2A},
    {"burlywood", 0xDEB887},
    {"cadetblue", 0x5F9EA0},
    {"chartreuse", 0x7FFF00},
    {"chocolate", 0xD2691E},
    {"coral", 0xFF7F50},
    {"cornflowerblue", 0x6495ED},
    {"cornsilk", 0xFFF8DC},
    {"crimson", 0xDC143C},
    {"cyan", 0x00FFFF},
    {"darkblue", 0x00008B},
    {"darkcyan", 0x008B8B},
    {"darkgoldenrod", 0xB8860B},
    {"darkgray", 0xA9A9A9},
    {"darkgreen", 0x006400},
    {"darkgrey", 0xA9A9A9},
    {"darkkhaki", 0xBDB76B},
    {"darkmagenta", 0x8B008B},
    {"darkolivegreen", 0x556B2F},
    {"darkorange", 0xFF8C00},
    {"darkorchid", 0x9932CC},
    {"darkred", 0x8B0000},
    {"darksalmon", 0xE9967A},
    {"darkseagreen", 0x8FBC8F},
    {"darkslateblue", 0x483D8B},
    {"darkslategray", 0x2F4F4F},
    {"darkslategrey", 0x2F4F4F},
    {"darkturquoise", 0x00CED1},
    {"darkviolet", 0x9400D3},
    {"deeppink", 0xFF1493},
    {"deepskyblue", 0x00BFFF},
    {"dimgray", 0x696969},
    {"dimgrey", 0x696969},
    {"dodgerblue", 0x1E90FF},
    {"firebrick", 0xB22222},
    {"floralwhite", 0xFFFAF0},
    {"forestgreen", 0x228B22},
    {"fuchsia", 0xFF00FF},
    {"gainsboro", 0xDCDCDC},
    {"ghostwhite", 0xF8F8FF},
    {"gold", 0xFFD700},
    {"goldenrod", 0xDAA520},
    {"gray", 0x808080},
    {"green", 0x008000},
    {"greenyellow", 0xADFF2F},
    {"grey", 0x808080},
    {"honeydew", 0xF0FFF0},
    {"hotpink", 0xFF69B4},
    {"indianred", 0xCD5C5C},
    {"indigo", 0x4B0082},
    {"ivory", 0xFFFFF0},
    {"khaki", 0xF0E68C},
    {"lavender", 0xE6E6FA},
    {"lavenderblush", 0xFFF0F5},
    {"lawngreen", 0x7CFC00},
    {"lemonchiffon", 0xFFFACD},
    {"lightblue", 0xADD8E6},
    {"lightcoral", 0xF08080},
    {"lightcyan", 0xE0FFFF},
    {"lightgoldenrodyellow", 0xFAFAD2},
    {"lightgray", 0xD3D3D3},
    {"lightgreen", 0x90EE90},
    {"lightgrey", 0xD3D3D3},
    {"lightpink", 0xFFB6C1},
    {"lightsalmon", 0xFFA07A},
    {"lightseagreen", 0x20B2AA},
    {"lightskyblue", 0x87CEFA},
    {"lightslategray", 0x778899},
    {"lightslategrey", 0x778899},
    {"lightsteelblue", 0xB0C4DE},
    {"lightyellow", 0xFFFFE0},
    {"lime", 0x00FF00},
    {"limegreen", 0x32CD32},
    {"linen", 0xFAF0E6},
    {"magenta", 0xFF00FF},
    {"maroon", 0x800000},
    {"mediumaquamarine", 0x66CDAA},
    {"mediumblue", 0x0000CD},
    {"mediumorchid", 0xBA55D3},
    {"mediumpurple", 0x9370DB},
    {"mediumseagreen", 0x3CB371},
    {"mediumslateblue", 0x7B68EE},
    {"mediumspringgreen", 0x00FA9A},
    {"mediumturquoise", 0x48D1CC},
    {"mediumvioletred", 0xC71585},
    {"midnightblue", 0x191970},
    {"mintcream", 0xF5FFFA},
    {"mistyrose", 0xFFE4E1},
    {"moccasin", 0xFFE4B5},
    {"navajowhite", 0xFFDEAD},
    {"navy", 0x000080},
    {"oldlace", 0xFDF5E6},
    {"olive", 0x808000},
    {"olivedrab", 0x6B8E23},
    {"orange", 0xFFA500},
    {"orangered", 0xFF4500},
    {"orchid", 0xDA70D6},
    {"palegoldenrod", 0xEEE8AA},
    {"palegreen", 0x98FB98},
    {"paleturquoise", 0xAFEEEE},
    {"palevioletred", 0xDB7093},
    {"papayawhip", 0xFFEFD5},
    {"peachpuff", 0xFFDAB9},
    {"peru", 0xCD853F},
    {"pink", 0xFFC0CB},
    {"plum", 0xDDA0DD},
    {"powderblue", 0xB0E0E6},
    {"purple", 0x800080},
    {"rebeccapurple", 0x663399},
    {"red", 0xFF0000},
    {"rosybrown", 0xBC8F8F},
    {"royalblue", 0x4169E1},
    {"saddlebrown", 0x8B4513},
    {"salmon", 0xFA8072},
    {"sandybrown", 0xF4A460},
    {"seagreen", 0x2E8B57},
    {"seashell", 0xFFF5EE},
    {"sienna", 0xA0522D},
    {"silver", 0xC0C0C0},
    {"skyblue", 0x87CEEB},
    {"slateblue", 0x6A5ACD},
    {"slategray", 0x708090},
    {"slategrey", 0x708090},
    {"snow", 0xFFFAFA},
    {"springgreen", 0x00FF7F},
    {"steelblue", 0x4682B4},
    {"tan", 0xD2B48C},
    {"teal", 0x008080},
    {"thistle", 0xD8BFD8},
    {"tomato", 0xFF6347},
    {"turquoise", 0x40E0D0},
    {"violet", 0xEE82EE},
    {"wheat", 0xF5DEB3},
    {"white", 0xFFFFFF},
    {"whitesmoke", 0xF5F5F5},
    {"yellow", 0xFFFF00},
    {"yellowgreen", 0x9ACD32}
};

static bool parse_color_component(const char** begin, const char* end, int* component)
{
    float value = 0;
    if(!parse_float(begin, end, &value))
        return false;

    if(skip_delim(begin, end, '%'))
        value *= 2.55f;
    *component = roundf(value);
    *component = otfsvg_clamp(*component, 0.f, 255.f);
    return true;
}

static bool parse_color_value(const char** begin, const char* end, color_t* color)
{
    const char* it = *begin;
    if(skip_delim(&it, end, '#')) {
        const char* begin = it;
        uint32_t value = 0;
        while(it < end && isxdigit(*it)) {
            const char cc = *it++;
            unsigned int digit = 0;
            if(cc >= '0' && cc <= '9')
                digit = cc - '0';
            else if(cc >= 'a' && cc <= 'f')
                digit = 10 + cc - 'a';
            else if(cc >= 'A' && cc <= 'F')
                digit = 10 + cc - 'A';
            value = value * 16 + digit;
        }

        int count = (int)(it - begin);
        if(count != 6 && count != 3)
            return false;
        if(count == 3) {
            value = ((value&0xf00) << 8) | ((value&0x0f0) << 4) | (value&0x00f);
            value |= value << 4;
        }

        color->type = color_type_fixed;
        color->value = value | 0xFF000000;
    } else if(skip_string(&it, end, "currentColor")) {
        color->type = color_type_current;
        color->value = 0xFF000000;
    } else if(skip_string(&it, end, "rgb(")) {
        int red = 0;
        int green = 0;
        int blue = 0;
        if(!skip_ws(&it, end)
            || !parse_color_component(&it, end, &red)
            || !skip_ws_comma(&it, end)
            || !parse_color_component(&it, end, &green)
            || !skip_ws_comma(&it, end)
            || !parse_color_component(&it, end, &blue)
            || !skip_ws(&it, end)
            || !skip_delim(&it, end, ')')) {
            return false;
        }

        color->type = color_type_fixed;
        color->value = (0xFF000000 | red << 16 | green << 8 | blue);
    } else {
        int length = 0;
        char name[24];
        while(it < end && length < 24 && isalpha(*it))
            name[length++] = tolower(*it++);
        if(length == 0)
            return false;
        name[length] = 0;

        size_t count = sizeof(colormap);
        size_t size = sizeof(color_entry_t);
        color_entry_t* entry = bsearch(name, colormap, count / size, size, color_entry_compare);
        if(entry == NULL)
            return false;
        color->type = color_type_fixed;
        color->value = entry->value | 0xFF000000;
    }

    *begin = it;
    skip_ws(begin, end);
    return true;
}

static bool parse_color(element_t* element, int id, color_t* color)
{
    const string_t* value = property_find(element, id);
    if(value == NULL)
        return false;
    const char* it = value->data;
    const char* end = it + value->length;
    if(parse_color_value(&it, end, color))
        return it == end;
    return false;
}

static bool parse_paint(element_t* element, int id, paint_t* paint)
{
    const string_t* value = property_find(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(skip_string(&it, end, "none")) {
        paint->type = paint_type_none;
        return !skip_ws(&it, end);
    }

    if(skip_string(&it, end, "url(")) {
        if(!skip_ws(&it, end) || !skip_delim(&it, end, '#'))
            return false;
        const char* begin = it;
        while(it < end && *it != ')')
            ++it;

        paint->type = paint_type_url;
        paint->id.data = begin;
        paint->id.length = it - begin;
        paint->color.value = otfsvg_transparent_color;
        if(!skip_delim(&it, end, ')') || (skip_ws(&it, end) && !parse_color_value(&it, end, &paint->color)))
            return false;
        return it == end;
    }

    if(skip_string(&it, end, "var(")) {
        if(!skip_ws(&it, end) || !skip_string(&it, end, "--"))
            return false;
        const char* begin = it;
        while(it < end && IS_NAMECHAR(*it))
            ++it;
        paint->type = paint_type_var;
        paint->id.data = begin;
        paint->id.length = it - begin;
        paint->color.value = otfsvg_transparent_color;
        if(skip_ws(&it, end) && skip_delim(&it, end, ',') && !(skip_ws(&it, end) && parse_color_value(&it, end, &paint->color)))
            return false;
        return skip_delim(&it, end, ')') && !skip_ws(&it, end);
    }

    if(parse_color_value(&it, end, &paint->color)) {
        paint->type = paint_type_color;
        return it == end;
    }

    return false;
}

static bool parse_view_box(element_t* element, int id, otfsvg_rect_t* viewbox)
{
    const string_t* value = property_get(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    if(!parse_float(&it, end, &x)
            || !skip_ws_comma(&it, end)
            || !parse_float(&it, end, &y)
            || !skip_ws_comma(&it, end)
            || !parse_float(&it, end, &w)
            || !skip_ws_comma(&it, end)
            || !parse_float(&it, end, &h)
            || skip_ws(&it, end)) {
        return false;
    }

    if(w < 0.f || h < 0.f)
        return false;

    viewbox->x = x;
    viewbox->y = y;
    viewbox->w = w;
    viewbox->h = h;
    return true;
}

typedef enum {
    transform_type_matrix,
    transform_type_rotate,
    transform_type_scale,
    transform_type_skew_x,
    transform_type_skew_y,
    transform_type_translate
} transform_type_t;

static bool parse_transform_value(const char** begin, const char* end, transform_type_t* type, float values[6], int* count)
{
    const char* it = *begin;
    int required = 0;
    int optional = 0;
    if(skip_string(&it, end, "matrix")) {
        *type = transform_type_matrix;
        required = 6;
        optional = 0;
    } else if(skip_string(&it, end, "rotate")) {
        *type = transform_type_rotate;
        required = 1;
        optional = 2;
    } else if(skip_string(&it, end, "scale")) {
        *type = transform_type_scale;
        required = 1;
        optional = 1;
    } else if(skip_string(&it, end, "skewX")) {
        *type = transform_type_skew_x;
        required = 1;
        optional = 0;
    } else if(skip_string(&it, end, "skewY")) {
        *type = transform_type_skew_y;
        required = 1;
        optional = 0;
    } else if(skip_string(&it, end, "translate")) {
        *type = transform_type_translate;
        required = 1;
        optional = 1;
    } else {
        return false;
    }

    if(!skip_ws(&it, end) || !skip_delim(&it, end, '('))
        return false;

    int i = 0;
    int max_count = required + optional;
    skip_ws(&it, end);
    while(i < max_count) {
        if(!parse_float(&it, end, values + i))
            break;
        ++i;
        skip_ws_comma(&it, end);
    }

    if(it >= end || *it != ')' || !(i == required || i == max_count))
        return false;
    ++it;

    *begin = it;
    *count = i;
    return true;
}

static bool parse_transform(element_t* element, int id, otfsvg_matrix_t* matrix)
{
    otfsvg_matrix_init_identity(matrix);
    const string_t* value = property_get(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;

    float values[6];
    int count;
    transform_type_t type;
    while(it < end) {
        if(!parse_transform_value(&it, end, &type, values, &count))
            return false;
        skip_ws_comma(&it, end);
        switch(type) {
        case transform_type_matrix:
            otfsvg_matrix_multiply(matrix, (otfsvg_matrix_t*)(values), matrix);
            break;
        case transform_type_rotate:
            if(count == 1)
                otfsvg_matrix_rotate(matrix, values[0], 0, 0);
            else
                otfsvg_matrix_rotate(matrix, values[0], values[1], values[2]);
            break;
        case transform_type_scale:
            if(count == 1)
                otfsvg_matrix_scale(matrix, values[0], values[0]);
            else
                otfsvg_matrix_scale(matrix, values[0], values[1]);
            break;
        case transform_type_skew_x:
            otfsvg_matrix_shear(matrix, values[0], 0);
            break;
        case transform_type_skew_y:
            otfsvg_matrix_shear(matrix, 0, values[0]);
            break;
        case transform_type_translate:
            if(count == 1)
                otfsvg_matrix_translate(matrix, values[0], 0);
            else
                otfsvg_matrix_translate(matrix, values[0], values[1]);
            break;
        }
    }

    return true;
}

static bool parse_coordinates(const char** begin, const char* end, float* points, int count)
{
    const char* it = *begin;
    for(int i = 0; i < count; i++) {
        if(!parse_float(&it, end, points + i))
            return false;
        skip_ws_comma(&it, end);
    }

    *begin = it;
    return true;
}

static bool parse_arc_flag(const char** begin, const char* end, bool* flag)
{
    if(skip_delim(begin, end, '0'))
        *flag = 0;
    else if(skip_delim(begin, end, '1'))
        *flag = 1;
    else
        return false;

    skip_ws_comma(begin, end);
    return true;
}

static bool parse_path(element_t* element, int id, otfsvg_path_t* path)
{
    otfsvg_path_clear(path);
    const string_t* value = property_get(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(it >= end || !(*it == 'M' || *it == 'm'))
        return false;

    char command = *it++;
    float c[6];
    bool f[2];

    float start_x = 0;
    float start_y = 0;
    float last_control_x = 0;
    float last_control_y = 0;
    float current_x = 0;
    float current_y = 0;
    while(true) {
        skip_ws(&it, end);
        if(command == 'M' || command == 'm') {
            if(!parse_coordinates(&it, end, c, 2))
                return false;
            if(command == 'm') {
                c[0] += current_x;
                c[1] += current_y;
            }

            otfsvg_path_move_to(path, c[0], c[1]);
            current_x = start_x = last_control_x = c[0];
            current_y = start_y = last_control_y = c[1];
            command = command == 'm' ? 'l' : 'L';
        } else if(command == 'L' || command == 'l') {
            if(!parse_coordinates(&it, end, c, 2))
                return false;
            if(command == 'l') {
                c[0] += current_x;
                c[1] += current_y;
            }

            otfsvg_path_line_to(path, c[0], c[1]);
            current_x = last_control_x = c[0];
            current_y = last_control_y = c[1];
        } else if(command == 'Q' || command == 'q') {
            if(!parse_coordinates(&it, end, c, 4))
                return false;
            if(command == 'q') {
                c[0] += current_x;
                c[1] += current_y;
                c[2] += current_x;
                c[3] += current_y;
            }

            otfsvg_path_quad_to(path, current_x, current_y, c[0], c[1], c[2], c[3]);
            last_control_x = c[0];
            last_control_y = c[1];
            current_x = c[2];
            current_y = c[3];
        } else if(command == 'C' || command == 'c') {
            if(!parse_coordinates(&it, end, c, 6))
                return false;
            if(command == 'c') {
                c[0] += current_x;
                c[1] += current_y;
                c[2] += current_x;
                c[3] += current_y;
                c[4] += current_x;
                c[5] += current_y;
            }

            otfsvg_path_cubic_to(path, c[0], c[1], c[2], c[3], c[4], c[5]);
            last_control_x = c[2];
            last_control_y = c[3];
            current_x = c[4];
            current_y = c[5];
        } else if(command == 'T' || command == 't') {
            c[0] = 2 * current_x - last_control_x;
            c[1] = 2 * current_y - last_control_y;
            if(!parse_coordinates(&it, end, c + 2, 2))
                return false;
            if(command == 't') {
                c[2] += current_x;
                c[3] += current_y;
            }

            otfsvg_path_quad_to(path, current_x, current_y, c[0], c[1], c[2], c[3]);
            last_control_x = c[0];
            last_control_y = c[1];
            current_x = c[2];
            current_y = c[3];
        } else if(command == 'S' || command == 's') {
            c[0] = 2 * current_x - last_control_x;
            c[1] = 2 * current_y - last_control_y;
            if(!parse_coordinates(&it, end, c + 2, 4))
                return false;
            if(command == 's') {
                c[2] += current_x;
                c[3] += current_y;
                c[4] += current_x;
                c[5] += current_y;
            }

            otfsvg_path_cubic_to(path, c[0], c[1], c[2], c[3], c[4], c[5]);
            last_control_x = c[2];
            last_control_y = c[3];
            current_x = c[4];
            current_y = c[5];
        } else if(command == 'H' || command == 'h') {
            if(!parse_coordinates(&it, end, c, 1))
                return false;
            if(command == 'h')
                c[0] += current_x;

            otfsvg_path_line_to(path, c[0], current_y);
            current_x = last_control_x = c[0];
        } else if(command == 'V' || command == 'v') {
            if(!parse_coordinates(&it, end, c + 1, 1))
                return false;
            if(command == 'v')
                c[1] += current_y;

            otfsvg_path_line_to(path, current_x, c[1]);
            current_y = last_control_y = c[1];
        } else if(command == 'A' || command == 'a') {
            if(!parse_coordinates(&it, end, c, 3)
                || !parse_arc_flag(&it, end, f)
                || !parse_arc_flag(&it, end, f + 1)
                || !parse_coordinates(&it, end, c + 3, 2)) {
                return false;
            }

            if(command == 'a') {
                c[3] += current_x;
                c[4] += current_y;
            }

            otfsvg_path_arc_to(path, current_x, current_y, c[0], c[1], c[2], f[0], f[1], c[3], c[4]);
            current_x = last_control_x = c[3];
            current_y = last_control_y = c[4];
        } else if(command == 'Z' || command == 'z'){
            otfsvg_path_close(path);
            current_x = last_control_x = start_x;
            current_y = last_control_y = start_y;
        } else {
            return false;
        }

        skip_ws_comma(&it, end);
        if(it >= end)
            break;

        if(IS_ALPHA(*it))
            command = *it++;
    }

    return true;
}

static bool parse_points(element_t* element, int id, otfsvg_path_t* path)
{
    otfsvg_path_clear(path);
    const string_t* value = property_get(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;

    float c[2];
    if(!parse_coordinates(&it, end, c, 2))
        return false;

    otfsvg_path_move_to(path, c[0], c[1]);
    skip_ws_comma(&it, end);
    while(it < end) {
        if(!parse_coordinates(&it, end, c, 2))
            return false;
        otfsvg_path_line_to(path, c[0], c[1]);
        skip_ws_comma(&it, end);
    }

    return true;
}

typedef enum {
    position_align_none,
    position_align_x_min_y_min,
    position_align_x_mid_y_min,
    position_align_x_max_y_min,
    position_align_x_min_y_mid,
    position_align_x_mid_y_mid,
    position_align_x_max_y_mid,
    position_align_x_min_y_max,
    position_align_x_mid_y_max,
    position_align_x_max_y_max
} position_align_t;

typedef enum {
    position_scale_meet,
    position_scale_slice
} position_scale_t;

typedef struct {
    position_align_t align;
    position_scale_t scale;
} position_t;

static bool parse_position(element_t* element, int id, position_t* position)
{
    const string_t* value = property_get(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(skip_string(&it, end, "none"))
        position->align = position_align_none;
    else if(skip_string(&it, end, "xMinYMin"))
        position->align = position_align_x_min_y_min;
    else if(skip_string(&it, end, "xMidYMin"))
        position->align = position_align_x_mid_y_min;
    else if(skip_string(&it, end, "xMaxYMin"))
        position->align = position_align_x_max_y_min;
    else if(skip_string(&it, end, "xMinYMid"))
        position->align = position_align_x_min_y_mid;
    else if(skip_string(&it, end, "xMidYMid"))
        position->align = position_align_x_mid_y_mid;
    else if(skip_string(&it, end, "xMaxYMid"))
        position->align = position_align_x_max_y_mid;
    else if(skip_string(&it, end, "xMinYMax"))
        position->align = position_align_x_min_y_max;
    else if(skip_string(&it, end, "xMidYMax"))
        position->align = position_align_x_mid_y_max;
    else if(skip_string(&it, end, "xMaxYMax"))
        position->align = position_align_x_max_y_max;
    else
        return false;

    position->scale = position_scale_meet;
    if(position->align != position_align_none) {
        skip_ws(&it, end);
        if(skip_string(&it, end, "meet"))
            position->scale = position_scale_meet;
        else if(skip_string(&it, end, "slice"))
            position->scale = position_scale_slice;
    }

    return !skip_ws(&it, end);
}

static void position_get_rect(const position_t* position, otfsvg_rect_t* rect, const otfsvg_rect_t* clip, float width, float height)
{
    rect->x = clip->x;
    rect->y = clip->y;
    if(position->align == position_align_none) {
        rect->w = clip->w;
        rect->h = clip->h;
        return;
    }

    float sx = clip->w / width;
    float sy = clip->h / height;
    float scale = (position->scale == position_scale_meet) ? otfsvg_min(sx, sy) : otfsvg_max(sx, sy);
    rect->w = width * scale;
    rect->h = height * scale;

    switch(position->align) {
    case position_align_x_mid_y_min:
    case position_align_x_mid_y_mid:
    case position_align_x_mid_y_max:
        rect->x += (clip->w - rect->w) * 0.5f;
        break;
    case position_align_x_max_y_min:
    case position_align_x_max_y_mid:
    case position_align_x_max_y_max:
        rect->x += (clip->w - rect->w);
        break;
    default:
        break;
    }

    switch(position->align) {
    case position_align_x_min_y_mid:
    case position_align_x_mid_y_mid:
    case position_align_x_max_y_mid:
        rect->y += (clip->h - rect->h) * 0.5f;
        break;
    case position_align_x_min_y_max:
    case position_align_x_mid_y_max:
    case position_align_x_max_y_max:
        rect->y += (clip->h - rect->h);
        break;
    default:
        break;
    }
}

static void position_get_matrix(const position_t* position, otfsvg_matrix_t* matrix, const otfsvg_rect_t* viewbox, float width, float height)
{
    otfsvg_matrix_init_identity(matrix);
    if(viewbox->w == 0.0 || viewbox->h == 0.0)
        return;

    float sx = width / viewbox->w;
    float sy = height / viewbox->h;
    if(sx == 0.0 || sy == 0.0)
        return;

    float tx = -viewbox->x;
    float ty = -viewbox->y;
    if(position->align == position_align_none) {
        otfsvg_matrix_scale(matrix, sx, sy);
        otfsvg_matrix_translate(matrix, tx, ty);
        return;
    }

    float scale = (position->scale == position_scale_meet) ? otfsvg_min(sx, sy) : otfsvg_max(sx, sy);
    float vw = width / scale;
    float vh = height / scale;

    switch(position->align) {
    case position_align_x_mid_y_min:
    case position_align_x_mid_y_mid:
    case position_align_x_mid_y_max:
        tx -= (viewbox->w - vw) * 0.5f;
        break;
    case position_align_x_max_y_min:
    case position_align_x_max_y_mid:
    case position_align_x_max_y_max:
        tx -= (viewbox->w - vw);
        break;
    default:
        break;
    }

    switch(position->align) {
    case position_align_x_min_y_mid:
    case position_align_x_mid_y_mid:
    case position_align_x_max_y_mid:
        ty -= (viewbox->h - vh) * 0.5f;
        break;
    case position_align_x_min_y_max:
    case position_align_x_mid_y_max:
    case position_align_x_max_y_max:
        ty -= (viewbox->h - vh);
        break;
    default:
        break;
    }

    otfsvg_matrix_scale(matrix, scale, scale);
    otfsvg_matrix_translate(matrix, tx, ty);
}

static bool parse_line_cap(element_t* element, int id, otfsvg_line_cap_t* linecap)
{
    const string_t* value = property_find(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(skip_string(&it, end, "round"))
        *linecap = otfsvg_line_cap_round;
    else if(skip_string(&it, end, "square"))
        *linecap = otfsvg_line_cap_square;
    else if(skip_string(&it, end, "butt"))
        *linecap = otfsvg_line_cap_butt;
    return !skip_ws(&it, end);
}

static bool parse_line_join(element_t* element, int id, otfsvg_line_join_t* linejoin)
{
    const string_t* value = property_find(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(skip_string(&it, end, "bevel"))
        *linejoin = otfsvg_line_join_bevel;
    else if(skip_string(&it, end, "round"))
        *linejoin = otfsvg_line_join_round;
    else if(skip_string(&it, end, "miter"))
        *linejoin = otfsvg_line_join_miter;
    return !skip_ws(&it, end);
}

static bool parse_winding(element_t* element, int id, otfsvg_fill_rule_t* winding)
{
    const string_t* value = property_find(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(skip_string(&it, end, "evenodd"))
        *winding = otfsvg_fill_rule_even_odd;
    else if(skip_string(&it, end, "nonzero"))
        *winding = otfsvg_fill_rule_non_zero;
    return !skip_ws(&it, end);
}

static bool parse_gradient_spread(element_t* element, int id, otfsvg_gradient_spread_t* spread)
{
    const string_t* value = property_get(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(skip_string(&it, end, "reflect"))
        *spread = otfsvg_gradient_spread_reflect;
    else if(skip_string(&it, end, "repeat"))
        *spread = otfsvg_gradient_spread_repeat;
    else if(skip_string(&it, end, "pad"))
        *spread = otfsvg_gradient_spread_pad;
    return !skip_ws(&it, end);
}

typedef enum {
    display_inline,
    display_none
} display_t;

static bool parse_display(element_t* element, int id, display_t* display)
{
    const string_t* value = property_get(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(skip_string(&it, end, "none"))
        *display = display_none;
    else if(skip_string(&it, end, "inline"))
        *display = display_inline;
    return !skip_ws(&it, end);
}

typedef enum {
    visibility_visible,
    visibility_hidden
} visibility_t;

static bool parse_visibility(element_t* element, int id, visibility_t* visibility)
{
    const string_t* value = property_find(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(skip_string(&it, end, "hidden"))
        *visibility = visibility_hidden;
    else if(skip_string(&it, end, "visible"))
        *visibility = visibility_visible;
    return !skip_ws(&it, end);
}

typedef enum {
    units_type_object_bounding_box,
    units_type_user_space_on_use
} units_type_t;

static bool parse_units(element_t* element, int id, units_type_t* units)
{
    const string_t* value = property_get(element, id);
    if(value == NULL)
        return false;

    const char* it = value->data;
    const char* end = it + value->length;
    if(skip_string(&it, end, "userSpaceOnUse"))
        *units = units_type_user_space_on_use;
    else if(skip_string(&it, end, "objectBoundingBox"))
        *units = units_type_object_bounding_box;
    return !skip_ws(&it, end);
}

typedef enum {
    render_mode_display,
    render_mode_clipping,
    render_mode_bounding
} render_mode_t;

typedef struct {
    element_t* element;
    render_mode_t mode;
    float opacity;
    otfsvg_matrix_t matrix;
    otfsvg_rect_t bbox;
    element_t* clippath;
    bool compositing;
} render_state_t;

static bool document_fill_path(const otfsvg_document_t* document, const render_state_t* state, otfsvg_fill_rule_t winding)
{
    otfsvg_canvas_t* canvas = document->canvas;
    if(canvas && canvas->fill_path)
        return canvas->fill_path(document->canvas_data, &document->path, &state->matrix, winding, &document->paint);
    return false;
}

static bool document_stroke_path(const otfsvg_document_t* document, const render_state_t* state)
{
    otfsvg_canvas_t* canvas = document->canvas;
    if(canvas && canvas->stroke_path)
        return canvas->stroke_path(document->canvas_data, &document->path, &state->matrix, &document->strokedata, &document->paint);
    return false;
}

static bool document_push_group(otfsvg_document_t* document, float opacity, otfsvg_blend_mode_t mode)
{
    otfsvg_canvas_t* canvas = document->canvas;
    if(canvas && canvas->push_group)
        return canvas->push_group(document->canvas_data, opacity, mode);
    return false;
}

static bool document_pop_group(otfsvg_document_t* document, float opacity, otfsvg_blend_mode_t mode)
{
    otfsvg_canvas_t* canvas = document->canvas;
    if(canvas && canvas->pop_group)
        return canvas->pop_group(document->canvas_data, opacity, mode);
    return false;
}

static bool document_decode_image(otfsvg_document_t* document, const string_t* href, otfsvg_image_t* image)
{
    otfsvg_canvas_t* canvas = document->canvas;
    if(canvas && canvas->decode_image)
        return canvas->decode_image(document->canvas_data, href->data, href->length, image);
    return false;
}

static bool document_draw_image(otfsvg_document_t* document, const render_state_t* state, const otfsvg_image_t* image, const otfsvg_rect_t* clip)
{
    otfsvg_canvas_t* canvas = document->canvas;
    if(canvas && canvas->draw_image)
        return canvas->draw_image(document->canvas_data, image, &state->matrix, clip, state->opacity);
    return false;
}

static bool document_get_palette(otfsvg_document_t* document, const string_t* id, otfsvg_color_t* color)
{
    if(document->palette_func == NULL)
        return false;
    return document->palette_func(document->palette_data, id->data, id->length, color);
}

static element_t* resolve_iri(const otfsvg_document_t* document, element_t* element, int id);

static void render_state_begin(otfsvg_document_t* document, render_state_t* state, render_state_t* newstate, otfsvg_blend_mode_t mode)
{
    element_t* element = newstate->element;
    float opacity = 1.f;

    if(newstate->mode == render_mode_display)
        parse_number(element, ID_OPACITY, &opacity, true, false);
    parse_transform(element, ID_TRANSFORM, &newstate->matrix);
    otfsvg_matrix_multiply(&newstate->matrix, &newstate->matrix, &state->matrix);

    newstate->clippath = resolve_iri(document, element, ID_CLIP_PATH);
    newstate->opacity = opacity;
    newstate->bbox.x = 0;
    newstate->bbox.y = 0;
    newstate->bbox.w = 0;
    newstate->bbox.h = 0;
    newstate->compositing = false;
    if(newstate->mode == render_mode_bounding)
        return;
    if(mode == otfsvg_blend_mode_dst_in || newstate->clippath || (opacity < 1.f && element->firstchild)) {
        document_push_group(document, opacity, mode);
        newstate->compositing = true;
    }
}

static void render_clip_path(otfsvg_document_t* document, render_state_t* state, element_t* element);

static void render_state_end(otfsvg_document_t* document, render_state_t* state, render_state_t* newstate, otfsvg_blend_mode_t mode)
{
    if(newstate->clippath)
        render_clip_path(document, newstate, newstate->clippath);
    if(newstate->compositing)
        document_pop_group(document, newstate->opacity, mode);

    otfsvg_matrix_t matrix = state->matrix;
    otfsvg_matrix_invert(&matrix);
    otfsvg_matrix_multiply(&matrix, &newstate->matrix, &matrix);
    otfsvg_matrix_map_rect(&matrix, &newstate->bbox, &newstate->bbox);
    if(mode == otfsvg_blend_mode_dst_in) {
        otfsvg_rect_intersect(&state->bbox, &newstate->bbox);
    } else {
        otfsvg_rect_unite(&state->bbox, &newstate->bbox);
    }
}

static float resolve_length(const otfsvg_document_t* document, const length_t* length, char mode)
{
    if(length->type == length_type_percent) {
        float w = document->width;
        float h = document->width;
        float max = (mode == 'x') ? w : (mode == 'y') ? h : sqrtf(w*w+h*h) / otfsvg_sqrt2;
        return length->value * max / 100.f;
    }

    return convert_length(length, 1.0f, document->dpi);
}

static element_t* element_find(const otfsvg_document_t* document, const string_t* id)
{
    return hashmap_get(document->idcache, id->data, id->length);
}

static element_t* resolve_iri(const otfsvg_document_t* document, element_t* element, int id)
{
    const string_t* value = property_get(element, id);
    if(value && value->length > 1 && value->data[0] == '#') {
        string_t id = {value->data + 1, value->length - 1};
        return element_find(document, &id);
    }

    return NULL;
}

static otfsvg_color_t resolve_color(const otfsvg_document_t* document, const color_t* color, float opacity)
{
    otfsvg_color_t value = color->value;
    if(color->type == color_type_current)
        value = document->current_color;
    uint32_t rgb = value & 0x00FFFFFF;
    uint32_t a = opacity * otfsvg_alpha_channel(value);
    return (rgb | a << 24);
}

static float resolve_gradient_length(const otfsvg_document_t* document, const length_t* length, int units, char mode)
{
    if(units == units_type_object_bounding_box)
        return convert_length(length, 1.f, document->dpi);
    return resolve_length(document, length, mode);
}

static void resolve_gradient_stop(const otfsvg_document_t* document, otfsvg_gradient_t* gradient, float opacity, element_t* element)
{
    float offset = 0;
    float stop_opacity = 1.f;
    color_t stop_color = {color_type_fixed, otfsvg_black_color};

    parse_number(element, ID_OFFSET, &offset, true, false);
    parse_number(element, ID_STOP_OPACITY, &stop_opacity, true, true);
    parse_color(element, ID_STOP_COLOR, &stop_color);

    otfsvg_array_ensure(gradient->stops, 1);
    otfsvg_gradient_stop_t* stop = &gradient->stops.data[gradient->stops.size];
    stop->offset = offset;
    stop->color = resolve_color(document, &stop_color, opacity * stop_opacity);
    gradient->stops.size += 1;
}

static void resolve_gradient_stops(const otfsvg_document_t* document, otfsvg_gradient_t* gradient, float opacity, element_t* element)
{
    otfsvg_array_clear(gradient->stops);
    element_t* child = element->firstchild;
    while(child) {
        if(child->id == TAG_STOP)
            resolve_gradient_stop(document, gradient, opacity, child);
        child = child->nextchild;
    }
}

static void fill_gradient_elements(element_t* current, element_t** elements)
{
    if(elements[0] == NULL) {
        element_t* child = current->firstchild;
        while(child) {
            if(child->id == TAG_STOP) {
                elements[0] = current;
                break;
            }

            child = child->nextchild;
        }
    }

    if(elements[1] == NULL && property_has(current, ID_GRADIENT_TRANSFORM))
        elements[1] = current;
    if(elements[2] == NULL && property_has(current, ID_GRADIENT_UNITS))
        elements[2] = current;
    if(elements[3] == NULL && property_has(current, ID_SPREAD_METHOD))
        elements[3] = current;
}

static bool resolve_linear_gradient(otfsvg_document_t* document, render_state_t* state, element_t* element, float opacity)
{
    element_t* elements[8];
    memset(elements, 0, sizeof(elements));
    element_t* current = element;
    while(true) {
        fill_gradient_elements(current, elements);
        if(current->id == TAG_LINEAR_GRADIENT) {
            if(elements[4] == NULL && property_has(current, ID_X1))
                elements[4] = current;
            if(elements[5] == NULL && property_has(current, ID_Y1))
                elements[5] = current;
            if(elements[6] == NULL && property_has(current, ID_X2))
                elements[6] = current;
            if(elements[7] == NULL && property_has(current, ID_Y2))
                elements[7] = current;
        }

        element_t* ref = resolve_iri(document, current, ID_XLINK_HREF);
        if(ref == NULL || !(ref->id == TAG_LINEAR_GRADIENT || ref->id == TAG_RADIAL_GRADIENT))
            break;
        current = ref;
    }

    if(elements[0] == NULL)
        return false;
    for(int i = 1; i < 8; i++) {
        if(elements[i] == NULL) {
            elements[i] = element;
        }
    }

    otfsvg_paint_t* paint = &document->paint;
    otfsvg_gradient_t* gradient = &paint->gradient;

    paint->type = otfsvg_paint_type_gradient;
    gradient->type = otfsvg_gradient_type_linear;

    otfsvg_matrix_t matrix;
    units_type_t units = units_type_object_bounding_box;
    otfsvg_gradient_spread_t spread = otfsvg_gradient_spread_pad;

    resolve_gradient_stops(document, gradient, opacity, elements[0]);
    parse_transform(elements[1], ID_GRADIENT_TRANSFORM, &matrix);
    parse_units(elements[2], ID_GRADIENT_UNITS, &units);
    parse_gradient_spread(elements[3], ID_SPREAD_METHOD, &spread);
    if(units == units_type_object_bounding_box) {
        otfsvg_matrix_t m;
        otfsvg_matrix_init_translate(&m, state->bbox.x, state->bbox.y);
        otfsvg_matrix_scale(&m, state->bbox.w, state->bbox.h);
        otfsvg_matrix_multiply(&matrix, &matrix, &m);
    }

    gradient->matrix = matrix;
    gradient->spread = spread;

    length_t x1 = {0, length_type_px};
    length_t y1 = {0, length_type_px};
    length_t x2 = {100, length_type_percent};
    length_t y2 = {0, length_type_px};

    parse_length(elements[4], ID_X1, &x1, true, false);
    parse_length(elements[5], ID_Y1, &y1, true, false);
    parse_length(elements[6], ID_X2, &x2, true, false);
    parse_length(elements[7], ID_Y2, &y2, true, false);

    gradient->x1 = resolve_gradient_length(document, &x1, units, 'x');
    gradient->y1 = resolve_gradient_length(document, &y1, units, 'y');
    gradient->x2 = resolve_gradient_length(document, &x2, units, 'x');
    gradient->y2 = resolve_gradient_length(document, &y2, units, 'y');
    return true;
}

static bool resolve_radial_gradient(otfsvg_document_t* document, render_state_t* state, element_t* element, float opacity)
{
    element_t* elements[9];
    memset(elements, 0, sizeof(elements));
    element_t* current = element;
    while(true) {
        fill_gradient_elements(current, elements);
        if(current->id == TAG_RADIAL_GRADIENT) {
            if(elements[4] == NULL && property_has(current, ID_CX))
                elements[4] = current;
            if(elements[5] == NULL && property_has(current, ID_CY))
                elements[5] = current;
            if(elements[6] == NULL && property_has(current, ID_R))
                elements[6] = current;
            if(elements[7] == NULL && property_has(current, ID_FX))
                elements[7] = current;
            if(elements[8] == NULL && property_has(current, ID_FY))
                elements[8] = current;
        }

        element_t* ref = resolve_iri(document, current, ID_XLINK_HREF);
        if(ref == NULL || !(ref->id == TAG_LINEAR_GRADIENT || ref->id == TAG_RADIAL_GRADIENT))
            break;
        current = ref;
    }

    if(elements[7] == NULL) elements[7] = elements[4];
    if(elements[8] == NULL) elements[8] = elements[5];
    if(elements[0] == NULL)
        return false;
    for(int i = 1; i < 9; i++) {
        if(elements[i] == NULL) {
            elements[i] = element;
        }
    }

    otfsvg_paint_t* paint = &document->paint;
    otfsvg_gradient_t* gradient = &paint->gradient;

    paint->type = otfsvg_paint_type_gradient;
    gradient->type = otfsvg_gradient_type_radial;

    otfsvg_matrix_t matrix;
    units_type_t units = units_type_object_bounding_box;
    otfsvg_gradient_spread_t spread = otfsvg_gradient_spread_pad;

    resolve_gradient_stops(document, gradient, opacity, elements[0]);
    parse_transform(elements[1], ID_GRADIENT_TRANSFORM, &matrix);
    parse_units(elements[2], ID_GRADIENT_UNITS, &units);
    parse_gradient_spread(elements[3], ID_SPREAD_METHOD, &spread);
    if(units == units_type_object_bounding_box) {
        otfsvg_matrix_t m;
        otfsvg_matrix_init_translate(&m, state->bbox.x, state->bbox.y);
        otfsvg_matrix_scale(&m, state->bbox.w, state->bbox.h);
        otfsvg_matrix_multiply(&matrix, &matrix, &m);
    }

    gradient->matrix = matrix;
    gradient->spread = spread;

    length_t cx = {50, length_type_percent};
    length_t cy = {50, length_type_percent};
    length_t r = {50, length_type_percent};
    length_t fx = {50, length_type_percent};
    length_t fy = {50, length_type_percent};

    parse_length(elements[4], ID_CX, &cx, true, false);
    parse_length(elements[5], ID_CY, &cy, true, false);
    parse_length(elements[6], ID_R, &r, false, false);
    parse_length(elements[7], ID_FX, &fx, true, false);
    parse_length(elements[8], ID_FY, &fy, true, false);

    gradient->cx = resolve_gradient_length(document, &cx, units, 'x');
    gradient->cy = resolve_gradient_length(document, &cy, units, 'y');
    gradient->r = resolve_gradient_length(document, &r, units, 'o');
    gradient->fx = resolve_gradient_length(document, &fx, units, 'x');
    gradient->fy = resolve_gradient_length(document, &fy, units, 'y');
    return true;
}

static bool resolve_solid_color(otfsvg_document_t* document, element_t* element, float opacity)
{
    float solid_opacity = 1.f;
    color_t solid_color = {color_type_fixed, otfsvg_black_color};

    parse_number(element, ID_SOLID_OPACITY, &solid_opacity, true, true);
    parse_color(element, ID_SOLID_COLOR, &solid_color);

    document->paint.type = otfsvg_paint_type_color;
    document->paint.color = resolve_color(document, &solid_color, opacity * solid_opacity);
    return true;
}

static bool resolve_paint(otfsvg_document_t* document, render_state_t* state, const paint_t* paint, float opacity)
{
    if(paint->type == paint_type_none)
        return false;
    if(paint->type == paint_type_color) {
        document->paint.type = otfsvg_paint_type_color;
        document->paint.color = resolve_color(document, &paint->color, opacity);
        return true;
    }

    if(paint->type == paint_type_var) {
        color_t color = {color_type_fixed, otfsvg_transparent_color};
        if(!document_get_palette(document, &paint->id, &color.value))
            color = paint->color;
        document->paint.type = otfsvg_paint_type_color;
        document->paint.color = resolve_color(document, &color, opacity);;
    }

    element_t* ref = element_find(document, &paint->id);
    if(ref == NULL) {
        document->paint.type = otfsvg_paint_type_color;
        document->paint.color = resolve_color(document, &paint->color, opacity);
        return true;
    }

    if(ref->id == TAG_SOLID_COLOR)
        return resolve_solid_color(document, ref, opacity);
    if(ref->id == TAG_LINEAR_GRADIENT)
        return resolve_linear_gradient(document, state, ref, opacity);
    if(ref->id == TAG_RADIAL_GRADIENT)
        return resolve_radial_gradient(document, state, ref, opacity);
    return false;
}

static bool resolve_fill(otfsvg_document_t* document, render_state_t* state)
{
    element_t* element = state->element;
    paint_t fill = {paint_type_color, {color_type_fixed, otfsvg_black_color}};
    float opacity = 1.f;

    parse_paint(element, ID_FILL, &fill);
    parse_number(element, ID_FILL_OPACITY, &opacity, true, true);
    return resolve_paint(document, state, &fill, opacity * state->opacity);
}

static bool resolve_stroke(otfsvg_document_t* document, render_state_t* state)
{
    element_t* element = state->element;
    paint_t stroke = {paint_type_none, {color_type_fixed, otfsvg_transparent_color}};
    float opacity = 1.f;

    parse_paint(element, ID_STROKE, &stroke);
    parse_number(element, ID_STROKE_OPACITY, &opacity, true, true);
    return resolve_paint(document, state, &stroke, opacity * state->opacity);
}

static void resolve_stroke_data(otfsvg_document_t* document, render_state_t* state)
{
    element_t* element = state->element;
    otfsvg_line_cap_t linecap = otfsvg_line_cap_butt;
    otfsvg_line_join_t linejoin = otfsvg_line_join_miter;

    parse_line_cap(element, ID_STROKE_LINECAP, &linecap);
    parse_line_join(element, ID_STROKE_LINEJOIN, &linejoin);

    float miterlimit = 4;
    length_t linewidth = {1, length_type_number};
    length_t dashoffset = {0, length_type_number};

    parse_number(element, ID_STROKE_MITERLIMIT, &miterlimit, false, true);
    parse_length(element, ID_STROKE_WIDTH, &linewidth, false, true);
    parse_length(element, ID_STROKE_DASHOFFSET, &dashoffset, true, true);

    otfsvg_stroke_data_t* strokedata = &document->strokedata;
    strokedata->linecap = linecap;
    strokedata->linejoin = linejoin;
    strokedata->miterlimit = miterlimit;
    strokedata->linewidth = resolve_length(document, &linewidth, 'o');
    strokedata->dashoffset = resolve_length(document, &dashoffset, 'o');
    strokedata->dasharray.size = 0;
    const string_t* value = property_find(element, ID_STROKE_DASHARRAY);
    if(value == NULL)
        return;
    const char* it = value->data;
    const char* end = it + value->length;
    while(it < end) {
        length_t dash = {0, length_type_unknown};
        if(!parse_length_value(&it, end, &dash, false))
            break;
        otfsvg_array_ensure(strokedata->dasharray, 1);
        float* data = strokedata->dasharray.data;
        data[strokedata->dasharray.size] = resolve_length(document, &dash, 'o');
        strokedata->dasharray.size += 1;
        skip_ws_comma(&it, end);
    }
}

static void document_draw(otfsvg_document_t* document, render_state_t* state)
{
    element_t* element = state->element;
    if(state->mode == render_mode_bounding) {
        paint_t paint = {paint_type_none};
        parse_paint(element, ID_STROKE, &paint);
        if(paint.type == paint_type_none)
            return;
        resolve_stroke_data(document, state);
        otfsvg_stroke_data_t* strokedata = &document->strokedata;
        float caplimit = strokedata->linewidth / 2.f;
        if(strokedata->linecap == otfsvg_line_cap_square)
            caplimit *= otfsvg_sqrt2;

        float joinlimit = strokedata->linewidth / 2.f;
        if(strokedata->linejoin == otfsvg_line_join_miter)
            joinlimit *= strokedata->miterlimit;

        float delta = otfsvg_max(caplimit, joinlimit);
        state->bbox.x -= delta;
        state->bbox.y -= delta;
        state->bbox.w += delta * 2.f;
        state->bbox.h += delta * 2.f;
        return;
    }

    visibility_t visibility = visibility_visible;
    parse_visibility(element, ID_VISIBILITY, &visibility);
    if(visibility == visibility_hidden)
        return;
    if(state->mode == render_mode_clipping) {
        otfsvg_fill_rule_t winding = otfsvg_fill_rule_non_zero;
        parse_winding(element, ID_CLIP_RULE, &winding);

        document->paint.type = otfsvg_paint_type_color;
        document->paint.color = otfsvg_black_color;
        document_fill_path(document, state, winding);
        return;
    }

    if(resolve_fill(document, state)) {
        otfsvg_fill_rule_t winding = otfsvg_fill_rule_non_zero;
        parse_winding(element, ID_FILL_RULE, &winding);
        document_fill_path(document, state, winding);
    }

    if(resolve_stroke(document, state)) {
        resolve_stroke_data(document, state);
        document_stroke_path(document, state);
    }
}

static bool is_display_none(element_t* element)
{
    display_t display = display_inline;
    parse_display(element, ID_DISPLAY, &display);
    return display == display_none;
}

static void render_element(otfsvg_document_t* document, render_state_t* state, element_t* element);
static void render_children(otfsvg_document_t* document, render_state_t* state, element_t* element);

static void render_clip_path(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    units_type_t units = units_type_user_space_on_use;
    parse_units(element, ID_CLIP_PATH_UNITS, &units);

    render_state_t newstate = {element, render_mode_clipping};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_dst_in);

    if(units == units_type_object_bounding_box) {
        otfsvg_matrix_translate(&newstate.matrix, state->bbox.x, state->bbox.y);
        otfsvg_matrix_scale(&newstate.matrix, state->bbox.w, state->bbox.h);
    }

    render_children(document, &newstate, element);
    render_state_end(document, state, &newstate, otfsvg_blend_mode_dst_in);
}

static void render_image(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    if(is_display_none(element))
        return;

    length_t w = {0, length_type_px};
    length_t h = {0, length_type_px};

    parse_length(element, ID_WIDTH, &w, false, false);
    parse_length(element, ID_HEIGHT, &h, false, false);
    if(is_length_zero(w) || is_length_zero(h))
        return;

    length_t x = {0, length_type_px};
    length_t y = {0, length_type_px};

    parse_length(element, ID_X, &x, true, false);
    parse_length(element, ID_Y, &y, true, false);

    float _x = resolve_length(document, &x, 'x');
    float _y = resolve_length(document, &y, 'y');
    float _w = resolve_length(document, &w, 'x');
    float _h = resolve_length(document, &h, 'y');

    const string_t* href = property_get(element, ID_XLINK_HREF);
    if(href == NULL)
        return;

    otfsvg_image_t image;
    if(!document_decode_image(document, href, &image))
        return;

    render_state_t newstate = {element, state->mode};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_src_over);

    newstate.bbox.x = _x;
    newstate.bbox.y = _y;
    newstate.bbox.w = _w;
    newstate.bbox.h = _h;

    position_t position = {position_align_x_mid_y_mid, position_scale_meet};
    parse_position(element, ID_PRESERVE_ASPECT_RATIO, &position);

    otfsvg_rect_t rect;
    otfsvg_rect_t clip = {_x, _y, _w, _h};
    position_get_rect(&position, &rect, &clip, image.width, image.height);

    otfsvg_matrix_translate(&newstate.matrix, rect.x, rect.y);
    otfsvg_matrix_scale(&newstate.matrix, rect.w / image.width, rect.h / image.height);

    document_draw_image(document, &newstate, &image, &clip);
    render_state_end(document, state, &newstate, otfsvg_blend_mode_src_over);
}

static void render_svg(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    if(document->width == 0.f || document->height == 0.f)
        return;
    if(is_display_none(element))
        return;

    length_t x = {0, length_type_px};
    length_t y = {0, length_type_px};

    parse_length(element, ID_X, &x, true, false);
    parse_length(element, ID_Y, &y, true, false);

    render_state_t newstate = {element, state->mode};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_src_over);

    float _x = resolve_length(document, &x, 'x');
    float _y = resolve_length(document, &y, 'y');

    otfsvg_matrix_translate(&newstate.matrix, _x, _y);

    otfsvg_rect_t viewbox;
    if(parse_view_box(element, ID_VIEWBOX, &viewbox)) {
        position_t position = {position_align_x_mid_y_mid, position_scale_meet};
        parse_position(element, ID_PRESERVE_ASPECT_RATIO, &position);

        otfsvg_matrix_t matrix;
        position_get_matrix(&position, &matrix, &viewbox, document->width, document->height);
        otfsvg_matrix_multiply(&newstate.matrix, &matrix, &newstate.matrix);
    }

    render_children(document, &newstate, element);
    render_state_end(document, state, &newstate, otfsvg_blend_mode_src_over);
}

static void render_use(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    if(is_display_none(element))
        return;

    element_t* ref = resolve_iri(document, element, ID_XLINK_HREF);
    if(ref == NULL)
        return;

    length_t x = {0, length_type_px};
    length_t y = {0, length_type_px};

    parse_length(element, ID_X, &x, true, false);
    parse_length(element, ID_Y, &y, true, false);

    render_state_t newstate = {element, state->mode};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_src_over);

    float _x = resolve_length(document, &x, 'x');
    float _y = resolve_length(document, &y, 'y');

    otfsvg_matrix_translate(&newstate.matrix, _x, _y);

    element_t* parent = ref->parent;
    ref->parent = element;
    render_element(document, &newstate, ref);
    ref->parent = parent;

    render_state_end(document, state, &newstate, otfsvg_blend_mode_src_over);
}

static void render_g(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    if(is_display_none(element))
        return;

    render_state_t newstate = {element, state->mode};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_src_over);
    render_children(document, &newstate, element);
    render_state_end(document, state, &newstate, otfsvg_blend_mode_src_over);
}

static void render_line(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    if(is_display_none(element))
        return;

    length_t x1 = {0, length_type_px};
    length_t y1 = {0, length_type_px};
    length_t x2 = {0, length_type_px};
    length_t y2 = {0, length_type_px};

    parse_length(element, ID_X1, &x1, true, false);
    parse_length(element, ID_Y1, &y1, true, false);
    parse_length(element, ID_X2, &x2, true, false);
    parse_length(element, ID_Y2, &y2, true, false);

    float _x1 = resolve_length(document, &x1, 'x');
    float _y1 = resolve_length(document, &y1, 'y');
    float _x2 = resolve_length(document, &x2, 'x');
    float _y2 = resolve_length(document, &y2, 'y');

    render_state_t newstate = {element, state->mode};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_src_over);

    newstate.bbox.x = otfsvg_min(_x1, _x2);
    newstate.bbox.y = otfsvg_min(_y1, _y2);
    newstate.bbox.w = fabsf(_x2 - _x1);
    newstate.bbox.h = fabsf(_y2 - _y1);

    otfsvg_path_t* path = &document->path;
    otfsvg_path_clear(path);
    otfsvg_path_move_to(path, _x1, _y1);
    otfsvg_path_line_to(path, _x2, _y2);

    document_draw(document, &newstate);
    render_state_end(document, state, &newstate, otfsvg_blend_mode_src_over);
}

static void render_polyline(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    if(is_display_none(element))
        return;

    otfsvg_path_t* path = &document->path;
    otfsvg_path_clear(path);
    parse_points(element, ID_POINTS, path);
    if(path->commands.size == 0)
        return;

    render_state_t newstate = {element, state->mode};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_src_over);

    otfsvg_path_bounding_box(path, &newstate.bbox);

    document_draw(document, &newstate);
    render_state_end(document, state, &newstate, otfsvg_blend_mode_src_over);
}

static void render_polygon(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    if(is_display_none(element))
        return;

    otfsvg_path_t* path = &document->path;
    otfsvg_path_clear(path);
    parse_points(element, ID_POINTS, path);
    otfsvg_path_close(path);
    if(path->commands.size == 0)
        return;

    render_state_t newstate = {element, state->mode};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_src_over);

    otfsvg_path_bounding_box(path, &newstate.bbox);

    document_draw(document, &newstate);
    render_state_end(document, state, &newstate, otfsvg_blend_mode_src_over);
}

static void render_path(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    if(is_display_none(element))
        return;

    otfsvg_path_t* path = &document->path;
    otfsvg_path_clear(path);
    parse_path(element, ID_D, path);
    if(path->commands.size == 0)
        return;

    render_state_t newstate = {element, state->mode};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_src_over);

    otfsvg_path_bounding_box(path, &newstate.bbox);

    document_draw(document, &newstate);
    render_state_end(document, state, &newstate, otfsvg_blend_mode_src_over);
}

static void render_ellipse(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    if(is_display_none(element))
        return;

    length_t rx = {0, length_type_px};
    length_t ry = {0, length_type_px};

    parse_length(element, ID_RX, &rx, false, false);
    parse_length(element, ID_RY, &ry, false, false);

    if(is_length_zero(rx) || is_length_zero(ry))
        return;

    length_t cx = {0, length_type_px};
    length_t cy = {0, length_type_px};

    parse_length(element, ID_CX, &cx, true, false);
    parse_length(element, ID_CY, &cy, true, false);

    float _cx = resolve_length(document, &cx, 'x');
    float _cy = resolve_length(document, &cy, 'y');
    float _rx = resolve_length(document, &rx, 'x');
    float _ry = resolve_length(document, &ry, 'y');

    render_state_t newstate = {element, state->mode};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_src_over);

    newstate.bbox.x = _cx - _rx;
    newstate.bbox.y = _cy - _ry;
    newstate.bbox.w = _rx + _rx;
    newstate.bbox.h = _ry + _ry;

    otfsvg_path_t* path = &document->path;
    otfsvg_path_clear(path);
    otfsvg_path_add_ellipse(path, _cx, _cy, _rx, _ry);
    document_draw(document, &newstate);

    render_state_end(document, state, &newstate, otfsvg_blend_mode_src_over);
}

static void render_circle(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    if(is_display_none(element))
        return;

    length_t r = {0, length_type_px};
    parse_length(element, ID_R, &r, false, false);
    if(is_length_zero(r))
        return;

    length_t cx = {0, length_type_px};
    length_t cy = {0, length_type_px};

    parse_length(element, ID_CX, &cx, true, false);
    parse_length(element, ID_CY, &cy, true, false);

    float _cx = resolve_length(document, &cx, 'x');
    float _cy = resolve_length(document, &cy, 'y');
    float _r = resolve_length(document, &r, 'o');

    render_state_t newstate = {element, state->mode};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_src_over);

    newstate.bbox.x = _cx - _r;
    newstate.bbox.y = _cy - _r;
    newstate.bbox.w = _r + _r;
    newstate.bbox.h = _r + _r;

    otfsvg_path_t* path = &document->path;
    otfsvg_path_clear(path);
    otfsvg_path_add_ellipse(path, _cx, _cy, _r, _r);
    document_draw(document, &newstate);

    render_state_end(document, state, &newstate, otfsvg_blend_mode_src_over);
}

static void render_rect(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    if(is_display_none(element))
        return;

    length_t w = {0, length_type_px};
    length_t h = {0, length_type_px};

    parse_length(element, ID_WIDTH, &w, false, false);
    parse_length(element, ID_HEIGHT, &h, false, false);

    if(is_length_zero(w) || is_length_zero(h))
        return;

    length_t x = {0, length_type_px};
    length_t y = {0, length_type_px};

    parse_length(element, ID_X, &x, true, false);
    parse_length(element, ID_Y, &y, true, false);

    float _x = resolve_length(document, &x, 'x');
    float _y = resolve_length(document, &y, 'y');
    float _w = resolve_length(document, &w, 'x');
    float _h = resolve_length(document, &h, 'y');

    length_t rx = {0, length_type_unknown};
    length_t ry = {0, length_type_unknown};

    parse_length(element, ID_RX, &rx, false, false);
    parse_length(element, ID_RY, &ry, false, false);

    float _rx = resolve_length(document, &rx, 'x');
    float _ry = resolve_length(document, &ry, 'y');

    if(!is_length_valid(rx)) _rx = _ry;
    if(!is_length_valid(ry)) _ry = _rx;

    render_state_t newstate = {element, state->mode};
    render_state_begin(document, state, &newstate, otfsvg_blend_mode_src_over);

    newstate.bbox.x = _x;
    newstate.bbox.y = _y;
    newstate.bbox.w = _w;
    newstate.bbox.h = _h;

    otfsvg_path_t* path = &document->path;
    otfsvg_path_clear(path);
    otfsvg_path_add_round_rect(path, _x, _y, _w, _h, _rx, _ry);
    document_draw(document, &newstate);

    render_state_end(document, state, &newstate, otfsvg_blend_mode_src_over);
}

static void render_element(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    switch(element->id) {
    case TAG_USE:
        render_use(document, state, element);
        break;
    case TAG_G:
        render_g(document, state, element);
        break;
    case TAG_LINE:
        render_line(document, state, element);
        break;
    case TAG_POLYLINE:
        render_polyline(document, state, element);
        break;
    case TAG_POLYGON:
        render_polygon(document, state, element);
        break;
    case TAG_PATH:
        render_path(document, state, element);
        break;
    case TAG_ELLIPSE:
        render_ellipse(document, state, element);
        break;
    case TAG_CIRCLE:
        render_circle(document, state, element);
        break;
    case TAG_RECT:
        render_rect(document, state, element);
        break;
    }
}

static void render_children(otfsvg_document_t* document, render_state_t* state, element_t* element)
{
    element_t* child = element->firstchild;
    while(child) {
        render_element(document, state, child);
        child = child->nextchild;
    }
}

otfsvg_document_t* otfsvg_document_create(void)
{
    otfsvg_document_t* document = malloc(sizeof(otfsvg_document_t));
    otfsvg_array_init(document->path.commands);
    otfsvg_array_init(document->path.points);
    otfsvg_array_init(document->paint.gradient.stops);
    otfsvg_array_init(document->strokedata.dasharray);
    document->idcache = hashmap_create();
    document->heap = heap_create();
    document->root = NULL;
    document->width = 0.f;
    document->height = 0.f;
    document->dpi = 96.f;
    document->canvas = NULL;
    return document;
}

void otfsvg_document_destory(otfsvg_document_t* document)
{
    otfsvg_array_destroy(document->path.commands);
    otfsvg_array_destroy(document->path.points);
    otfsvg_array_destroy(document->paint.gradient.stops);
    otfsvg_array_destroy(document->strokedata.dasharray);
    hashmap_destroy(document->idcache);
    heap_destroy(document->heap);
    free(document);
}

void otfsvg_document_clear(otfsvg_document_t* document)
{
    hashmap_clear(document->idcache);
    heap_clear(document->heap);
    document->root = NULL;
    document->width = 0.f;
    document->height = 0.f;
}

static bool parse_attributes(const char** begin, const char* end, otfsvg_document_t* document, element_t* element)
{
    const char* it = *begin;
    while(it < end && IS_STARTNAMECHAR(*it)) {
        const char* begin = it;
        ++it;
        while(it < end && IS_NAMECHAR(*it))
            ++it;

        int id = propertyid(begin, it - begin);
        skip_ws(&it, end);
        if(it >= end || *it != '=')
            return false;

        ++it;
        skip_ws(&it, end);
        if(it >= end || (*it != '"' && *it != '\''))
            return false;

        const char quote = *it;
        ++it;
        skip_ws(&it, end);
        begin = it;
        while(it < end && *it != quote)
            ++it;
        if(it >= end || *it != quote)
            return false;
        if(id && element) {
            if(id == ID_ID) {
                hashmap_put(document->idcache, document->heap, begin, it - begin, element);
            } else {
                property_t* property = heap_alloc(document->heap, sizeof(property_t));
                property->id = id;
                property->value.data = begin;
                property->value.length = it - begin;
                property->next = element->property;
                element->property = property;
            }
        }

        ++it;
        skip_ws(&it, end);
    }

    *begin = it;
    return true;
}

bool otfsvg_document_load(otfsvg_document_t* document, const char* data, size_t length, float width, float height, float dpi)
{
    otfsvg_document_clear(document);
    const char* it = data;
    const char* end = it + length;

    element_t* current = NULL;
    int ignoring = 0;
    while(it < end) {
        while(it < end && *it != '<')
            ++it;

        if(it >= end || *it != '<')
            break;

        ++it;
        if(it < end && *it == '/') {
            ++it;
            if(it >= end || !IS_STARTNAMECHAR(*it))
                break;

            ++it;
            while(it < end && IS_NAMECHAR(*it))
                ++it;

            skip_ws(&it, end);
            if(it >= end || *it != '>')
                break;

            if(ignoring > 0)
                --ignoring;
            else if(current && current->parent)
                current = current->parent;

            ++it;
            continue;
        }

        if(it < end && *it == '?') {
            ++it;
            if(!skip_string(&it, end, "xml"))
                break;

            skip_ws(&it, end);
            if(!parse_attributes(&it, end, document, NULL))
                break;

            if(!skip_string(&it, end, "?>"))
                break;

            skip_ws(&it, end);
            continue;
        }

        if(it < end && *it == '!') {
            ++it;
            if(skip_string(&it, end, "--")) {
                const char* begin = string_find(it, end, "-->");
                if(begin == NULL)
                    break;
                it = begin + 3;
                skip_ws(&it, end);
                continue;
            }

            if(skip_string(&it, end, "[CDATA[")) {
                const char* begin = string_find(it, end, "]]>");
                if(begin == NULL)
                    break;
                it = begin + 3;
                skip_ws(&it, end);
                continue;
            }

            if(skip_string(&it, end, "DOCTYPE")) {
                while(it < end && *it != '>') {
                    if(*it == '[') {
                        ++it;
                        int depth = 1;
                        while(it < end && depth > 0) {
                            if(*it == '[') ++depth;
                            else if(*it == ']') --depth;
                            ++it;
                        }
                    } else {
                        ++it;
                    }
                }

                if(it >= end || *it != '>')
                    break;
                it += 1;
                skip_ws(&it, end);
                continue;
            }

            break;
        }

        if(it >= end || !IS_STARTNAMECHAR(*it))
            break;

        const char* begin = it;
        ++it;
        while(it < end && IS_NAMECHAR(*it))
            ++it;

        element_t* element = NULL;
        if(ignoring > 0) {
            ++ignoring;
        } else {
            int id = elementid(begin, it - begin);
            if(id == TAG_UNKNOWN) {
                ignoring = 1;
            } else {
                if(document->root && current == NULL)
                    break;
                element = heap_alloc(document->heap, sizeof(element_t));
                element->id = id;
                element->parent = NULL;
                element->nextchild = NULL;
                element->firstchild = NULL;
                element->lastchild = NULL;
                element->property = NULL;
                if(document->root == NULL) {
                    if(element->id != TAG_SVG)
                        break;
                    document->root = element;
                } else {
                    element->parent = current;
                    if(current->lastchild) {
                        current->lastchild->nextchild = element;
                        current->lastchild = element;
                    } else {
                        current->lastchild = element;
                        current->firstchild = element;
                    }
                }
            }
        }

        skip_ws(&it, end);
        if(!parse_attributes(&it, end, document, element))
            break;

        if(it < end && *it == '>') {
            if(element)
                current = element;
            ++it;
            continue;
        }

        if(it < end && *it == '/') {
            ++it;
            if(it >= end || *it != '>')
                break;

            if(ignoring > 0)
                --ignoring;
            ++it;
            continue;
        }

        break;
    }

    skip_ws(&it, end);
    if(document->root == NULL || it != end || ignoring != 0) {
        otfsvg_document_clear(document);
        return false;
    }

    otfsvg_rect_t viewbox;
    if(parse_view_box(document->root, ID_VIEWBOX, &viewbox)) {
        document->width = viewbox.w;
        document->height = viewbox.h;
        document->dpi = dpi;
    } else {
        length_t w = {100, length_type_percent};
        length_t h = {100, length_type_percent};

        parse_length(document->root, ID_WIDTH, &w, false, false);
        parse_length(document->root, ID_HEIGHT, &h, false, false);

        document->width = convert_length(&w, width, dpi);
        document->height = convert_length(&h, height, dpi);
        document->dpi = dpi;
    }

    return true;
}

float otfsvg_document_width(const otfsvg_document_t* document)
{
    return document->width;
}

float otfsvg_document_height(const otfsvg_document_t* document)
{
    return document->height;
}

bool otfsvg_document_render(otfsvg_document_t* document, otfsvg_canvas_t* canvas, void* canvas_data, otfsvg_palette_func_t palette_func, void* palette_data, otfsvg_color_t current_color, const char* id)
{
    if(document->root == NULL)
        return false;
    document->canvas = canvas;
    document->canvas_data = canvas_data;
    document->palette_func = palette_func;
    document->palette_data = palette_data;
    document->current_color = current_color;

    render_state_t state;
    state.mode = render_mode_display;
    otfsvg_rect_init(&state.bbox, 0, 0, 0, 0);
    otfsvg_matrix_init_identity(&state.matrix);
    if(id == NULL) {
        state.element = document->root;
        render_svg(document, &state, state.element);
    } else {
        string_t name = {id, strlen(id)};
        element_t* element = element_find(document, &name);
        if(element == NULL)
            return false;
        state.element = element;
        render_element(document, &state, state.element);
    }

    return true;
}

bool otfsvg_document_rect(otfsvg_document_t* document, otfsvg_rect_t* rect, const char* id)
{
    if(document->root == NULL)
        return false;
    document->canvas = NULL;
    document->canvas_data = NULL;
    document->palette_func = NULL;
    document->palette_data = NULL;
    document->current_color = otfsvg_black_color;

    render_state_t state;
    state.mode = render_mode_bounding;
    otfsvg_rect_init(&state.bbox, 0, 0, 0, 0);
    otfsvg_matrix_init_identity(&state.matrix);
    if(id == NULL) {
        state.element = document->root;
        render_svg(document, &state, state.element);
    } else {
        string_t name = {id, strlen(id)};
        element_t* element = element_find(document, &name);
        if(element == NULL)
            return false;
        state.element = element;
        render_element(document, &state, state.element);
    }

    rect->x = state.bbox.x;
    rect->y = state.bbox.y;
    rect->w = state.bbox.w;
    rect->h = state.bbox.h;
    return true;
}
