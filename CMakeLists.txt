cmake_minimum_required(VERSION 3.20)

project(Spine2Dump VERSION 0.1.0 LANGUAGES C)

include(FetchContent)

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Clang compiler required")
endif()

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SPINE2DUMP_SPINE_VERSIONS
    "3.5;3.6;3.7;3.8;4.0;4.1;4.2"
    CACHE STRING "Semicolon-separated Spine runtime branches to embed"
)
option(ENABLE_CLANG_TIDY "Run clang-tidy on project C sources during builds" OFF)
option(ENABLE_WARNINGS_AS_ERRORS "Treat project warnings as build errors" OFF)
option(FETCHCONTENT_UPDATES_DISCONNECTED
       "Skip FetchContent dependency update checks during configure" ON
)

set(SPINE2DUMP_PROJECT_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wformat=2
    -Wundef
    -Wcast-align
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wold-style-definition
)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
endif()

function(spine2dump_enable_project_warnings target_name)
    target_compile_options(${target_name} PRIVATE ${SPINE2DUMP_PROJECT_WARNINGS})
    if(ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(${target_name} PRIVATE -Werror)
    endif()
endfunction()

function(spine2dump_enable_clang_tidy target_name)
    if(ENABLE_CLANG_TIDY)
        set_target_properties(${target_name} PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_EXE};--quiet")
    endif()
endfunction()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)

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
