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
#include "display.h"
#include "file.h"
#include "gpu_backend.h"
#include "gpu_renderer.h"
#include "image_io.h"
#include "log.h"
#include "media_encoder.h"
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
    void* data = NULL;
    size_t size = 0;
    if (file_read_all(path, &data, &size) != 0) {
        ZF_LOGE("Failed to read file: %s", path);
        return NULL;
    }
    *length = (int)size;
    return data;
}

typedef struct {
    const spSkeletonData* data;
    DisplayTree* tree;
    int printed;
} PrintSkinAttachmentsContext;

static void collect_skin_attachment(int slot_index,
                                    const char* attachment_name,
                                    spAttachment* attachment,
                                    void* user) {
    (void)attachment;
    PrintSkinAttachmentsContext* context = user;
    const char* slot_name = "<unknown-slot>";
    if (slot_index >= 0 && slot_index < context->data->slotsCount) {
        slot_name = context->data->slots[slot_index]->name;
    }

    char line[512];
    snprintf(line, sizeof(line), "%s/%s", slot_name, attachment_name);
    display_tree_add(context->tree, 1, line);
    context->printed++;
}

static void collect_skin_attachments(const spSkeletonData* data,
                                     const spSkin* skin,
                                     DisplayTree* tree) {
    PrintSkinAttachmentsContext context = {
        .data = data,
        .tree = tree,
        .printed = 0,
    };
    spine_compat_visit_skin_entries(skin, collect_skin_attachment, &context);

    if (context.printed == 0) {
        display_tree_add(tree, 1, "<no attachments>");
    }
}

