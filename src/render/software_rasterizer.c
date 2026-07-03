#include "software_rasterizer.h"

#include <math.h>
#include <string.h>

enum {
    RASTER_BLEND_MODE_NORMAL = 0,
    RASTER_BLEND_MODE_ADDITIVE = 1,
    RASTER_BLEND_MODE_MULTIPLY = 2,
    RASTER_BLEND_MODE_SCREEN = 3,
};

static float edge(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static float clamp01(float value) {
    return fminf(fmaxf(value, 0.0f), 1.0f);
}

static void blend_pixel(unsigned char* dst, const float* src, int blend_mode) {
    float sa = src[3] / 255.0f;
    float da = dst[3] / 255.0f;
    float sr = src[0] / 255.0f;
    float sg = src[1] / 255.0f;
    float sb = src[2] / 255.0f;
    float dr = dst[0] / 255.0f;
    float dg = dst[1] / 255.0f;
    float db = dst[2] / 255.0f;

    if (blend_mode == RASTER_BLEND_MODE_ADDITIVE) {
        dst[0] = (unsigned char)(clamp01(dr + sr * sa) * 255.0f);
        dst[1] = (unsigned char)(clamp01(dg + sg * sa) * 255.0f);
        dst[2] = (unsigned char)(clamp01(db + sb * sa) * 255.0f);
        dst[3] = (unsigned char)(clamp01(da + sa) * 255.0f);
        return;
    }

    if (blend_mode == RASTER_BLEND_MODE_MULTIPLY) {
        sr = sr * dr;
        sg = sg * dg;
        sb = sb * db;
    } else if (blend_mode == RASTER_BLEND_MODE_SCREEN) {
        sr = 1.0f - (1.0f - sr) * (1.0f - dr);
        sg = 1.0f - (1.0f - sg) * (1.0f - dg);
        sb = 1.0f - (1.0f - sb) * (1.0f - db);
    }

    float out_a = sa + da * (1.0f - sa);
    if (out_a <= 0.0f) {
        memset(dst, 0, 4);
        return;
    }

    dst[0] = (unsigned char)(clamp01((sr * sa + dr * da * (1.0f - sa)) / out_a) * 255.0f);
    dst[1] = (unsigned char)(clamp01((sg * sa + dg * da * (1.0f - sa)) / out_a) * 255.0f);
    dst[2] = (unsigned char)(clamp01((sb * sa + db * da * (1.0f - sa)) / out_a) * 255.0f);
    dst[3] = (unsigned char)fminf(fmaxf(out_a * 255.0f, 0.0f), 255.0f);
}

void software_rasterizer_draw_triangle(const RasterTriangleRequest* request) {
    if (request == NULL || request->target == NULL || request->target->pixels == NULL ||
        request->texture.pixels == NULL || request->vertices == NULL || request->uvs == NULL) {
        return;
    }

    RgbaImage* target = request->target;
    const RasterTexture* texture = &request->texture;
    const RasterTriangle* triangle = &request->triangle;
    const RasterTransform* transform = &request->transform;

    float ax = (request->vertices[triangle->a * 2] - transform->min_x) * transform->scale;
    float ay = (request->vertices[triangle->a * 2 + 1] - transform->min_y) * transform->scale;
    float bx = (request->vertices[triangle->b * 2] - transform->min_x) * transform->scale;
    float by = (request->vertices[triangle->b * 2 + 1] - transform->min_y) * transform->scale;
    float cx = (request->vertices[triangle->c * 2] - transform->min_x) * transform->scale;
    float cy = (request->vertices[triangle->c * 2 + 1] - transform->min_y) * transform->scale;

    ay = (float)(target->height - 1) - ay;
    by = (float)(target->height - 1) - by;
    cy = (float)(target->height - 1) - cy;

    float min_px = floorf(fminf(ax, fminf(bx, cx)));
    float max_px = ceilf(fmaxf(ax, fmaxf(bx, cx)));
    float min_py = floorf(fminf(ay, fminf(by, cy)));
    float max_py = ceilf(fmaxf(ay, fmaxf(by, cy)));

    int x0 = (int)fmaxf(min_px, 0.0f);
    int x1 = (int)fminf(max_px, (float)(target->width - 1));
    int y0 = (int)fmaxf(min_py, 0.0f);
    int y1 = (int)fminf(max_py, (float)(target->height - 1));

    float area = edge(ax, ay, bx, by, cx, cy);
    if (fabsf(area) < 0.0001f) {
        return;
    }

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;
            float w0 = edge(bx, by, cx, cy, px, py) / area;
            float w1 = edge(cx, cy, ax, ay, px, py) / area;
            float w2 = edge(ax, ay, bx, by, px, py) / area;
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                continue;
            }

            float u = request->uvs[triangle->a * 2] * w0 + request->uvs[triangle->b * 2] * w1 +
                      request->uvs[triangle->c * 2] * w2;
            float v = request->uvs[triangle->a * 2 + 1] * w0 +
                      request->uvs[triangle->b * 2 + 1] * w1 +
                      request->uvs[triangle->c * 2 + 1] * w2;
            int sx = (int)fminf(fmaxf(u * (float)texture->width, 0.0f),
                                (float)(texture->width - 1));
            int sy = (int)fminf(fmaxf(v * (float)texture->height, 0.0f),
                                (float)(texture->height - 1));

            const unsigned char* src = texture->pixels + ((sy * texture->width + sx) * 4);
            unsigned char* dst = target->pixels + ((y * target->width + x) * 4);
            float tinted[4] = {
                src[0] * request->shade.color[0],
                src[1] * request->shade.color[1],
                src[2] * request->shade.color[2],
                src[3] * request->shade.color[3],
            };
            blend_pixel(dst, tinted, request->shade.blend_mode);
        }
    }
}
