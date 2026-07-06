#ifndef SPINE2DUMP_SPINE_SLOT_WALK_H
#define SPINE2DUMP_SPINE_SLOT_WALK_H

#include <spine/Atlas.h>
#include <spine/Skeleton.h>

#include "atlas_pages.h"
#include "render_options.h"
#include "software_rasterizer.h"

typedef struct SlotWalkTriangle {
    int page_index;
    const float* vertices;
    const float* uvs;
    RasterTriangle triangle;
    RasterTransform transform;
    RasterShade shade;
} SlotWalkTriangle;

typedef void (*SlotWalkSink)(const SlotWalkTriangle* triangle, void* user);

typedef struct SlotWalkRequest {
    spSkeleton* skeleton;
    spAtlas* atlas;
    const CpuAtlasPages* pages;
    const RenderOptions* options;
} SlotWalkRequest;

int spine_slot_walk(const SlotWalkRequest* request, SlotWalkSink sink, void* user);

#endif
