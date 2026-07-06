#include "cpu_renderer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spine/Attachment.h>
#include <spine/MeshAttachment.h>
#include <spine/RegionAttachment.h>
#include <spine/SkeletonClipping.h>
#include <spine/SlotData.h>
#include <spine/VertexAttachment.h>
#include <zf_log/zf_log.h>

#include "image_io.h"
#include "path.h"
#include "render_canvas.h"
#include "software_rasterizer.h"
#include "spine_compat.h"

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
            ZF_LOGE("could not load atlas page PNG: %s", load_path);
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

static const LoadedPage* find_page(const LoadedPage* pages, int count, const char* name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(pages[i].name, name) == 0) {
            return &pages[i];
        }
    }
    return NULL;
}

static void attachment_color(const spSkeleton* skeleton,
                             const spSlot* slot,
                             const spAttachment* attachment,
                             float* color) {
    color[0] = skeleton->color.r * slot->color.r;
    color[1] = skeleton->color.g * slot->color.g;
    color[2] = skeleton->color.b * slot->color.b;
    color[3] = skeleton->color.a * slot->color.a;

    if (attachment->type == SP_ATTACHMENT_REGION) {
        const spRegionAttachment* region = (const spRegionAttachment*)attachment;
        color[0] *= region->color.r;
        color[1] *= region->color.g;
        color[2] *= region->color.b;
        color[3] *= region->color.a;
    } else if (attachment->type == SP_ATTACHMENT_MESH) {
        const spMeshAttachment* mesh = (const spMeshAttachment*)attachment;
        color[0] *= mesh->color.r;
        color[1] *= mesh->color.g;
        color[2] *= mesh->color.b;
        color[3] *= mesh->color.a;
    }
}

static RasterShade raster_shade_from_attachment(const spSkeleton* skeleton,
                                                const spSlot* slot,
                                                const spAttachment* attachment) {
    RasterShade shade = {
        .blend_mode = slot->data->blendMode,
    };
    attachment_color(skeleton, slot, attachment, shade.color);
    return shade;
}

static void draw_page_triangle(RgbaImage* canvas,
                               const LoadedPage* page,
                               const float* vertices,
                               const float* uvs,
                               RasterTriangle triangle,
                               RasterTransform transform,
                               RasterShade shade) {
    RasterTriangleRequest request = {
        .target = canvas,
        .texture =
            {
                      .pixels = page->pixels,
                      .width = page->width,
                      .height = page->height,
                      },
        .vertices = vertices,
        .uvs = uvs,
        .triangle = triangle,
        .transform = transform,
        .shade = shade,
    };
    software_rasterizer_draw_triangle(&request);
}

