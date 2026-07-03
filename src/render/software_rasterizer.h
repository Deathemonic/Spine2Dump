#ifndef SPINE2DUMP_SOFTWARE_RASTERIZER_H
#define SPINE2DUMP_SOFTWARE_RASTERIZER_H

#include "render_canvas.h"

typedef struct RasterTexture {
    const unsigned char* pixels;
    int width;
    int height;
} RasterTexture;

typedef struct RasterTransform {
    float min_x;
    float min_y;
    float scale;
} RasterTransform;

typedef struct RasterTriangle {
    int a;
    int b;
    int c;
} RasterTriangle;

typedef struct RasterShade {
    float color[4];
    int blend_mode;
} RasterShade;

typedef struct RasterTriangleRequest {
    RgbaImage* target;
    RasterTexture texture;
    const float* vertices;
    const float* uvs;
    RasterTriangle triangle;
    RasterTransform transform;
    RasterShade shade;
} RasterTriangleRequest;

void software_rasterizer_draw_triangle(const RasterTriangleRequest* request);

#endif
