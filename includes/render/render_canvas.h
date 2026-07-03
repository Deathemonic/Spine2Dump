#ifndef SPINE2DUMP_RENDER_CANVAS_H
#define SPINE2DUMP_RENDER_CANVAS_H

#include "render_options.h"

typedef struct RgbaImage {
    unsigned char* pixels;
    int width;
    int height;
} RgbaImage;

typedef struct RenderCropRect {
    int x;
    int y;
    int width;
    int height;
    int valid;
} RenderCropRect;

void rgba_image_free(RgbaImage* image);
RenderCropRect render_canvas_alpha_bounds(const RgbaImage* image,
                                          unsigned char alpha_threshold,
                                          int padding);
RenderCropRect render_crop_union(RenderCropRect a, RenderCropRect b);
int render_canvas_crop(const RgbaImage* source, RenderCropRect crop, RgbaImage* out);
const RgbaImage* render_canvas_select_output(const RgbaImage* source,
                                             const RenderOptions* options,
                                             const RenderCropRect* forced_crop,
                                             RgbaImage* cropped);

#endif
