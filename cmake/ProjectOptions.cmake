# User-facing build options and cache variables for Spine2Dump.

set(SPINE_VERSIONS
    "3.5;3.6;3.7;3.8;4.0;4.1;4.2"
    CACHE STRING "Semicolon-separated Spine runtime branches to embed"
)
option(ENABLE_CLANG_TIDY "Run clang-tidy on project C sources during builds" OFF)
option(ENABLE_WARNINGS_AS_ERRORS "Treat project warnings as build errors" OFF)
option(FETCHCONTENT_UPDATES_DISCONNECTED
       "Skip FetchContent dependency update checks during configure" ON
)
