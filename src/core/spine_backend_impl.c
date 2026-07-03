#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spine/Animation.h>
#include <spine/Atlas.h>
#include <spine/SkeletonBinary.h>
#include <spine/SkeletonData.h>
#include <spine/Skin.h>
#include <spine/extension.h>
#include <zf_log/zf_log.h>

#include "cpu_renderer.h"
#include "file.h"
#include "path.h"
#include "spine_backend.h"
#include "spine_compat.h"

void _spAtlasPage_createTexture(spAtlasPage* self, const char* path) {
    (void)path;
    self->rendererObject = NULL;
}

void _spAtlasPage_disposeTexture(spAtlasPage* self) {
    self->rendererObject = NULL;
}

char* _spUtil_readFile(const char* path, int* length) {
    FILE* file = file_open(path, "rb");
    if (file == NULL) {
        ZF_LOGE("failed to read file: %s", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    char* data = malloc((size_t)size);
    if (data == NULL && size > 0) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (read != (size_t)size) {
        free(data);
        return NULL;
    }

    *length = (int)size;
    return data;
}

typedef struct {
    const spSkeletonData* data;
    int printed;
} PrintSkinAttachmentsContext;

static void print_skin_attachment(int slot_index,
                                  const char* attachment_name,
                                  spAttachment* attachment,
                                  void* user) {
    (void)attachment;
    PrintSkinAttachmentsContext* context = user;
    const char* slot_name = "<unknown-slot>";
    if (slot_index >= 0 && slot_index < context->data->slotsCount) {
        slot_name = context->data->slots[slot_index]->name;
    }

    ZF_LOGI("      %s/%s", slot_name, attachment_name);
    context->printed++;
}

static void print_skin_attachments(const spSkeletonData* data, const spSkin* skin) {
    PrintSkinAttachmentsContext context = {
        .data = data,
        .printed = 0,
    };
    spine_compat_visit_skin_entries(skin, print_skin_attachment, &context);

    if (context.printed == 0) {
        ZF_LOGI("      <no attachments>");
    }
}

static spSkeletonData* load_skeleton_data(const char* skel_path,
                                          const char* atlas_path,
                                          spAtlas** atlas_out,
                                          spSkeletonBinary** binary_out) {
    spAtlas* atlas = spAtlas_createFromFile(atlas_path, NULL);
    if (atlas == NULL) {
        ZF_LOGE("spine-c could not load atlas: %s", atlas_path);
        return NULL;
    }

    spSkeletonBinary* binary = spSkeletonBinary_create(atlas);
    if (binary == NULL) {
        ZF_LOGE("spine-c could not create binary loader");
        spAtlas_dispose(atlas);
        return NULL;
    }

    spSkeletonData* data = spSkeletonBinary_readSkeletonDataFile(binary, skel_path);
    if (data == NULL) {
        ZF_LOGE("spine-c could not load skeleton: %s: %s", skel_path,
                binary->error == NULL ? "<none>" : binary->error);
        spSkeletonBinary_dispose(binary);
        spAtlas_dispose(atlas);
        return NULL;
    }

    *atlas_out = atlas;
    *binary_out = binary;
    return data;
}

static int slot_name_sounds_like_expression(const char* slot_name) {
    return strstr(slot_name, "Default") != NULL || strstr(slot_name, "default") != NULL ||
           strstr(slot_name, "Face") != NULL || strstr(slot_name, "face") != NULL ||
           strstr(slot_name, "Mouth") != NULL || strstr(slot_name, "mouth") != NULL ||
           strstr(slot_name, "Eye") != NULL || strstr(slot_name, "eye") != NULL;
}

static void sanitize_filename(const char* input, char* output, size_t output_size) {
    size_t written = 0;
    for (size_t i = 0; input[i] != '\0' && written + 1 < output_size; i++) {
        char c = input[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '_' || c == '-') {
            output[written++] = c;
        } else {
            output[written++] = '_';
        }
    }
    output[written] = '\0';
}

typedef struct {
    const spSkeletonData* data;
    const spSkin* skin;
    int count;
} ListExpressionsContext;

static void list_expression(int slot_index,
                            const char* attachment_name,
                            spAttachment* attachment,
                            void* user) {
    (void)attachment;
    ListExpressionsContext* context = user;
    const char* slot_name = "<unknown-slot>";
    if (slot_index >= 0 && slot_index < context->data->slotsCount) {
        slot_name = context->data->slots[slot_index]->name;
    }
    if (slot_name_sounds_like_expression(slot_name)) {
        ZF_LOGI("  skin=%s slot=%s attachment=%s", context->skin->name, slot_name, attachment_name);
        context->count++;
    }
}

typedef struct {
    spSkeletonData* data;
    spAtlas* atlas;
    const char* atlas_dir;
    const char* output_dir;
    const SpineDumpExpressionsOptions* options;
    spSkin* skin;
    int rendered;
} DumpExpressionsContext;

static void dump_expression(int slot_index,
                            const char* attachment_name,
                            spAttachment* attachment,
                            void* user) {
    (void)attachment;
    DumpExpressionsContext* context = user;
    const char* slot_name = "<unknown-slot>";
    if (slot_index >= 0 && slot_index < context->data->slotsCount) {
        slot_name = context->data->slots[slot_index]->name;
    }

    if (!slot_name_sounds_like_expression(slot_name)) {
        return;
    }

    spSkeleton* skeleton = spSkeleton_create(context->data);
    if (skeleton == NULL) {
        return;
    }
    spSkeleton_setSkin(skeleton, context->skin);
    spSkeleton_setToSetupPose(skeleton);
    spSkeleton_setAttachment(skeleton, slot_name, attachment_name);

    char safe_slot[128];
    char safe_attachment[128];
    char file_name[320];
    char output_path[1024];
    sanitize_filename(slot_name, safe_slot, sizeof(safe_slot));
    sanitize_filename(attachment_name, safe_attachment, sizeof(safe_attachment));
    snprintf(file_name, sizeof(file_name), "%s__%s.png", safe_slot, safe_attachment);

    CpuRenderPngRequest render_request = {
        .render =
            {
                     .skeleton = skeleton,
                     .atlas = context->atlas,
                     .atlas_dir = context->atlas_dir,
                     .options = &context->options->render,
                     },
        .output_path = output_path,
        .forced_crop = NULL,
    };
    if (path_join(context->output_dir, file_name, output_path, sizeof(output_path)) == 0 &&
        cpu_renderer_render_png(&render_request) == 0) {
        ZF_LOGI("wrote %s", output_path);
        context->rendered++;
    }

    spSkeleton_dispose(skeleton);
}

int spine_backend_inspect(const char* skel_path, const char* atlas_path) {
    spAtlas* atlas = NULL;
    spSkeletonBinary* binary = NULL;
    spSkeletonData* data = load_skeleton_data(skel_path, atlas_path, &atlas, &binary);
    if (data == NULL) {
        return -1;
    }

    ZF_LOGI("spine-c %s load result", SPINE2DUMP_RUNTIME_VERSION);
    ZF_LOGI("  skeleton:   %s", skel_path);
    ZF_LOGI("  atlas:      %s", atlas_path);
    ZF_LOGI("  version:    %s", data->version == NULL ? "<unknown>" : data->version);
    ZF_LOGI("  bounds:     %.2f %.2f %.2f %.2f", spine_compat_skeleton_data_x(data),
            spine_compat_skeleton_data_y(data), data->width, data->height);
    ZF_LOGI("  bones:      %d", data->bonesCount);
    ZF_LOGI("  slots:      %d", data->slotsCount);
    ZF_LOGI("  skins:      %d", data->skinsCount);
    ZF_LOGI("  animations: %d", data->animationsCount);

    ZF_LOGI("Animations");
    if (data->animationsCount == 0) {
        ZF_LOGI("  <none>");
    }
    for (int i = 0; i < data->animationsCount; i++) {
        const spAnimation* animation = data->animations[i];
        ZF_LOGI("  %s %.3fs", animation->name, animation->duration);
    }

    ZF_LOGI("Skins / expression candidates");
    if (data->skinsCount == 0) {
        ZF_LOGI("  <none>");
    }
    for (int i = 0; i < data->skinsCount; i++) {
        const spSkin* skin = data->skins[i];
        ZF_LOGI("  %s", skin->name);
        print_skin_attachments(data, skin);
    }

    spSkeletonData_dispose(data);
    spSkeletonBinary_dispose(binary);
    spAtlas_dispose(atlas);
    return 0;
}

int spine_backend_list_expressions(const char* skel_path, const char* atlas_path) {
    spAtlas* atlas = NULL;
    spSkeletonBinary* binary = NULL;
    spSkeletonData* data = load_skeleton_data(skel_path, atlas_path, &atlas, &binary);
    if (data == NULL) {
        return -1;
    }

    ZF_LOGI("Expression candidates");
    ListExpressionsContext context = {
        .data = data,
        .skin = NULL,
        .count = 0,
    };
    for (int i = 0; i < data->skinsCount; i++) {
        context.skin = data->skins[i];
        spine_compat_visit_skin_entries(data->skins[i], list_expression, &context);
    }

    if (context.count == 0) {
        ZF_LOGI("  <none detected by slot-name heuristic>");
    }

    spSkeletonData_dispose(data);
    spSkeletonBinary_dispose(binary);
    spAtlas_dispose(atlas);
    return 0;
}

int spine_backend_dump_expressions(const char* skel_path,
                                   const char* atlas_path,
                                   const char* output_dir,
                                   const SpineDumpExpressionsOptions* options) {
    spAtlas* atlas = NULL;
    spSkeletonBinary* binary = NULL;
    spSkeletonData* data = load_skeleton_data(skel_path, atlas_path, &atlas, &binary);
    if (data == NULL) {
        return -1;
    }

    path_make_dirs(output_dir);

    char atlas_dir[1024];
    path_dirname(atlas_path, atlas_dir, sizeof(atlas_dir));

    DumpExpressionsContext context = {
        .data = data,
        .atlas = atlas,
        .atlas_dir = atlas_dir,
        .output_dir = output_dir,
        .options = options,
        .skin = NULL,
        .rendered = 0,
    };
    for (int i = 0; i < data->skinsCount; i++) {
        context.skin = data->skins[i];
        spine_compat_visit_skin_entries(data->skins[i], dump_expression, &context);
    }

    if (context.rendered == 0) {
        ZF_LOGW("no expression candidates were rendered");
    }

    spSkeletonData_dispose(data);
    spSkeletonBinary_dispose(binary);
    spAtlas_dispose(atlas);
    return context.rendered > 0 ? 0 : -1;
}

static spSkeleton* create_animation_skeleton(spSkeletonData* data,
                                             spAnimation* animation,
                                             double time) {
    spSkeleton* skeleton = spSkeleton_create(data);
    if (skeleton == NULL) {
        return NULL;
    }
    if (data->defaultSkin != NULL) {
        spSkeleton_setSkin(skeleton, data->defaultSkin);
    }
    spSkeleton_setToSetupPose(skeleton);
    spine_compat_animation_apply(animation, skeleton, 0.0f, (float)time, 0, 1.0f);
    return skeleton;
}

static double animation_frame_time(double start, double end, double fps, int frame) {
    double time = start + ((double)frame / fps);
    return time > end ? end : time;
}

static int compute_animation_crop(spSkeletonData* data,
                                  spAtlas* atlas,
                                  const char* atlas_dir,
                                  spAnimation* animation,
                                  const SpineDumpOptions* options,
                                  double start,
                                  double end,
                                  int frame_count,
                                  RenderCropRect* crop_out) {
    RenderCropRect crop = {};
    for (int frame = 0; frame < frame_count; frame++) {
        double time = animation_frame_time(start, end, options->fps, frame);
        spSkeleton* skeleton = create_animation_skeleton(data, animation, time);
        if (skeleton == NULL) {
            return -1;
        }

        CpuRenderRequest request = {
            .skeleton = skeleton,
            .atlas = atlas,
            .atlas_dir = atlas_dir,
            .options = &options->render,
        };
        RgbaImage image = {};
        int result = cpu_renderer_render_image(&request, &image);
        spSkeleton_dispose(skeleton);
        if (result != 0) {
            return -1;
        }

        RenderCropRect frame_crop = render_canvas_alpha_bounds(&image,
                                                               options->render.alpha_threshold,
                                                               options->render.trim_padding);
        crop = render_crop_union(crop, frame_crop);
        rgba_image_free(&image);
    }

    *crop_out = crop;
    return 0;
}

static spAnimation* find_animation_by_name(spSkeletonData* data, const char* name) {
    for (int i = 0; i < data->animationsCount; i++) {
        if (strcmp(data->animations[i]->name, name) == 0) {
            return data->animations[i];
        }
    }
    return NULL;
}

static int dump_one_animation(spSkeletonData* data,
                              spAtlas* atlas,
                              const char* atlas_dir,
                              spAnimation* animation,
                              const char* output_dir,
                              const SpineDumpOptions* options) {
    char safe_animation[128];
    char animation_dir[1024];
    sanitize_filename(animation->name, safe_animation, sizeof(safe_animation));
    if (path_join(output_dir, safe_animation, animation_dir, sizeof(animation_dir)) != 0) {
        ZF_LOGE("animation output path is too long: %s", animation->name);
        return -1;
    }
    path_make_dirs(animation_dir);

    double start = options->start_seconds;
    double end = options->end_seconds >= 0.0 ? options->end_seconds : animation->duration;
    if (start > animation->duration) {
        ZF_LOGW("skipping %s: start %.3fs is past animation duration %.3fs", animation->name, start,
                animation->duration);
        return 0;
    }
    if (end > animation->duration) {
        end = animation->duration;
    }
    if (end < start) {
        end = start;
    }

    int frame_count = (int)((end - start) * options->fps) + 1;
    if (frame_count < 1) {
        frame_count = 1;
    }

    ZF_LOGI("dumping %s: %.3fs to %.3fs, %d frames", animation->name, start, end, frame_count);

    RenderCropRect animation_crop = {};
    if (options->trim_mode == RENDER_TRIM_ANIMATION &&
        compute_animation_crop(data, atlas, atlas_dir, animation, options, start, end, frame_count,
                               &animation_crop) != 0) {
        return -1;
    }

    for (int frame = 0; frame < frame_count; frame++) {
        double time = animation_frame_time(start, end, options->fps, frame);

        spSkeleton* skeleton = create_animation_skeleton(data, animation, time);
        if (skeleton == NULL) {
            return -1;
        }

        char file_name[64];
        char output_path[1024];
        snprintf(file_name, sizeof(file_name), "frame_%05d.png", frame);
        RenderCropRect* forced_crop = options->trim_mode == RENDER_TRIM_ANIMATION &&
                                              animation_crop.valid
                                          ? &animation_crop
                                          : NULL;
        CpuRenderPngRequest render_request = {
            .render =
                {
                         .skeleton = skeleton,
                         .atlas = atlas,
                         .atlas_dir = atlas_dir,
                         .options = &options->render,
                         },
            .output_path = output_path,
            .forced_crop = forced_crop,
        };
        if (path_join(animation_dir, file_name, output_path, sizeof(output_path)) != 0 ||
            cpu_renderer_render_png(&render_request) != 0) {
            spSkeleton_dispose(skeleton);
            return -1;
        }

        spSkeleton_dispose(skeleton);
    }

    return 0;
}

int spine_backend_dump_animations(const char* skel_path,
                                  const char* atlas_path,
                                  const char* output_dir,
                                  const SpineDumpOptions* options) {
    spAtlas* atlas = NULL;
    spSkeletonBinary* binary = NULL;
    spSkeletonData* data = load_skeleton_data(skel_path, atlas_path, &atlas, &binary);
    if (data == NULL) {
        return -1;
    }

    path_make_dirs(output_dir);

    char atlas_dir[1024];
    path_dirname(atlas_path, atlas_dir, sizeof(atlas_dir));

    int result = 0;
    int dumped = 0;
    if (options->animation != NULL) {
        spAnimation* animation = find_animation_by_name(data, options->animation);
        if (animation == NULL) {
            ZF_LOGE("animation not found: %s", options->animation);
            result = -1;
        } else {
            result = dump_one_animation(data, atlas, atlas_dir, animation, output_dir, options);
            dumped = result == 0 ? 1 : 0;
        }
    } else {
        for (int i = 0; i < data->animationsCount; i++) {
            if (dump_one_animation(data, atlas, atlas_dir, data->animations[i], output_dir,
                                   options) != 0) {
                result = -1;
                break;
            }
            dumped++;
        }
    }

    if (result == 0) {
        ZF_LOGI("dumped %d animation(s) to %s", dumped, output_dir);
    }

    spSkeletonData_dispose(data);
    spSkeletonBinary_dispose(binary);
    spAtlas_dispose(atlas);
    return result;
}
