# Source/include layout and the spine2dump executable definition.
#
# Requires Dependencies.cmake (for the third-party targets and
# spine2dump_add_embedded_runtime) and StaticAnalysis.cmake to have been
# included beforehand.

set(SPINE2DUMP_INCLUDE_DIRS
    src
    src/cli
    src/core
    src/render
    src/utils
)

set(SPINE2DUMP_COMMON_SOURCES
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
    src/utils/asset_bundle.c
    src/utils/file.c
    src/utils/path.c
)

set(SPINE2DUMP_VERSIONED_APP_SOURCES
    src/core/spine_backend_impl.c
    src/render/cpu_renderer.c
)

add_executable(spine2dump ${SPINE2DUMP_COMMON_SOURCES})
target_include_directories(spine2dump PRIVATE ${SPINE2DUMP_INCLUDE_DIRS})
spine2dump_enable_project_warnings(spine2dump)
spine2dump_enable_clang_tidy(spine2dump)
target_link_libraries(spine2dump PRIVATE argtable3 zf_log spng_static)

foreach(spine_version IN LISTS SPINE2DUMP_SPINE_VERSIONS)
    spine2dump_add_embedded_runtime(${spine_version})
endforeach()
