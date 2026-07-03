#include "spine_backend.h"

#include <stdio.h>

#include <zf_log/zf_log.h>

#include "spine_version.h"

typedef struct SpineBackendVTable {
    const char* runtime;
    int major;
    int minor;
    int (*inspect)(const char* skel_path, const char* atlas_path);
    int (*list_expressions)(const char* skel_path, const char* atlas_path);
    int (*dump_expressions)(const char* skel_path,
                            const char* atlas_path,
                            const char* output_dir,
                            const SpineDumpExpressionsOptions* options);
    int (*dump_animations)(const char* skel_path,
                           const char* atlas_path,
                           const char* output_dir,
                           const SpineDumpOptions* options);
} SpineBackendVTable;

#define DECLARE_BACKEND(prefix)                                                                    \
    int prefix##_spine_backend_inspect(const char* skel_path, const char* atlas_path);             \
    int prefix##_spine_backend_list_expressions(const char* skel_path, const char* atlas_path);    \
    int prefix##_spine_backend_dump_expressions(const char* skel_path, const char* atlas_path,     \
                                                const char* output_dir,                            \
                                                const SpineDumpExpressionsOptions* options);       \
    int prefix##_spine_backend_dump_animations(const char* skel_path, const char* atlas_path,      \
                                               const char* output_dir,                             \
                                               const SpineDumpOptions* options)

DECLARE_BACKEND(sp35);
DECLARE_BACKEND(sp36);
DECLARE_BACKEND(sp37);
DECLARE_BACKEND(sp38);
DECLARE_BACKEND(sp40);
DECLARE_BACKEND(sp41);
DECLARE_BACKEND(sp42);

#define BACKEND_ENTRY(runtime_text, major_value, minor_value, prefix)                              \
    {                                                                                              \
        .runtime = runtime_text,                                                                   \
        .major = major_value,                                                                      \
        .minor = minor_value,                                                                      \
        .inspect = prefix##_spine_backend_inspect,                                                 \
        .list_expressions = prefix##_spine_backend_list_expressions,                               \
        .dump_expressions = prefix##_spine_backend_dump_expressions,                               \
        .dump_animations = prefix##_spine_backend_dump_animations,                                 \
    }

static const SpineBackendVTable backends[] = {
    BACKEND_ENTRY("3.5", 3, 5, sp35), BACKEND_ENTRY("3.6", 3, 6, sp36),
    BACKEND_ENTRY("3.7", 3, 7, sp37), BACKEND_ENTRY("3.8", 3, 8, sp38),
    BACKEND_ENTRY("4.0", 4, 0, sp40), BACKEND_ENTRY("4.1", 4, 1, sp41),
    BACKEND_ENTRY("4.2", 4, 2, sp42),
};

static const SpineBackendVTable* find_backend_for_skel(const char* skel_path) {
    char version[32];
    if (spine_version_detect_file(skel_path, version, sizeof(version)) != 0) {
        ZF_LOGE("could not detect Spine version from %s", skel_path);
        return NULL;
    }

    int major = 0;
    int minor = 0;
    if (spine_version_major_minor(version, &major, &minor) != 0) {
        ZF_LOGE("invalid Spine version in %s: %s", skel_path, version);
        return NULL;
    }

    for (size_t i = 0; i < sizeof(backends) / sizeof(backends[0]); i++) {
        if (backends[i].major == major && backends[i].minor == minor) {
            ZF_LOGI("using Spine runtime %s for skeleton version %s", backends[i].runtime, version);
            return &backends[i];
        }
    }

    ZF_LOGE("unsupported Spine skeleton version: %s", version);
    return NULL;
}

int spine_backend_inspect(const char* skel_path, const char* atlas_path) {
    const SpineBackendVTable* backend = find_backend_for_skel(skel_path);
    return backend == NULL ? -1 : backend->inspect(skel_path, atlas_path);
}

int spine_backend_list_expressions(const char* skel_path, const char* atlas_path) {
    const SpineBackendVTable* backend = find_backend_for_skel(skel_path);
    return backend == NULL ? -1 : backend->list_expressions(skel_path, atlas_path);
}

int spine_backend_dump_expressions(const char* skel_path,
                                   const char* atlas_path,
                                   const char* output_dir,
                                   const SpineDumpExpressionsOptions* options) {
    const SpineBackendVTable* backend = find_backend_for_skel(skel_path);
    return backend == NULL ? -1
                           : backend->dump_expressions(skel_path, atlas_path, output_dir, options);
}

int spine_backend_dump_animations(const char* skel_path,
                                  const char* atlas_path,
                                  const char* output_dir,
                                  const SpineDumpOptions* options) {
    const SpineBackendVTable* backend = find_backend_for_skel(skel_path);
    return backend == NULL ? -1
                           : backend->dump_animations(skel_path, atlas_path, output_dir, options);
}
