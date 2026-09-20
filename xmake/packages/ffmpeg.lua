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

package("ffmpeg", {
	urls = "https://ffmpeg.org/releases/ffmpeg-$(version).tar.xz",
	versions = { "9.0.2", "8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e" },
	configs = { "gpl", { description = "Enable GPL encoders such as libx264.", default = false, type = "boolean" } },
	deps = "nasm",
	links = { "avformat", "avcodec", "swscale", "avutil" },
	load = function(package)
		if package:is_plat("windows", "mingw", "msys") then
			package:add("syslinks", "bcrypt", "secur32", "ws2_32", "user32")
		elseif package:is_plat("macosx") then
			package:add("frameworks", "VideoToolbox", "CoreVideo", "CoreMedia", "CoreFoundation")
		else
			package:add("syslinks", "m")
		end

		local needs_from_source = not package:is_plat("windows") or package:config("static")
		if package:config("gpl") and needs_from_source then
			package:add("deps", "x264")
		end
	end,
	install = {
		"windows",
		"mingw",
		"msys",
		"linux",
		"macosx",
		function(package)
			if package:is_plat("windows") and not package:config("static") then
				ffmpeg_install_prebuilt_windows(package, import("net.http"))
			else
				ffmpeg_install_from_source(package, import("package.tools.autoconf"))
			end
		end,
	},
})
