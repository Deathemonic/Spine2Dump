package("spine_runtime", {
	urls = { "https://github.com/EsotericSoftware/spine-runtimes.git", { includes = { "spine-c" } } },
	install = function(package)
		local version = package:version_str()
		local tag = version:gsub("%.", "_")
		local prefixfile = "spine_prefix_" .. tag .. ".h"
		os.cd(path.join("spine-c", "spine-c"))

		local prefix = import("prefix", { rootdir = path.join(os.projectdir(), "xmake") })
		io.writefile(
			prefixfile,
			prefix("sp" .. version:gsub("%.", ""), table.join(os.files("src/spine/*.c"), os.files("include/spine/*.h")))
		)

		local xmake_lua = table.concat({
			format('target("spine_runtime_%s", {', tag),
			'	kind = "static",',
			'	languages = "c99",',
			'	warnings = "none",',
			'	files = "src/spine/*.c",',
			'	includedirs = {".", "include"},',
			format('	forceincludes = "%s",', prefixfile),
			'	defines = "_GNU_SOURCE",',
			'	cflags = {"-Wno-deprecated-declarations", {force = true}},',
			format('	headerfiles = {"include/(spine/*.h)", "%s"},', prefixfile),
			"})",
			"",
		}, "\n")

		io.writefile("xmake.lua", xmake_lua)
		import("package.tools.xmake").install(package)
	end,
})
