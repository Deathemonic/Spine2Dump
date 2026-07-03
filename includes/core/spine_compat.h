#ifndef SPINE2DUMP_SPINE_COMPAT_H
#define SPINE2DUMP_SPINE_COMPAT_H

#include <stddef.h>

#include <spine/Animation.h>
#include <spine/Atlas.h>
#include <spine/MeshAttachment.h>
#include <spine/RegionAttachment.h>
#include <spine/Skeleton.h>
#include <spine/SkeletonData.h>
#include <spine/Skin.h>
#include <spine/Slot.h>

#ifndef RUNTIME_MAJOR
    #define RUNTIME_MAJOR 4
#endif
#ifndef RUNTIME_MINOR
    #define RUNTIME_MINOR 2
#endif

#define RUNTIME_AT_LEAST(major, minor)                                                  \
    (RUNTIME_MAJOR > (major) ||                                                         \
     (RUNTIME_MAJOR == (major) && RUNTIME_MINOR >= (minor)))

typedef void (*SpineSkinEntryVisitor)(int slot_index,
                                      const char* attachment_name,
                                      spAttachment* attachment,
                                      void* user);

static inline void spine_compat_visit_skin_entries(const spSkin* skin,
                                                   SpineSkinEntryVisitor visitor,
                                                   void* user) {
#if RUNTIME_AT_LEAST(3, 8)
    spSkinEntry* entry = spSkin_getAttachments(skin);
    while (entry != NULL) {
        visitor(entry->slotIndex, entry->name, entry->attachment, user);
        entry = entry->next;
    }
#else
    const _spSkin* internal = (const _spSkin*)skin;
    for (const _Entry* entry = internal->entries; entry != NULL; entry = entry->next) {
        visitor(entry->slotIndex, entry->name, entry->attachment, user);
    }
#endif
}

static inline void spine_compat_skeleton_update_world_transform(spSkeleton* skeleton) {
#if RUNTIME_AT_LEAST(4, 2)
    spSkeleton_updateWorldTransform(skeleton, SP_PHYSICS_UPDATE);
#else
    spSkeleton_updateWorldTransform(skeleton);
#endif
}

static inline void spine_compat_region_compute_world_vertices(spRegionAttachment* attachment,
                                                              spSlot* slot,
                                                              float* vertices,
                                                              int offset,
                                                              int stride) {
#if RUNTIME_AT_LEAST(4, 1)
    spRegionAttachment_computeWorldVertices(attachment, slot, vertices, offset, stride);
#else
    spRegionAttachment_computeWorldVertices(attachment, slot->bone, vertices, offset, stride);
#endif
}

static inline spAtlasRegion* spine_compat_region_atlas_region(spRegionAttachment* attachment) {
#if RUNTIME_AT_LEAST(4, 1)
    return (spAtlasRegion*)attachment->region;
#else
    return (spAtlasRegion*)attachment->rendererObject;
#endif
}

static inline spAtlasRegion* spine_compat_mesh_atlas_region(spMeshAttachment* attachment) {
#if RUNTIME_AT_LEAST(4, 1)
    return (spAtlasRegion*)attachment->region;
#else
    return (spAtlasRegion*)attachment->rendererObject;
#endif
}

static inline float spine_compat_skeleton_data_x(const spSkeletonData* data) {
#if RUNTIME_AT_LEAST(4, 2)
    return data->x;
#else
    (void)data;
    return 0.0f;
#endif
}

static inline float spine_compat_skeleton_data_y(const spSkeletonData* data) {
#if RUNTIME_AT_LEAST(4, 2)
    return data->y;
#else
    (void)data;
    return 0.0f;
#endif
}

static inline void spine_compat_animation_apply(spAnimation* animation,
                                                spSkeleton* skeleton,
                                                float last_time,
                                                float time,
                                                int loop,
                                                float alpha) {
#if RUNTIME_AT_LEAST(3, 7)
    spAnimation_apply(animation, skeleton, last_time, time, loop, NULL, NULL, alpha,
                      SP_MIX_BLEND_REPLACE, SP_MIX_DIRECTION_IN);
#else
    spAnimation_apply(animation, skeleton, last_time, time, loop, NULL, NULL, alpha,
                      SP_MIX_POSE_CURRENT, SP_MIX_DIRECTION_IN);
#endif
}

#endif
