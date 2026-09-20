package("zf_log", {
	urls = "https://github.com/wonder-mice/zf_log.git",
	install = function(package)
		io.writefile(
			"xmake.lua",
			table.concat({
				'target("zf_log", {',
				'    kind = "static",',
				'    languages = "c99",',
				'    warnings = "none",',
				'    files = "zf_log/zf_log.c",',
				'    headerfiles = "(zf_log/zf_log.h)",',
				"})",
			}, "\n")
		)
		import("package.tools.xmake").install(package)
	end,
})
