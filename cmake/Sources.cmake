# Source/include layout and the spine2dump executable definition.
#
# Requires Dependencies.cmake (for the third-party targets and
# add_embedded_runtime) and StaticAnalysis.cmake to have been
# included beforehand.

set(INCLUDE_DIRS
    includes/cli
    includes/core
    includes/render
    includes/utils
)

set(COMMON_SOURCES
    src/main.c
    src/cli/args.c
    src/cli/parser.c
    src/cli/run.c
    src/core/assets.c
    src/core/spine_backend.c
    src/core/spine_version.c
    src/render/render_canvas.c
    src/render/software_rasterizer.c
    src/render/image_io.c
    src/render/media_encoder.c
    src/utils/asset_bundle.c
    src/utils/display.c
    src/utils/file.c
    src/utils/log.c
    src/utils/path.c
)

set(VERSIONED_APP_SOURCES
    src/core/spine_backend_impl.c
    src/render/cpu_renderer.c
    src/render/spine_slot_walk.c
)

add_executable(spine2dump ${COMMON_SOURCES})
target_include_directories(spine2dump PRIVATE ${INCLUDE_DIRS})
enable_project_warnings(spine2dump)
enable_clang_tidy(spine2dump)
target_link_libraries(spine2dump PRIVATE argtable3 zf_log fort spng_static uv_a OpenMP::OpenMP_C)
if(TARGET FFmpeg::FFmpeg)
    target_compile_definitions(spine2dump PRIVATE HAVE_FFMPEG=1)
    target_link_libraries(spine2dump PRIVATE FFmpeg::FFmpeg)
endif()

foreach(spine_version IN LISTS SPINE_VERSIONS)
    add_embedded_runtime(${spine_version})
endforeach()