int cpu_renderer_render_image(const CpuRenderRequest* request, RgbaImage* out) {
    if (request == NULL || request->skeleton == NULL || request->atlas == NULL ||
        request->options == NULL || out == NULL) {
        return -1;
    }

    spSkeleton* skeleton = request->skeleton;
    spAtlas* atlas = request->atlas;
    const char* atlas_dir = request->atlas_dir;
    int width = request->options->width;
    int height = request->options->height;
    if (width <= 0 || height <= 0 || request->options->scale <= 0.0f) {
        return -1;
    }

    CpuAtlasPages* owned_pages = NULL;
    const LoadedPage* pages = NULL;
    int page_count = 0;
    if (request->pages != NULL) {
        pages = request->pages->items;
        page_count = request->pages->count;
    } else {
        owned_pages = cpu_atlas_pages_load(atlas, atlas_dir);
        if (owned_pages == NULL) {
            return -1;
        }
        pages = owned_pages->items;
        page_count = owned_pages->count;
    }

    spine_compat_skeleton_update_world_transform(skeleton);

    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 1.0f;
    float max_y = 1.0f;
    int has_bounds = 0;

    for (int i = 0; i < skeleton->slotsCount; i++) {
        spAttachment* attachment = skeleton->drawOrder[i]->attachment;
        if (attachment == NULL ||
            (attachment->type != SP_ATTACHMENT_REGION && attachment->type != SP_ATTACHMENT_MESH)) {
            continue;
        }

        int vertex_count = 4;
        float stack_vertices[8];
        float* vertices = stack_vertices;
        if (attachment->type == SP_ATTACHMENT_REGION) {
            spine_compat_region_compute_world_vertices((spRegionAttachment*)attachment,
                                                       skeleton->drawOrder[i], vertices, 0, 2);
        } else {
            spMeshAttachment* mesh = (spMeshAttachment*)attachment;
            vertex_count = mesh->super.worldVerticesLength / 2;
            vertices = malloc((size_t)mesh->super.worldVerticesLength * sizeof(float));
            if (vertices == NULL) {
                continue;
            }
            spVertexAttachment_computeWorldVertices(&mesh->super, skeleton->drawOrder[i], 0,
                                                    mesh->super.worldVerticesLength, vertices, 0,
                                                    2);
        }

        for (int j = 0; j < vertex_count; j++) {
            float x = vertices[j * 2];
            float y = vertices[j * 2 + 1];
            if (!has_bounds) {
                min_x = max_x = x;
                min_y = max_y = y;
                has_bounds = 1;
            } else {
                min_x = fminf(min_x, x);
                min_y = fminf(min_y, y);
                max_x = fmaxf(max_x, x);
                max_y = fmaxf(max_y, y);
            }
        }
        if (vertices != stack_vertices) {
            free(vertices);
        }
    }

    if (!has_bounds) {
        ZF_LOGE("no region or mesh attachments to render");
        cpu_atlas_pages_free(owned_pages);
        return -1;
    }

    float content_width = max_x - min_x;
    float content_height = max_y - min_y;
    float scale = fminf(((float)width - 8.0f) / content_width,
                        ((float)height - 8.0f) / content_height);
    scale *= request->options->scale;
    min_x -= 4.0f / scale;
    min_y -= 4.0f / scale;

    RgbaImage canvas = {
        .pixels = calloc((size_t)width * (size_t)height * 4, 1),
        .width = width,
        .height = height,
    };
    if (canvas.pixels == NULL) {
        cpu_atlas_pages_free(owned_pages);
        return -1;
    }

    spSkeletonClipping* clipper = spSkeletonClipping_create();
    if (clipper == NULL) {
        rgba_image_free(&canvas);
        cpu_atlas_pages_free(owned_pages);
        return -1;
    }

    RasterTransform transform = {
        .min_x = min_x,
        .min_y = min_y,
        .scale = scale,
    };

    for (int i = 0; i < skeleton->slotsCount; i++) {
        spSlot* slot = skeleton->drawOrder[i];
        spAttachment* attachment = slot->attachment;
        if (attachment == NULL) {
            spSkeletonClipping_clipEnd(clipper, slot);
            continue;
        }
        if (attachment->type == SP_ATTACHMENT_CLIPPING) {
            spSkeletonClipping_clipStart(clipper, slot, (spClippingAttachment*)attachment);
            continue;
        }
        if (attachment->type != SP_ATTACHMENT_REGION && attachment->type != SP_ATTACHMENT_MESH) {
            spSkeletonClipping_clipEnd(clipper, slot);
            continue;
        }

        RasterShade shade = raster_shade_from_attachment(skeleton, slot, attachment);

        spAtlasRegion* region = NULL;
        if (attachment->type == SP_ATTACHMENT_REGION) {
            region = spine_compat_region_atlas_region((spRegionAttachment*)attachment);
        } else {
            region = spine_compat_mesh_atlas_region((spMeshAttachment*)attachment);
        }

        const LoadedPage* page = find_page(pages, page_count, region->page->name);
        if (page == NULL) {
            continue;
        }

        if (attachment->type == SP_ATTACHMENT_REGION) {
            spRegionAttachment* region_attachment = (spRegionAttachment*)attachment;
            float vertices[8];
            unsigned short triangles[6] = {0, 1, 2, 2, 3, 0};
            spine_compat_region_compute_world_vertices(region_attachment, slot, vertices, 0, 2);
            if (spSkeletonClipping_isClipping(clipper)) {
                spSkeletonClipping_clipTriangles(clipper, vertices, 8, triangles, 6,
                                                 region_attachment->uvs, 2);
                for (int tri = 0; tri + 2 < clipper->clippedTriangles->size; tri += 3) {
                    RasterTriangle triangle = {
                        .a = clipper->clippedTriangles->items[tri],
                        .b = clipper->clippedTriangles->items[tri + 1],
                        .c = clipper->clippedTriangles->items[tri + 2],
                    };
                    draw_page_triangle(&canvas, page, clipper->clippedVertices->items,
                                       clipper->clippedUVs->items, triangle, transform, shade);
                }
            } else {
                draw_page_triangle(&canvas, page, vertices, region_attachment->uvs,
                                   (RasterTriangle){.a = 0, .b = 1, .c = 2}, transform, shade);
                draw_page_triangle(&canvas, page, vertices, region_attachment->uvs,
                                   (RasterTriangle){.a = 2, .b = 3, .c = 0}, transform, shade);
            }
        } else {
            spMeshAttachment* mesh = (spMeshAttachment*)attachment;
            float* vertices = malloc((size_t)mesh->super.worldVerticesLength * sizeof(float));
            if (vertices == NULL) {
                continue;
            }
            spVertexAttachment_computeWorldVertices(&mesh->super, slot, 0,
                                                    mesh->super.worldVerticesLength, vertices, 0,
                                                    2);
            if (spSkeletonClipping_isClipping(clipper)) {
                spSkeletonClipping_clipTriangles(clipper, vertices, mesh->super.worldVerticesLength,
                                                 mesh->triangles, mesh->trianglesCount, mesh->uvs,
                                                 2);
                for (int tri = 0; tri + 2 < clipper->clippedTriangles->size; tri += 3) {
                    RasterTriangle triangle = {
                        .a = clipper->clippedTriangles->items[tri],
                        .b = clipper->clippedTriangles->items[tri + 1],
                        .c = clipper->clippedTriangles->items[tri + 2],
                    };
                    draw_page_triangle(&canvas, page, clipper->clippedVertices->items,
                                       clipper->clippedUVs->items, triangle, transform, shade);
                }
            } else {
                for (int tri = 0; tri + 2 < mesh->trianglesCount; tri += 3) {
                    RasterTriangle triangle = {
                        .a = mesh->triangles[tri],
                        .b = mesh->triangles[tri + 1],
                        .c = mesh->triangles[tri + 2],
                    };
                    draw_page_triangle(&canvas, page, vertices, mesh->uvs, triangle, transform,
                                       shade);
                }
            }
            free(vertices);
        }

        spSkeletonClipping_clipEnd(clipper, slot);
    }
    spSkeletonClipping_clipEnd2(clipper);

    spSkeletonClipping_dispose(clipper);
    cpu_atlas_pages_free(owned_pages);

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
        ZF_LOGE("could not write PNG: %s", request->output_path);
        return -1;
    }

    return 0;
}
