set_toolchains("clang")
set_languages("c23")
add_defines("_GNU_SOURCE")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {lsp = "clangd"})

if has_config("warnings_as_errors") then
    set_warnings("allextra", "error")
else
    set_warnings("allextra")
end

add_cflags("-Wpedantic",
           "-Wshadow",
           "-Wconversion",
           "-Wsign-conversion",
           "-Wformat=2",
           "-Wundef",
           "-Wcast-align",
           "-Wstrict-prototypes",
           "-Wmissing-prototypes",
           "-Wold-style-definition", {force = true})
