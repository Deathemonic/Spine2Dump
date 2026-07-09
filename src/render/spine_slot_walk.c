#include "spine_slot_walk.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <spine/Attachment.h>
#include <spine/MeshAttachment.h>
#include <spine/RegionAttachment.h>
#include <spine/SkeletonClipping.h>
#include <spine/SlotData.h>
#include <spine/VertexAttachment.h>
#include <zf_log/zf_log.h>

#include "spine_compat.h"

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

static int page_index_for(const CpuAtlasPages* pages, const char* name) {
    for (int i = 0; i < pages->count; i++) {
        if (strcmp(pages->items[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int pattern_matches(const char* pattern, const char* text) {
    const char* star = NULL;
    const char* retry = NULL;

    if (pattern == NULL || text == NULL) {
        return 0;
    }

    while (*text != '\0') {
        if (*pattern == *text) {
            pattern++;
            text++;
        } else if (*pattern == '*') {
            star = pattern++;
            retry = text;
        } else if (star != NULL) {
            pattern = star + 1;
            text = ++retry;
        } else {
            return 0;
        }
    }

    while (*pattern == '*') {
        pattern++;
    }
    return *pattern == '\0';
}

static int slot_attachment_hidden(const RenderOptions* options,
                                  const spSlot* slot,
                                  const spAttachment* attachment) {
    if (options == NULL || slot == NULL || attachment == NULL) {
        return 0;
    }
    const char* slot_name = slot->data == NULL ? NULL : slot->data->name;
    const char* attachment_name = attachment->name;
    for (int i = 0; i < options->hide_count; i++) {
        const char* pattern = options->hide_patterns[i];
        if (pattern_matches(pattern, slot_name) || pattern_matches(pattern, attachment_name)) {
            return 1;
        }
    }
    return 0;
}

static void emit(SlotWalkSink sink,
                 void* user,
                 int page_index,
                 const float* vertices,
                 const float* uvs,
                 RasterTriangle triangle,
                 RasterTransform transform,
                 RasterShade shade) {
    SlotWalkTriangle out = {
        .page_index = page_index,
        .vertices = vertices,
        .uvs = uvs,
        .triangle = triangle,
        .transform = transform,
        .shade = shade,
    };
    sink(&out, user);
}

int spine_slot_walk(const SlotWalkRequest* request, SlotWalkSink sink, void* user) {
    if (request == NULL || request->skeleton == NULL || request->pages == NULL ||
        request->options == NULL || sink == NULL) {
        return -1;
    }

    spSkeleton* skeleton = request->skeleton;
    const CpuAtlasPages* pages = request->pages;
    int width = request->options->width;
    int height = request->options->height;
    if (width <= 0 || height <= 0 || request->options->scale <= 0.0f) {
        return -1;
    }

    spine_compat_skeleton_update_world_transform(skeleton);

    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 1.0f;
    float max_y = 1.0f;
    int has_bounds = 0;

    for (int i = 0; i < skeleton->slotsCount; i++) {
        spSlot* slot = skeleton->drawOrder[i];
        spAttachment* attachment = slot->attachment;
        if (attachment == NULL ||
            (attachment->type != SP_ATTACHMENT_REGION && attachment->type != SP_ATTACHMENT_MESH) ||
            slot_attachment_hidden(request->options, slot, attachment)) {
            continue;
        }

        int vertex_count = 4;
        float stack_vertices[8];
        float* vertices = stack_vertices;
        if (attachment->type == SP_ATTACHMENT_REGION) {
            spine_compat_region_compute_world_vertices((spRegionAttachment*)attachment, slot,
                                                       vertices, 0, 2);
        } else {
            spMeshAttachment* mesh = (spMeshAttachment*)attachment;
            vertex_count = mesh->super.worldVerticesLength / 2;
            vertices = malloc((size_t)mesh->super.worldVerticesLength * sizeof(float));
            if (vertices == NULL) {
                continue;
            }
            spVertexAttachment_computeWorldVertices(&mesh->super, slot, 0,
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
        ZF_LOGE("No region or mesh attachments to render.");
        return -1;
    }

    float content_width = max_x - min_x;
    float content_height = max_y - min_y;
    float scale = fminf(((float)width - 8.0f) / content_width,
                        ((float)height - 8.0f) / content_height);
    scale *= request->options->scale;
    min_x -= 4.0f / scale;
    min_y -= 4.0f / scale;

    spSkeletonClipping* clipper = spSkeletonClipping_create();
    if (clipper == NULL) {
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
        if (slot_attachment_hidden(request->options, slot, attachment)) {
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

        int page_index = page_index_for(pages, region->page->name);
        if (page_index < 0) {
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
                    emit(sink, user, page_index, clipper->clippedVertices->items,
                         clipper->clippedUVs->items, triangle, transform, shade);
                }
            } else {
                emit(sink, user, page_index, vertices, region_attachment->uvs,
                     (RasterTriangle){.a = 0, .b = 1, .c = 2}, transform, shade);
                emit(sink, user, page_index, vertices, region_attachment->uvs,
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
                    emit(sink, user, page_index, clipper->clippedVertices->items,
                         clipper->clippedUVs->items, triangle, transform, shade);
                }
            } else {
                for (int tri = 0; tri + 2 < mesh->trianglesCount; tri += 3) {
                    RasterTriangle triangle = {
                        .a = mesh->triangles[tri],
                        .b = mesh->triangles[tri + 1],
                        .c = mesh->triangles[tri + 2],
                    };
                    emit(sink, user, page_index, vertices, mesh->uvs, triangle, transform, shade);
                }
            }
            free(vertices);
        }

        spSkeletonClipping_clipEnd(clipper, slot);
    }
    spSkeletonClipping_clipEnd2(clipper);
    spSkeletonClipping_dispose(clipper);

    return 0;
}
