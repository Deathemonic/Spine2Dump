#include "render_canvas.h"

#include <stdlib.h>
#include <string.h>

void rgba_image_free(RgbaImage* image) {
    if (image == NULL) {
        return;
    }
    free(image->pixels);
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
}

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

RenderCropRect render_canvas_alpha_bounds(const RgbaImage* image,
                                          unsigned char alpha_threshold,
                                          int padding) {
    RenderCropRect empty = {};
    if (image == NULL || image->pixels == NULL || image->width <= 0 || image->height <= 0) {
        return empty;
    }

    int min_x = image->width;
    int min_y = image->height;
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            unsigned char alpha = image->pixels[((y * image->width + x) * 4) + 3];
            if (alpha < alpha_threshold) {
                continue;
            }
            if (x < min_x) {
                min_x = x;
            }
            if (y < min_y) {
                min_y = y;
            }
            if (x > max_x) {
                max_x = x;
            }
            if (y > max_y) {
                max_y = y;
            }
        }
    }

    if (max_x < min_x || max_y < min_y) {
        return empty;
    }

    padding = padding < 0 ? 0 : padding;
    min_x = clamp_int(min_x - padding, 0, image->width - 1);
    min_y = clamp_int(min_y - padding, 0, image->height - 1);
    max_x = clamp_int(max_x + padding, 0, image->width - 1);
    max_y = clamp_int(max_y + padding, 0, image->height - 1);

    RenderCropRect crop = {
        .x = min_x,
        .y = min_y,
        .width = max_x - min_x + 1,
        .height = max_y - min_y + 1,
        .valid = 1,
    };
    return crop;
}

RenderCropRect render_crop_union(RenderCropRect a, RenderCropRect b) {
    if (!a.valid) {
        return b;
    }
    if (!b.valid) {
        return a;
    }

    int min_x = a.x < b.x ? a.x : b.x;
    int min_y = a.y < b.y ? a.y : b.y;
    int max_x = (a.x + a.width) > (b.x + b.width) ? (a.x + a.width) : (b.x + b.width);
    int max_y = (a.y + a.height) > (b.y + b.height) ? (a.y + a.height) : (b.y + b.height);
    RenderCropRect crop = {
        .x = min_x,
        .y = min_y,
        .width = max_x - min_x,
        .height = max_y - min_y,
        .valid = 1,
    };
    return crop;
}

int render_canvas_crop(const RgbaImage* source, RenderCropRect crop, RgbaImage* out) {
    if (source == NULL || source->pixels == NULL || out == NULL || !crop.valid || crop.x < 0 ||
        crop.y < 0 || crop.width <= 0 || crop.height <= 0 || crop.x + crop.width > source->width ||
        crop.y + crop.height > source->height) {
        return -1;
    }

    unsigned char* pixels = malloc((size_t)crop.width * (size_t)crop.height * 4);
    if (pixels == NULL) {
        return -1;
    }

    for (int y = 0; y < crop.height; y++) {
        const unsigned char* src_row = source->pixels +
                                       (((crop.y + y) * source->width + crop.x) * 4);
        unsigned char* dst_row = pixels + ((size_t)y * (size_t)crop.width * 4);
        memcpy(dst_row, src_row, (size_t)crop.width * 4);
    }

    out->pixels = pixels;
    out->width = crop.width;
    out->height = crop.height;
    return 0;
}

const RgbaImage* render_canvas_select_output(const RgbaImage* source,
                                             const RenderOptions* options,
                                             const RenderCropRect* forced_crop,
                                             RgbaImage* cropped) {
    if (source == NULL || options == NULL || cropped == NULL) {
        return source;
    }

    RenderCropRect crop = {};
    if (options->crop.valid) {
        crop = options->crop;
    } else if (forced_crop != NULL && forced_crop->valid) {
        crop = *forced_crop;
    } else if (options->trim) {
        crop = render_canvas_alpha_bounds(source, options->alpha_threshold, options->trim_padding);
    }

    if (!crop.valid || render_canvas_crop(source, crop, cropped) != 0) {
        return source;
    }
    return cropped;
}
