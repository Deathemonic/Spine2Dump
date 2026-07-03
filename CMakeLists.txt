cmake_minimum_required(VERSION 3.20)

project(Spine2Dump VERSION 1.0.0 LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

include(CompilerSettings)
include(ProjectOptions)
include(Formatting)
include(StaticAnalysis)
include(Dependencies)
include(Sources)
