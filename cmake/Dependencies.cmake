include(FetchContent)
include(ExternalProject)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

function(add_ffmpeg_imported_target)
    if(TARGET FFmpeg::FFmpeg)
        return()
    endif()

    add_library(FFmpeg::FFmpeg INTERFACE IMPORTED GLOBAL)
    target_include_directories(FFmpeg::FFmpeg INTERFACE "${FFMPEG_INCLUDE_DIR}")
    target_link_libraries(FFmpeg::FFmpeg INTERFACE
        "${FFMPEG_AVFORMAT_LIBRARY}"
        "${FFMPEG_AVCODEC_LIBRARY}"
        "${FFMPEG_SWSCALE_LIBRARY}"
        "${FFMPEG_AVUTIL_LIBRARY}"
    )
    if(WIN32)
        target_link_libraries(FFmpeg::FFmpeg INTERFACE bcrypt secur32 ws2_32)
    else()
        target_link_libraries(FFmpeg::FFmpeg INTERFACE m z)
        if(APPLE)
            target_link_libraries(FFmpeg::FFmpeg INTERFACE
                "-framework VideoToolbox"
                "-framework CoreVideo"
                "-framework CoreMedia"
                "-framework CoreFoundation"
            )
        endif()
    endif()
endfunction()

function(find_root_ffmpeg out_found)
    if(FFMPEG_ROOT STREQUAL "")
        set(ffmpeg_search_args)
    else()
        set(ffmpeg_search_args PATHS "${FFMPEG_ROOT}/include" NO_DEFAULT_PATH)
    endif()

    find_path(FFMPEG_INCLUDE_DIR NAMES libavformat/avformat.h
              ${ffmpeg_search_args})
    if(FFMPEG_ROOT STREQUAL "")
        set(ffmpeg_search_args)
    else()
        set(ffmpeg_search_args PATHS "${FFMPEG_ROOT}/lib" NO_DEFAULT_PATH)
    endif()
    find_library(FFMPEG_AVFORMAT_LIBRARY NAMES avformat libavformat
                 ${ffmpeg_search_args})
    find_library(FFMPEG_AVCODEC_LIBRARY NAMES avcodec libavcodec
                 ${ffmpeg_search_args})
    find_library(FFMPEG_AVUTIL_LIBRARY NAMES avutil libavutil
                 ${ffmpeg_search_args})
    find_library(FFMPEG_SWSCALE_LIBRARY NAMES swscale libswscale
                 ${ffmpeg_search_args})

    if(FFMPEG_INCLUDE_DIR AND FFMPEG_AVFORMAT_LIBRARY AND FFMPEG_AVCODEC_LIBRARY AND
       FFMPEG_AVUTIL_LIBRARY AND FFMPEG_SWSCALE_LIBRARY)
        add_ffmpeg_imported_target()
        set(${out_found} TRUE PARENT_SCOPE)
    else()
        set(${out_found} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(add_external_ffmpeg)
    if(WIN32 AND NOT (STATIC AND MINGW))
        if(STATIC)
            message(FATAL_ERROR "STATIC Windows builds need FFMPEG_ROOT pointing to a static FFmpeg SDK, or a MinGW build so FFmpeg can be built from source. The bundled Windows external provider is shared.")
        endif()
        set(ffmpeg_version "8.1.2")
        set(ffmpeg_prefix "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg/ffmpeg-${ffmpeg_version}-full_build-shared")
        file(MAKE_DIRECTORY "${ffmpeg_prefix}/include" "${ffmpeg_prefix}/lib" "${ffmpeg_prefix}/bin")
        ExternalProject_Add(ffmpeg_external
            URL https://github.com/GyanD/codexffmpeg/releases/download/${ffmpeg_version}/ffmpeg-${ffmpeg_version}-full_build-shared.7z
            SOURCE_DIR "${ffmpeg_prefix}"
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS
                "${ffmpeg_prefix}/lib/avformat.lib"
                "${ffmpeg_prefix}/lib/avcodec.lib"
                "${ffmpeg_prefix}/lib/avutil.lib"
                "${ffmpeg_prefix}/lib/swscale.lib"
        )

        set(FFMPEG_ROOT "${ffmpeg_prefix}" CACHE PATH "Path to a prebuilt FFmpeg SDK" FORCE)
        set(FFMPEG_INCLUDE_DIR "${ffmpeg_prefix}/include")
        set(FFMPEG_AVFORMAT_LIBRARY "${ffmpeg_prefix}/lib/avformat.lib")
        set(FFMPEG_AVCODEC_LIBRARY "${ffmpeg_prefix}/lib/avcodec.lib")
        set(FFMPEG_AVUTIL_LIBRARY "${ffmpeg_prefix}/lib/avutil.lib")
        set(FFMPEG_SWSCALE_LIBRARY "${ffmpeg_prefix}/lib/swscale.lib")
        add_ffmpeg_imported_target()
        add_dependencies(FFmpeg::FFmpeg ffmpeg_external)
        return()
    endif()

    set(ffmpeg_prefix "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg")
    file(MAKE_DIRECTORY "${ffmpeg_prefix}/include" "${ffmpeg_prefix}/lib")
    set(ffmpeg_configure_args
        --prefix=${ffmpeg_prefix}
        --disable-shared
        --enable-static
        --disable-doc
        --disable-programs
        --disable-avdevice
        --disable-network
        --disable-everything
        --disable-audiotoolbox
        --disable-bzlib
        --disable-iconv
        --disable-libdrm
        --disable-lzma
        --disable-vaapi
        --disable-vdpau
        --disable-zlib
        --enable-avcodec
        --enable-avformat
        --enable-avutil
        --enable-swscale
        --enable-encoder=mpeg4
        --enable-encoder=ffv1
        --enable-encoder=gif
        --enable-muxer=matroska
        --enable-muxer=gif
    )
    if(ENABLE_GPL_CODECS)
        list(APPEND ffmpeg_configure_args
            --enable-gpl
            --enable-libx264
            --enable-encoder=libx264
            --enable-encoder=h264
        )
    endif()

    ExternalProject_Add(ffmpeg_external
        URL https://ffmpeg.org/releases/ffmpeg-8.1.2.tar.xz
        UPDATE_DISCONNECTED TRUE
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "PATH=$ENV{PATH}" sh <SOURCE_DIR>/configure ${ffmpeg_configure_args}
        BUILD_COMMAND make -j
        INSTALL_COMMAND make install
        BUILD_BYPRODUCTS
            "${ffmpeg_prefix}/lib/libavformat.a"
            "${ffmpeg_prefix}/lib/libavcodec.a"
            "${ffmpeg_prefix}/lib/libavutil.a"
            "${ffmpeg_prefix}/lib/libswscale.a"
    )

    set(FFMPEG_INCLUDE_DIR "${ffmpeg_prefix}/include")
    set(FFMPEG_AVFORMAT_LIBRARY "${ffmpeg_prefix}/lib/libavformat.a")
    set(FFMPEG_AVCODEC_LIBRARY "${ffmpeg_prefix}/lib/libavcodec.a")
    set(FFMPEG_AVUTIL_LIBRARY "${ffmpeg_prefix}/lib/libavutil.a")
    set(FFMPEG_SWSCALE_LIBRARY "${ffmpeg_prefix}/lib/libswscale.a")
    add_ffmpeg_imported_target()
    add_dependencies(FFmpeg::FFmpeg ffmpeg_external)
endfunction()

if(ENABLE_FFMPEG AND NOT FFMPEG_PROVIDER STREQUAL "off")
    set(ffmpeg_found FALSE)
    find_root_ffmpeg(ffmpeg_found)
    if(NOT ffmpeg_found AND (FFMPEG_PROVIDER STREQUAL "external" OR
                             (FFMPEG_PROVIDER STREQUAL "auto" AND STATIC)))
        add_external_ffmpeg()
        if(TARGET FFmpeg::FFmpeg)
            set(ffmpeg_found TRUE)
        endif()
    endif()
    if(NOT ffmpeg_found)
        if(FFMPEG_PROVIDER STREQUAL "root" OR FFMPEG_PROVIDER STREQUAL "external")
            message(FATAL_ERROR "FFmpeg provider '${FFMPEG_PROVIDER}' was requested but could not be configured.")
        endif()
        message(WARNING "FFmpeg media export disabled. Set FFMPEG_ROOT or FFMPEG_PROVIDER=external.")
    endif()
endif()

find_package(OpenMP QUIET COMPONENTS C)
if(NOT TARGET OpenMP::OpenMP_C)
    add_library(spine2dump_openmp INTERFACE)
    target_compile_options(spine2dump_openmp INTERFACE -fopenmp)
    target_link_options(spine2dump_openmp INTERFACE -fopenmp)
    add_library(OpenMP::OpenMP_C ALIAS spine2dump_openmp)
endif()

function(prefix_header out_file prefix)
    set(symbols
        _spAtlasPage_createTexture
        _spAtlasPage_disposeTexture
        _spUtil_readFile
        _clip
        _Entry_create
        _Entry_dispose
        _FromEntry_create
        _FromEntry_dispose
        findIkConstraintIndex
        findPathConstraintIndex
        findTransformConstraintIndex
        indexOf
        readFloat
        readString
        _ToEntry_create
        _ToEntry_dispose
        spine_backend_dump_animations
        spine_backend_dump_expressions
        spine_backend_inspect
        spine_backend_list_expressions
        cpu_renderer_render_image
        cpu_renderer_render_png
        cpu_atlas_pages_load
        cpu_atlas_pages_free
        gpu_renderer_render_image
        spine_slot_walk
    )

    foreach(source IN LISTS ARGN)
        file(READ "${source}" source_text)
        string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_ \t\r\n*/()]*[^A-Za-z0-9_](_?sp[A-Za-z0-9_]+|Json_[A-Za-z0-9_]+|_[A-Za-z]*Entry_[A-Za-z0-9_]+)[ \t\r\n]*\\(" matches "${source_text}")
        foreach(match IN LISTS matches)
            string(REGEX REPLACE ".*[^A-Za-z0-9_](_?sp[A-Za-z0-9_]+|Json_[A-Za-z0-9_]+|_[A-Za-z]*Entry_[A-Za-z0-9_]+)[ \t\r\n]*\\(" "\\1" symbol "${match}")
            list(APPEND symbols "${symbol}")
        endforeach()

        string(REGEX MATCHALL "_SP_ARRAY_(DECLARE|IMPLEMENT)_TYPE[ \t\r\n]*\\([ \t\r\n]*[A-Za-z_][A-Za-z0-9_]*" array_matches "${source_text}")
        foreach(match IN LISTS array_matches)
            string(REGEX REPLACE ".*\\([ \t\r\n]*([A-Za-z_][A-Za-z0-9_]*)" "\\1" array_type "${match}")
            list(APPEND symbols
                "${array_type}_create"
                "${array_type}_dispose"
                "${array_type}_clear"
                "${array_type}_setSize"
                "${array_type}_ensureCapacity"
                "${array_type}_add"
                "${array_type}_addAll"
                "${array_type}_addAllValues"
                "${array_type}_removeAt"
                "${array_type}_contains"
                "${array_type}_pop"
                "${array_type}_peek"
            )
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES symbols)

    file(WRITE "${out_file}" "#ifndef SPINE2DUMP_PREFIX_${prefix}_H\n#define SPINE2DUMP_PREFIX_${prefix}_H\n")
    foreach(symbol IN LISTS symbols)
        file(APPEND "${out_file}" "#define ${symbol} ${prefix}_${symbol}\n")
    endforeach()
    file(APPEND "${out_file}" "#endif\n")
