# User-facing build options and cache variables for Spine2Dump.

set(SPINE_VERSIONS
    "3.5;3.6;3.7;3.8;4.0;4.1;4.2"
    CACHE STRING "Semicolon-separated Spine runtime branches to embed"
)
option(ENABLE_CLANG_TIDY "Run clang-tidy on project C sources during builds" OFF)
option(ENABLE_WARNINGS_AS_ERRORS "Treat project warnings as build errors" OFF)
option(ENABLE_FFMPEG "Enable in-process FFmpeg media export" ON)
option(ENABLE_GPL_CODECS "Enable GPL FFmpeg codecs such as x264 when building FFmpeg" OFF)
option(STATIC "Build a static release binary; expects musl on Linux" OFF)
if(STATIC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif()
if(STATIC AND APPLE)
    message(FATAL_ERROR "STATIC=ON is not supported on macOS because fully static executables are not supported by Apple's platform linker.")
endif()
if(STATIC AND UNIX AND NOT APPLE)
    execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
        OUTPUT_VARIABLE compiler_target
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT compiler_target MATCHES "musl")
        message(FATAL_ERROR "STATIC=ON on Linux expects a musl compiler target; got '${compiler_target}'.")
    endif()
endif()
option(FETCHCONTENT_UPDATES_DISCONNECTED
       "Skip FetchContent dependency update checks during configure" ON
)
set(FFMPEG_PROVIDER
    "auto"
    CACHE STRING "FFmpeg provider: auto, root, external, off"
)
set(FFMPEG_ROOT "" CACHE PATH "Path to a prebuilt FFmpeg SDK")
set_property(CACHE FFMPEG_PROVIDER PROPERTY STRINGS auto root external off)
