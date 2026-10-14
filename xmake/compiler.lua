set_toolchains("clang")
if get_config("cc") then
	set_toolset("cc", get_config("cc"))
end
set_languages("c23")
add_defines("_GNU_SOURCE")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", { lsp = "clangd" })

set_policy("check.auto_ignore_flags", false)

if has_config("warn_as_err") then
	set_warnings("allextra", "error")
else
	set_warnings("allextra")
end

add_cflags(
	"-Wpedantic",
	"-Wshadow",
	"-Wconversion",
	"-Wsign-conversion",
	"-Wformat=2",
	"-Wundef",
	"-Wcast-align",
	"-Wstrict-prototypes",
	"-Wmissing-prototypes",
	"-Wold-style-definition",
	{ force = true }
)
