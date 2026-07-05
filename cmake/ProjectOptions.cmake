# User-facing build options and cache variables for Spine2Dump.

set(SPINE_VERSIONS
    "3.5;3.6;3.7;3.8;4.0;4.1;4.2"
    CACHE STRING "Semicolon-separated Spine runtime branches to embed"
)
option(ENABLE_CLANG_TIDY "Run clang-tidy on project C sources during builds" OFF)
option(ENABLE_WARNINGS_AS_ERRORS "Treat project warnings as build errors" OFF)
option(ENABLE_FFMPEG "Enable in-process FFmpeg media export" ON)
option(ENABLE_GPL_CODECS "Enable GPL FFmpeg codecs such as x264 when building FFmpeg" OFF)
option(FETCHCONTENT_UPDATES_DISCONNECTED
       "Skip FetchContent dependency update checks during configure" ON
)
set(FFMPEG_PROVIDER
    "auto"
    CACHE STRING "FFmpeg provider: auto, root, external, off"
)
set(FFMPEG_ROOT "" CACHE PATH "Path to a prebuilt static FFmpeg SDK")
set_property(CACHE FFMPEG_PROVIDER PROPERTY STRINGS auto root external off)
