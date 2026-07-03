# Project warning flags and clang-tidy integration helpers.
#
# Provides:
#   spine2dump_enable_project_warnings(<target>)
#   spine2dump_enable_clang_tidy(<target>)

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
