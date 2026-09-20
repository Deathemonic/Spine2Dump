package("libfort", {
	urls = "https://github.com/seleznevae/libfort.git",
	install = function(package)
		io.writefile(
			"xmake.lua",
			table.concat({
				'target("fort", {',
				'    kind = "static",',
				'    languages = "c99",',
				'    warnings = "none",',
				'    files = "lib/fort.c",',
				'    headerfiles = "lib/fort.h",',
				"})",
			}, "\n")
		)
		import("package.tools.xmake").install(package)
	end,
})
