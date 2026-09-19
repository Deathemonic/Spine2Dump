add_requires("argtable3", "libspng 0.7.4", "libuv 1.52.1")
add_requires("sokol master", "zf_log master", "libfort v0.4.2")

for _, version in ipairs(SPINE_VERSIONS) do
	add_requires("spine_runtime " .. version, { alias = "sp" .. version:gsub("%.", "") })
end

if has_config("ffmpeg") then
	add_requires("ffmpeg 8.1.2", { configs = { gpl = has_config("gpl") } })
end

if is_plat("windows", "mingw", "msys") then
	package("libuv")
	set_homepage("https://libuv.org")
	set_description("A multi-platform support library with a focus on asynchronous I/O")
	set_urls("https://github.com/libuv/libuv/archive/refs/tags/v$(version).tar.gz")

	add_versions("1.52.1", "478baf2599bfbc882c355288c9cb6f92e0e7dda435fa04031fa5b607cf3f414c")

	add_syslinks("ws2_32", "iphlpapi", "userenv", "dbghelp", "ole32", "advapi32")

	on_install(function(package)
		local configs = {
			"-DLIBUV_BUILD_TESTS=OFF",
			"-DLIBUV_BUILD_BENCH=OFF",
			"-DBUILD_SHARED_LIBS=OFF",
		}
		import("package.tools.cmake").install(package, configs)
	end)

	on_test(function(package)
		assert(package:has_cfuncs("uv_fs_open", { includes = "uv.h" }))
	end)
	package_end()
end

package("spine_runtime")
set_urls("https://github.com/EsotericSoftware/spine-runtimes.git", { includes = { "spine-c" } })

on_install(function(package)
	local version = package:version_str()
	local tag = version:gsub("%.", "_")
	local prefixfile = "spine_prefix_" .. tag .. ".h"
	os.cd(path.join("spine-c", "spine-c"))

	local prefix = import("prefix", { rootdir = path.join(os.projectdir(), "xmake") })
	io.writefile(
		prefixfile,
		prefix("sp" .. version:gsub("%.", ""), table.join(os.files("src/spine/*.c"), os.files("include/spine/*.h")))
	)

	io.writefile(
		"xmake.lua",
		format(
			[[
            target("spine_runtime_%s")
                set_kind("static")
                set_languages("c99")
                set_warnings("none")
                add_files("src/spine/*.c")
                add_includedirs(".", "include")
                add_forceincludes("%s")
                add_defines("_GNU_SOURCE")
                add_cflags("-Wno-deprecated-declarations", {force = true})
                add_headerfiles("include/(spine/*.h)", "%s")
        ]],
			tag,
			prefixfile,
			prefixfile
		)
	)
	import("package.tools.xmake").install(package)
end)
package_end()

package("zf_log")
set_urls("https://github.com/wonder-mice/zf_log.git")

on_install(function(package)
	io.writefile(
		"xmake.lua",
		[[
            target("zf_log")
                set_kind("static")
                set_languages("c99")
                set_warnings("none")
                add_files("zf_log/zf_log.c")
                add_headerfiles("(zf_log/zf_log.h)")
        ]]
	)
	import("package.tools.xmake").install(package)
end)
package_end()

package("libfort")
set_urls("https://github.com/seleznevae/libfort.git")

on_install(function(package)
	io.writefile(
		"xmake.lua",
		[[
            target("fort")
                set_kind("static")
                set_languages("c99")
                set_warnings("none")
                add_files("lib/fort.c")
                add_headerfiles("lib/fort.h")
        ]]
	)
	import("package.tools.xmake").install(package)
end)
package_end()

local function ffmpeg_install_from_source(package, autoconf)
	local configs = {
		"--disable-shared",
		"--enable-static",
		"--disable-doc",
		"--disable-programs",
		"--disable-avdevice",
		"--disable-network",
		"--disable-everything",
		"--disable-audiotoolbox",
		"--disable-bzlib",
		"--disable-iconv",
		"--disable-libdrm",
		"--disable-lzma",
		"--disable-vaapi",
		"--disable-vdpau",
		"--disable-zlib",
		"--enable-avcodec",
		"--enable-avformat",
		"--enable-avutil",
		"--enable-swscale",
		"--enable-protocol=file",
		"--enable-encoder=mpeg4",
		"--enable-encoder=ffv1",
		"--enable-encoder=gif",
		"--enable-muxer=matroska",
		"--enable-muxer=gif",
	}
	configs.host = ""
	if package:config("gpl") then
		table.join2(configs, {
			"--enable-gpl",
			"--enable-libx264",
			"--enable-encoder=libx264",
			"--pkg-config-flags=--static",
			"--extra-cflags=-DX264_API=",
		})
	end
	if package:is_plat("windows", "mingw", "msys") then
		table.insert(configs, "--target-os=" .. (package:is_arch("x86", "i386") and "mingw32" or "mingw64"))
	end
	local cc = package:build_getenv("cc")
	local cxx = package:build_getenv("cxx")
	local ld = package:build_getenv("ld")
	if cc then
		table.insert(configs, "--cc=" .. path.unix(cc))
	end
	if cxx then
		table.insert(configs, "--cxx=" .. path.unix(cxx))
	end
	if ld then
		table.insert(configs, "--ld=" .. path.unix(ld))
	end
	local packagedeps = {}
	if package:config("gpl") then
		table.insert(packagedeps, "x264")
	end
	autoconf.install(package, configs, { packagedeps = packagedeps })
end

local function ffmpeg_install_prebuilt_windows(package, http)
	local url = format(
		"https://github.com/GyanD/codexffmpeg/releases/download/%s/ffmpeg-%s-full_build-shared.7z",
		package:version_str(),
		package:version_str()
	)
	local archive = "ffmpeg-full_build-shared.7z"
	http.download(url, archive)
	os.vrunv("7z", { "x", "-y", archive, "-o" .. package:installdir() })
	local extracted = os.dirs(path.join(package:installdir(), "ffmpeg-*"))[1]
	if extracted then
		os.cp(path.join(extracted, "include"), package:installdir())
		os.cp(path.join(extracted, "lib"), package:installdir())
		os.cp(path.join(extracted, "bin", "*.dll"), package:installdir("bin"))
		os.rm(extracted)
	end
end

package("ffmpeg")
set_urls("https://ffmpeg.org/releases/ffmpeg-$(version).tar.xz")
add_versions("8.1.2", "464beb5e7bf0c311e68b45ae2f04e9cc2af88851abb4082231742a74d97b524c")

add_configs("gpl", { description = "Enable GPL encoders such as libx264.", default = false, type = "boolean" })

add_deps("nasm")
add_links("avformat", "avcodec", "swscale", "avutil")
if is_plat("windows", "mingw", "msys") then
	add_syslinks("bcrypt", "secur32", "ws2_32", "user32")
elseif is_plat("macosx") then
	add_frameworks("VideoToolbox", "CoreVideo", "CoreMedia", "CoreFoundation")
else
	add_syslinks("m")
end

on_load(function(package)
	local needs_from_source = not package:is_plat("windows") or package:config("static")
	if package:config("gpl") and needs_from_source then
		package:add("deps", "x264")
	end
end)

on_install("windows", function(package)
	if package:config("static") then
		ffmpeg_install_from_source(package, import("package.tools.autoconf"))
	else
		ffmpeg_install_prebuilt_windows(package, import("net.http"))
	end
end)

on_install("mingw", "msys", "linux", "macosx", function(package)
	ffmpeg_install_from_source(package, import("package.tools.autoconf"))
end)
package_end()