endfunction()

foreach(spine_version IN LISTS SPINE_VERSIONS)
    string(REPLACE "." "_" spine_target_suffix "${spine_version}")
    FetchContent_Declare(spine_runtimes_${spine_target_suffix}
        GIT_REPOSITORY https://github.com/EsotericSoftware/spine-runtimes.git
        GIT_TAG ${spine_version}
        UPDATE_DISCONNECTED TRUE
    )
    FetchContent_GetProperties(spine_runtimes_${spine_target_suffix})
    if(NOT spine_runtimes_${spine_target_suffix}_POPULATED)
        FetchContent_Populate(spine_runtimes_${spine_target_suffix})
    endif()
endforeach()

function(add_embedded_runtime spine_version)
    string(REPLACE "." "_" spine_target_suffix "${spine_version}")
    string(REPLACE "." ";" spine_version_parts "${spine_version}")
    list(GET spine_version_parts 0 spine_version_major)
    list(GET spine_version_parts 1 spine_version_minor)
    set(prefix "sp${spine_version_major}${spine_version_minor}")
    set(runtime_name "spine-runtime-${spine_target_suffix}")
    set(app_name "spine-app-${spine_target_suffix}")
    set(prefix_header "${CMAKE_CURRENT_BINARY_DIR}/generated/spine_prefix_${spine_target_suffix}.h")
    set(spine_runtime_source_dir "${spine_runtimes_${spine_target_suffix}_SOURCE_DIR}/spine-c/spine-c")
    file(GLOB spine_runtime_sources CONFIGURE_DEPENDS
        "${spine_runtime_source_dir}/src/spine/*.c"
    )
    file(GLOB spine_runtime_headers CONFIGURE_DEPENDS
        "${spine_runtime_source_dir}/include/spine/*.h"
    )
    prefix_header("${prefix_header}" "${prefix}" ${spine_runtime_sources} ${spine_runtime_headers})

    add_library(${runtime_name} OBJECT ${spine_runtime_sources})
    target_include_directories(${runtime_name} PRIVATE
        "${spine_runtime_source_dir}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
    target_compile_options(${runtime_name} PRIVATE
        -include "${prefix_header}"
        -Wno-deprecated-declarations
        -Wno-implicit-const-int-float-conversion
    )

    add_library(${app_name} OBJECT ${VERSIONED_APP_SOURCES})
    target_include_directories(${app_name} PRIVATE
        ${INCLUDE_DIRS}
        "${spine_runtime_source_dir}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
        ${zf_log_src_SOURCE_DIR}
        ${spng_src_SOURCE_DIR}/spng
    )
    target_include_directories(${app_name} SYSTEM PRIVATE
        ${libuv_src_SOURCE_DIR}/include
    )
    enable_project_warnings(${app_name})
    enable_clang_tidy(${app_name})
    target_compile_options(${app_name} PRIVATE -include "${prefix_header}")
    target_link_libraries(${app_name} PRIVATE OpenMP::OpenMP_C)
    target_compile_definitions(${app_name} PRIVATE
        RUNTIME_VERSION="${spine_version}"
        RUNTIME_MAJOR=${spine_version_major}
        RUNTIME_MINOR=${spine_version_minor}
    )
    target_sources(spine2dump PRIVATE
        $<TARGET_OBJECTS:${runtime_name}>
        $<TARGET_OBJECTS:${app_name}>
    )
endfunction()

FetchContent_Declare(sokol_src
    GIT_REPOSITORY https://github.com/floooh/sokol.git
    GIT_TAG master
    UPDATE_DISCONNECTED TRUE
)
FetchContent_GetProperties(sokol_src)
if(NOT sokol_src_POPULATED)
    FetchContent_Populate(sokol_src)
endif()

set(LIBUV_BUILD_SHARED OFF CACHE BOOL "Build shared libuv" FORCE)
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "Build libuv tests" FORCE)
set(LIBUV_BUILD_BENCH OFF CACHE BOOL "Build libuv benchmarks" FORCE)
FetchContent_Declare(libuv_src
    GIT_REPOSITORY https://github.com/libuv/libuv.git
    GIT_TAG v1.52.1
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(libuv_src)
if(TARGET uv_a)
    get_target_property(libuv_includes uv_a INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(uv_a PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${libuv_includes}")
endif()

FetchContent_Declare(argtable3_src
    GIT_REPOSITORY https://github.com/argtable/argtable3.git
    GIT_TAG master
    SOURCE_SUBDIR cmake-no-upstream
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(argtable3_src)
add_library(argtable3 STATIC
    ${argtable3_src_SOURCE_DIR}/src/argtable3.c
    ${argtable3_src_SOURCE_DIR}/src/arg_cmd.c
    ${argtable3_src_SOURCE_DIR}/src/arg_date.c
    ${argtable3_src_SOURCE_DIR}/src/arg_dbl.c
    ${argtable3_src_SOURCE_DIR}/src/arg_dstr.c
    ${argtable3_src_SOURCE_DIR}/src/arg_end.c
    ${argtable3_src_SOURCE_DIR}/src/arg_file.c
    ${argtable3_src_SOURCE_DIR}/src/arg_getopt_long.c
    ${argtable3_src_SOURCE_DIR}/src/arg_hashtable.c
    ${argtable3_src_SOURCE_DIR}/src/arg_int.c
    ${argtable3_src_SOURCE_DIR}/src/arg_lit.c
    ${argtable3_src_SOURCE_DIR}/src/arg_rem.c
    ${argtable3_src_SOURCE_DIR}/src/arg_rex.c
    ${argtable3_src_SOURCE_DIR}/src/arg_str.c
    ${argtable3_src_SOURCE_DIR}/src/arg_utils.c
)
target_include_directories(argtable3 PUBLIC
    ${argtable3_src_SOURCE_DIR}/src
)
target_compile_definitions(argtable3 PUBLIC
    ARG_REPLACE_GETOPT=1
    REPLACE_GETOPT
)

FetchContent_Declare(zf_log_src
    GIT_REPOSITORY https://github.com/wonder-mice/zf_log.git
    GIT_TAG master
    SOURCE_SUBDIR cmake-no-upstream
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(zf_log_src)
add_library(zf_log STATIC
    ${zf_log_src_SOURCE_DIR}/zf_log/zf_log.c
)
target_include_directories(zf_log PUBLIC
    ${zf_log_src_SOURCE_DIR}
)

FetchContent_Declare(libfort_src
    GIT_REPOSITORY https://github.com/seleznevae/libfort.git
    GIT_TAG v0.4.2
    UPDATE_DISCONNECTED TRUE
)
FetchContent_GetProperties(libfort_src)
if(NOT libfort_src_POPULATED)
    FetchContent_Populate(libfort_src)
endif()
add_library(fort STATIC
    ${libfort_src_SOURCE_DIR}/lib/fort.c
)
target_include_directories(fort PUBLIC
    ${libfort_src_SOURCE_DIR}/lib
)

FetchContent_Declare(zlib_src
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
    UPDATE_DISCONNECTED TRUE
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Build zlib examples" FORCE)
FetchContent_MakeAvailable(zlib_src)
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

set(SPNG_SHARED OFF CACHE BOOL "Build shared libspng" FORCE)
set(SPNG_STATIC ON CACHE BOOL "Build static libspng" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build libspng examples" FORCE)
FetchContent_Declare(spng_src
    GIT_REPOSITORY https://github.com/randy408/libspng.git
    GIT_TAG v0.7.4
    SOURCE_SUBDIR cmake-no-upstream
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(spng_src)
add_library(spng_static STATIC
    ${spng_src_SOURCE_DIR}/spng/spng.c
)
target_compile_definitions(spng_static PUBLIC
    SPNG_STATIC
)
target_include_directories(spng_static PUBLIC
    ${spng_src_SOURCE_DIR}/spng
)
target_link_libraries(spng_static PRIVATE
    ZLIB::ZLIB
)
