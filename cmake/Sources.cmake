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
    src/render/gl_context.c
    src/render/gfx_impl.c
    src/render/gpu_backend.c
    src/render/gpu_frame.c
    src/render/gpu_pipeline.c
    src/utils/asset_bundle.c
    src/utils/display.c
    src/utils/file.c
    src/utils/log.c
    src/utils/path.c
)

set(VERSIONED_APP_SOURCES
    src/core/spine_backend_impl.c
    src/render/cpu_renderer.c
    src/render/gpu_renderer.c
    src/render/spine_slot_walk.c
)

add_executable(spine2dump ${COMMON_SOURCES})
target_include_directories(spine2dump PRIVATE ${INCLUDE_DIRS})
target_include_directories(spine2dump SYSTEM PRIVATE
    ${sokol_src_SOURCE_DIR}
)
enable_project_warnings(spine2dump)
enable_clang_tidy(spine2dump)
target_link_libraries(spine2dump PRIVATE argtable3 zf_log fort spng_static uv_a OpenMP::OpenMP_C)
if(WIN32)
    target_link_options(spine2dump PRIVATE -fopenmp)
    target_link_libraries(spine2dump PRIVATE opengl32 gdi32 user32)
    if(STATIC)
        target_link_options(spine2dump PRIVATE -static -static-libgcc)
    endif()
elseif(APPLE)
    target_link_options(spine2dump PRIVATE -fopenmp)
    target_link_libraries(spine2dump PRIVATE "-framework OpenGL")
else()
    target_link_libraries(spine2dump PRIVATE EGL -l:libgomp.a -lpthread)
    target_link_options(spine2dump PRIVATE -static-libgcc)
endif()
if(TARGET FFmpeg::FFmpeg)
    target_compile_definitions(spine2dump PRIVATE HAVE_FFMPEG=1)
    target_link_libraries(spine2dump PRIVATE FFmpeg::FFmpeg)
endif()

foreach(spine_version IN LISTS SPINE_VERSIONS)
    add_embedded_runtime(${spine_version})
endforeach()

install(TARGETS spine2dump RUNTIME DESTINATION bin COMPONENT Runtime)
if(APPLE)
    set_target_properties(spine2dump PROPERTIES INSTALL_RPATH "@executable_path")
elseif(UNIX)
    set_target_properties(spine2dump PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

set(bundle_search_dirs "")
get_filename_component(bundle_compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
get_filename_component(bundle_toolchain_dir "${bundle_compiler_dir}" DIRECTORY)
list(APPEND bundle_search_dirs "${bundle_compiler_dir}" "${bundle_toolchain_dir}/lib")
if(NOT FFMPEG_ROOT STREQUAL "")
    list(APPEND bundle_search_dirs "${FFMPEG_ROOT}/bin" "${FFMPEG_ROOT}/lib")
endif()
list(REMOVE_DUPLICATES bundle_search_dirs)

if(NOT STATIC OR MINGW)
    install(CODE "set(bundle_exe \"$<TARGET_FILE:spine2dump>\")" COMPONENT Runtime)
    install(CODE "set(bundle_search_dirs \"${bundle_search_dirs}\")" COMPONENT Runtime)
    install(CODE [[
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES "${bundle_exe}"
            RESOLVED_DEPENDENCIES_VAR bundle_resolved
            UNRESOLVED_DEPENDENCIES_VAR bundle_unresolved
            DIRECTORIES ${bundle_search_dirs}
            PRE_EXCLUDE_REGEXES "api-ms-win-.*" "ext-ms-.*"
            POST_EXCLUDE_REGEXES
                ".*[/\\]system32[/\\].*"
                "^/lib/.*"
                "^/usr/lib/(x86_64|aarch64)-linux-gnu/.*"
                "^/System/Library/.*"
                "^/usr/lib/libSystem.*"
        )
        get_filename_component(bundle_exe_name "${bundle_exe}" NAME)
        set(bundle_installed_exe "${CMAKE_INSTALL_PREFIX}/bin/${bundle_exe_name}")
        foreach(bundle_dep IN LISTS bundle_resolved)
            file(INSTALL
                DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                TYPE SHARED_LIBRARY
                FOLLOW_SYMLINK_CHAIN
                FILES "${bundle_dep}"
            )
            if(APPLE)
                get_filename_component(bundle_dep_name "${bundle_dep}" NAME)
                execute_process(COMMAND install_name_tool -change "${bundle_dep}"
                    "@executable_path/${bundle_dep_name}" "${bundle_installed_exe}")
                execute_process(COMMAND install_name_tool -id
                    "@executable_path/${bundle_dep_name}"
                    "${CMAKE_INSTALL_PREFIX}/bin/${bundle_dep_name}")
            endif()
        endforeach()
        foreach(bundle_dep IN LISTS bundle_unresolved)
            message(STATUS "Unresolved runtime dependency left to system: ${bundle_dep}")
        endforeach()
    ]] COMPONENT Runtime)
endif()
