#include "cpu_renderer.h"

#include <stdlib.h>
#include <string.h>

#include <spine/Atlas.h>
#include <zf_log/zf_log.h>

#include "image_io.h"
#include "path.h"
#include "render_canvas.h"
#include "software_rasterizer.h"
#include "spine_slot_walk.h"

static void free_pages(LoadedPage* pages, int count) {
    for (int i = 0; i < count; i++) {
        free(pages[i].name);
        free(pages[i].pixels);
    }
    free(pages);
}

static char* copy_text(const char* text) {
    size_t length = strlen(text);
    char* copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

static int load_pages(spAtlas* atlas,
                      const char* atlas_dir,
                      LoadedPage** pages_out,
                      int* count_out) {
    int count = 0;
    for (spAtlasPage* page = atlas->pages; page != NULL; page = page->next) {
        count++;
    }

    LoadedPage* pages = calloc((size_t)count, sizeof(*pages));
    if (pages == NULL) {
        return -1;
    }

    int index = 0;
    for (spAtlasPage* page = atlas->pages; page != NULL; page = page->next) {
        char image_path[1024];
        const char* load_path = page->name;
        if (atlas_dir != NULL && atlas_dir[0] != '\0' &&
            path_join(atlas_dir, page->name, image_path, sizeof(image_path)) == 0) {
            load_path = image_path;
        }

        pages[index].name = copy_text(page->name);
        unsigned width = 0;
        unsigned height = 0;
        int error = image_decode_png32_file(&pages[index].pixels, &width, &height, load_path);
        pages[index].width = (int)width;
        pages[index].height = (int)height;
        pages[index].channels = 4;
        if (pages[index].name == NULL || error != 0 || pages[index].pixels == NULL) {
            ZF_LOGE("Could not load atlas page PNG: %s", load_path);
            free_pages(pages, count);
            return -1;
        }
        index++;
    }

    *pages_out = pages;
    *count_out = count;
    return 0;
}

CpuAtlasPages* cpu_atlas_pages_load(spAtlas* atlas, const char* atlas_dir) {
    if (atlas == NULL) {
        return NULL;
    }
    CpuAtlasPages* cache = calloc(1, sizeof(*cache));
    if (cache == NULL) {
        return NULL;
    }
    if (load_pages(atlas, atlas_dir, &cache->items, &cache->count) != 0) {
        free(cache);
        return NULL;
    }
    return cache;
}

void cpu_atlas_pages_free(CpuAtlasPages* pages) {
    if (pages == NULL) {
        return;
    }
    free_pages(pages->items, pages->count);
    free(pages);
}

typedef struct {
    RgbaImage* canvas;
    const LoadedPage* pages;
} CpuSinkContext;

static void cpu_sink(const SlotWalkTriangle* triangle, void* user) {
    CpuSinkContext* context = user;
    const LoadedPage* page = &context->pages[triangle->page_index];
    RasterTriangleRequest request = {
        .target = context->canvas,
        .texture =
            {
                      .pixels = page->pixels,
                      .width = page->width,
                      .height = page->height,
                      },
        .vertices = triangle->vertices,
        .uvs = triangle->uvs,
        .triangle = triangle->triangle,
        .transform = triangle->transform,
        .shade = triangle->shade,
    };
    software_rasterizer_draw_triangle(&request);
}

int cpu_renderer_render_image(const CpuRenderRequest* request, RgbaImage* out) {
    if (request == NULL || request->skeleton == NULL || request->atlas == NULL ||
        request->options == NULL || out == NULL) {
        return -1;
    }

    int width = request->options->width;
    int height = request->options->height;
    if (width <= 0 || height <= 0 || request->options->scale <= 0.0f) {
        return -1;
    }

    CpuAtlasPages* owned_pages = NULL;
    const CpuAtlasPages* pages = request->pages;
    if (pages == NULL) {
        owned_pages = cpu_atlas_pages_load(request->atlas, request->atlas_dir);
        if (owned_pages == NULL) {
            return -1;
        }
        pages = owned_pages;
    }

    RgbaImage canvas = {
        .pixels = calloc((size_t)width * (size_t)height * 4, 1),
        .width = width,
        .height = height,
    };
    if (canvas.pixels == NULL) {
        cpu_atlas_pages_free(owned_pages);
        return -1;
    }

    CpuSinkContext context = {
        .canvas = &canvas,
        .pages = pages->items,
    };
    SlotWalkRequest walk = {
        .skeleton = request->skeleton,
        .atlas = request->atlas,
        .pages = pages,
        .options = request->options,
    };
    int walk_result = spine_slot_walk(&walk, cpu_sink, &context);
    cpu_atlas_pages_free(owned_pages);
    if (walk_result != 0) {
        rgba_image_free(&canvas);
        return -1;
    }

    *out = canvas;
    return 0;
}

int cpu_renderer_render_png(const CpuRenderPngRequest* request) {
    if (request == NULL || request->output_path == NULL) {
        return -1;
    }

    RgbaImage canvas = {};
    if (cpu_renderer_render_image(&request->render, &canvas) != 0) {
        return -1;
    }

    RgbaImage cropped = {};
    const RgbaImage* output = render_canvas_select_output(&canvas, request->render.options,
                                                          request->forced_crop, &cropped);
    PngEncodeOptions encode_options = png_encode_options_for(
        request->render.options->png_compression);
    int png_error = image_encode_png32_file(request->output_path, output->pixels,
                                            (unsigned)output->width, (unsigned)output->height,
                                            &encode_options);

    rgba_image_free(&cropped);
    rgba_image_free(&canvas);

    if (png_error != 0) {
        ZF_LOGE("Could not write PNG: %s", request->output_path);
        return -1;
    }

    return 0;
}