static spSkeletonData* load_skeleton_data(const char* skel_path,
                                          const char* atlas_path,
                                          spAtlas** atlas_out,
                                          spSkeletonBinary** binary_out) {
    spAtlas* atlas = spAtlas_createFromFile(atlas_path, NULL);
    if (atlas == NULL) {
        ZF_LOGE("Spine-c could not load atlas: %s", atlas_path);
        return NULL;
    }

    spSkeletonBinary* binary = spSkeletonBinary_create(atlas);
    if (binary == NULL) {
        ZF_LOGE("Spine-c could not create binary loader.");
        spAtlas_dispose(atlas);
        return NULL;
    }

    spSkeletonData* data = spSkeletonBinary_readSkeletonDataFile(binary, skel_path);
    if (data == NULL) {
        ZF_LOGE("Spine-c could not load skeleton: %s: %s", skel_path,
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

static GpuBackend* try_gpu_init(const RenderOptions* options, const CpuAtlasPages* pages) {
    if (options->software) {
        return NULL;
    }
    GpuBackend* backend = gpu_backend_init(options->width, options->height);
    if (backend == NULL) {
        ZF_LOGW("GPU init failed. Falling back to CPU renderer.");
        return NULL;
    }
    if (gpu_backend_upload_atlas(backend, pages) != 0) {
        ZF_LOGW("GPU atlas upload failed. Falling back to CPU renderer.");
        gpu_backend_shutdown(backend);
        return NULL;
    }
    return backend;
}

static int should_use_gpu_for_output(const RenderOptions* render, RenderOutputKind output) {
    return !render->software && output != RENDER_OUTPUT_IMAGE;
}

typedef struct RenderImageDispatchRequest {
    GpuBackend* backend;
    spSkeleton* skeleton;
    spAtlas* atlas;
    const char* atlas_dir;
    const CpuAtlasPages* pages;
    const RenderOptions* options;
} RenderImageDispatchRequest;

typedef struct WritePngRequest {
    const RgbaImage* image;
    const RenderOptions* options;
    const RenderCropRect* forced_crop;
    const char* output_path;
} WritePngRequest;

static int render_image_dispatch(const RenderImageDispatchRequest* request, RgbaImage* out) {
    if (request->backend != NULL) {
        GpuRenderRequest gpu_request = {
            .backend = request->backend,
            .skeleton = request->skeleton,
            .atlas = request->atlas,
            .pages = request->pages,
            .options = request->options,
        };
        return gpu_renderer_render_image(&gpu_request, out);
    }
    CpuRenderRequest cpu_request = {
        .skeleton = request->skeleton,
        .atlas = request->atlas,
        .atlas_dir = request->atlas_dir,
        .options = request->options,
        .pages = request->pages,
    };
    return cpu_renderer_render_image(&cpu_request, out);
}

static int write_png_image(const WritePngRequest* request) {
    RgbaImage cropped = {};
    const RgbaImage* output = render_canvas_select_output(request->image, request->options,
                                                          request->forced_crop, &cropped);
    PngEncodeOptions encode_options = png_encode_options_for(request->options->png_compression);
    int result = image_encode_png32_file(request->output_path, output->pixels,
                                         (unsigned)output->width, (unsigned)output->height,
                                         &encode_options);
    rgba_image_free(&cropped);
    if (result != 0) {
        ZF_LOGE("Could not write PNG: %s", request->output_path);
        return -1;
    }
    return 0;
}

typedef struct {
    const spSkeletonData* data;
    const spSkin* skin;
    DisplayTable* table;
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
        display_table_row(context->table, context->skin->name, slot_name, attachment_name, NULL);
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
    CpuAtlasPages* pages;
    GpuBackend* backend;
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

    RgbaImage image = {};
    int result = path_join(context->output_dir, file_name, output_path, sizeof(output_path));
    if (result == 0) {
        result = render_image_dispatch(
            &(RenderImageDispatchRequest){
                .backend = context->backend,
                .skeleton = skeleton,
                .atlas = context->atlas,
                .atlas_dir = context->atlas_dir,
                .pages = context->pages,
                .options = &context->options->render,
            },
            &image);
    }
    if (result == 0) {
        result = write_png_image(&(WritePngRequest){
            .image = &image,
            .options = &context->options->render,
            .forced_crop = NULL,
            .output_path = output_path,
        });
    }
    rgba_image_free(&image);
    if (result == 0) {
        ZF_LOGD("Wrote %s", output_path);
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

    char title[128];
    snprintf(title, sizeof(title), "spine-c %s load result", RUNTIME_VERSION);
    DisplayTable* kv = display_kv_create(title);
    display_kv_row(kv, "skeleton", skel_path);
    display_kv_row(kv, "atlas", atlas_path);
    display_kv_row(kv, "version", data->version == NULL ? "<unknown>" : data->version);
    display_kv_rowf(kv, "bounds", "%.2f %.2f %.2f %.2f", spine_compat_skeleton_data_x(data),
                    spine_compat_skeleton_data_y(data), data->width, data->height);
    display_kv_rowf(kv, "bones", "%d", data->bonesCount);
    display_kv_rowf(kv, "slots", "%d", data->slotsCount);
    display_kv_rowf(kv, "skins", "%d", data->skinsCount);
    display_kv_rowf(kv, "animations", "%d", data->animationsCount);
    display_kv_print(kv);

    char anim_title[64];
    snprintf(anim_title, sizeof(anim_title), "Animations (%d)", data->animationsCount);
    DisplayTable* animations = display_table_create(anim_title);
    display_table_header(animations, "name", "duration", NULL);
    if (data->animationsCount == 0) {
        display_table_row(animations, "<none>", "", NULL);
    }
    for (int i = 0; i < data->animationsCount; i++) {
        const spAnimation* animation = data->animations[i];
        char duration[32];
        snprintf(duration, sizeof(duration), "%.3fs", animation->duration);
        display_table_row(animations, animation->name, duration, NULL);
    }
    display_table_print(animations);

    char skins_title[64];
    snprintf(skins_title, sizeof(skins_title), "Skins (%d)", data->skinsCount);
    DisplayTree* skins = display_tree_create(skins_title);
    if (data->skinsCount == 0) {
        display_tree_add(skins, 0, "<none>");
    }
    for (int i = 0; i < data->skinsCount; i++) {
        const spSkin* skin = data->skins[i];
        display_tree_add(skins, 0, skin->name);
        collect_skin_attachments(data, skin, skins);
    }
    display_tree_print(skins);

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

    DisplayTable* table = display_table_create("Expression candidates");
    display_table_header(table, "skin", "slot", "attachment", NULL);
    ListExpressionsContext context = {
        .data = data,
        .skin = NULL,
        .table = table,
        .count = 0,
    };
    for (int i = 0; i < data->skinsCount; i++) {
        context.skin = data->skins[i];
        spine_compat_visit_skin_entries(data->skins[i], list_expression, &context);
    }

    if (context.count == 0) {
        display_table_row(table, "<none detected by slot-name heuristic>", "", "", NULL);
    }
    display_table_print(table);

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

    CpuAtlasPages* pages = cpu_atlas_pages_load(atlas, atlas_dir);
    if (pages == NULL) {
        spSkeletonData_dispose(data);
        spSkeletonBinary_dispose(binary);
        spAtlas_dispose(atlas);
        return -1;
    }
    GpuBackend* backend = NULL;

    DumpExpressionsContext context = {
        .data = data,
        .atlas = atlas,
        .atlas_dir = atlas_dir,
        .output_dir = output_dir,
        .options = options,
        .skin = NULL,
        .pages = pages,
        .backend = backend,
        .rendered = 0,
    };
    for (int i = 0; i < data->skinsCount; i++) {
        context.skin = data->skins[i];
        spine_compat_visit_skin_entries(data->skins[i], dump_expression, &context);
    }

    if (context.rendered == 0) {
        ZF_LOGW("No expression candidates were rendered.");
    } else {
        ZF_LOG_SUCCESS("Dumped %d expression still(s) to %s", context.rendered, output_dir);
    }

    gpu_backend_shutdown(backend);
    cpu_atlas_pages_free(pages);
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
                                  const CpuAtlasPages* pages,
                                  GpuBackend* backend,
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

        RgbaImage image = {};
        int result = render_image_dispatch(
            &(RenderImageDispatchRequest){
                .backend = backend,
                .skeleton = skeleton,
                .atlas = atlas,
                .atlas_dir = atlas_dir,
                .pages = pages,
                .options = &options->render,
            },
            &image);
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
    sanitize_filename(animation->name, safe_animation, sizeof(safe_animation));

    double start = options->start_seconds;
    double end = options->end_seconds >= 0.0 ? options->end_seconds : animation->duration;
    if (start > animation->duration) {
        ZF_LOGW("Skipping %s: start %.3fs is past animation duration %.3fs", animation->name, start,
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

    ZF_LOGD("Dumping %s: %.3fs to %.3fs, %d frames", animation->name, start, end, frame_count);

    CpuAtlasPages* pages = cpu_atlas_pages_load(atlas, atlas_dir);
    if (pages == NULL) {
        ZF_LOGE("Could not load atlas pages for %s", animation->name);
        return -1;
    }
    GpuBackend* backend = should_use_gpu_for_output(&options->render, options->output)
                              ? try_gpu_init(&options->render, pages)
                              : NULL;
    int gpu_active = backend != NULL;

    RenderCropRect animation_crop = {};
    if ((options->trim_mode == RENDER_TRIM_ANIMATION ||
         (options->output != RENDER_OUTPUT_IMAGE && options->render.trim)) &&
        compute_animation_crop(data, atlas, atlas_dir, pages, backend, animation, options, start,
                               end, frame_count, &animation_crop) != 0) {
        gpu_backend_shutdown(backend);
        cpu_atlas_pages_free(pages);
        return -1;
    }

    if (options->output != RENDER_OUTPUT_IMAGE) {
        char media_name[192];
        char media_path[1024];
        snprintf(media_name, sizeof(media_name), "%s.%s", safe_animation,
                 media_output_extension(options->output));
        if (path_join(output_dir, media_name, media_path, sizeof(media_path)) != 0) {
            ZF_LOGE("Media output path is too long: %s", animation->name);
            gpu_backend_shutdown(backend);
            cpu_atlas_pages_free(pages);
            return -1;
        }

        MediaEncodeRequest encode_request = {
            .output_path = media_path,
            .fps = options->fps,
            .output = options->output,
            .codec = options->codec,
        };
        MediaEncoder* encoder = NULL;
        int result = 0;
        for (int frame = 0; frame < frame_count; frame++) {
            double time = animation_frame_time(start, end, options->fps, frame);
            spSkeleton* skeleton = create_animation_skeleton(data, animation, time);
            if (skeleton == NULL) {
                result = -1;
                break;
            }

            RgbaImage image = {};
            RgbaImage cropped = {};
            result = render_image_dispatch(
                &(RenderImageDispatchRequest){
                    .backend = backend,
                    .skeleton = skeleton,
                    .atlas = atlas,
                    .atlas_dir = atlas_dir,
                    .pages = pages,
                    .options = &options->render,
                },
                &image);
            spSkeleton_dispose(skeleton);
            if (result != 0) {
                break;
            }

            RenderCropRect* forced_crop = animation_crop.valid ? &animation_crop : NULL;
            const RgbaImage* output = render_canvas_select_output(&image, &options->render,
                                                                  forced_crop, &cropped);
            if (encoder == NULL) {
                encoder = media_encoder_open(&encode_request, output->width, output->height);
                if (encoder == NULL) {
                    result = -1;
                }
            }
            if (result == 0) {
                result = media_encoder_write(encoder, output, frame);
            }
            rgba_image_free(&cropped);
            rgba_image_free(&image);
            if (result != 0) {
                break;
            }
        }

        if (encoder != NULL && media_encoder_close(encoder) != 0) {
            result = -1;
        }
        gpu_backend_shutdown(backend);
        cpu_atlas_pages_free(pages);
        return result;
    }

    char frame_dir[1024];
    if (path_join(output_dir, safe_animation, frame_dir, sizeof(frame_dir)) != 0) {
        ZF_LOGE("Animation output path is too long: %s", animation->name);
        gpu_backend_shutdown(backend);
        cpu_atlas_pages_free(pages);
        return -1;
    }
    path_make_dirs(frame_dir);

    int failed = 0;
#pragma omp parallel for schedule(dynamic) if (!gpu_active)
    for (int frame = 0; frame < frame_count; frame++) {
        if (failed) {
            continue;
        }
        double time = animation_frame_time(start, end, options->fps, frame);

        spSkeleton* skeleton = create_animation_skeleton(data, animation, time);
        if (skeleton == NULL) {
#pragma omp atomic write
            failed = 1;
            continue;
        }

        char file_name[64];
        char output_path[1024];
        snprintf(file_name, sizeof(file_name), "frame_%05d.png", frame);
        RenderCropRect* forced_crop = options->trim_mode == RENDER_TRIM_ANIMATION &&
                                              animation_crop.valid
                                          ? &animation_crop
                                          : NULL;
        RgbaImage image = {};
        int result = path_join(frame_dir, file_name, output_path, sizeof(output_path));
        if (result == 0) {
            result = render_image_dispatch(
                &(RenderImageDispatchRequest){
                    .backend = backend,
                    .skeleton = skeleton,
                    .atlas = atlas,
                    .atlas_dir = atlas_dir,
                    .pages = pages,
                    .options = &options->render,
                },
                &image);
        }
        if (result == 0) {
            result = write_png_image(&(WritePngRequest){
                .image = &image,
                .options = &options->render,
                .forced_crop = forced_crop,
                .output_path = output_path,
            });
        }
        rgba_image_free(&image);
        if (result != 0) {
#pragma omp atomic write
            failed = 1;
        }

        spSkeleton_dispose(skeleton);
    }

    gpu_backend_shutdown(backend);
    cpu_atlas_pages_free(pages);
    if (failed) {
        return -1;
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
            ZF_LOGE("Animation not found: %s", options->animation);
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
        ZF_LOG_SUCCESS("Dumped %d animation(s) to %s", dumped, output_dir);
    }

    spSkeletonData_dispose(data);
    spSkeletonBinary_dispose(binary);
    spAtlas_dispose(atlas);
    return result;
}
