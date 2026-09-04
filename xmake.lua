set_project("Spine2Dump")
set_version("1.0.0")
set_xmakever("2.9.0")

set_toolchains("clang")
add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {lsp = "clangd"})

includes("xmake/options.lua",
         "xmake/compiler.lua",
         "xmake/packages.lua",
         "xmake/targets.lua")
