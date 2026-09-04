add_requires("argtable3", "libspng 0.7.4", "libuv 1.52.1", "openmp")
add_requires("sokol master", "zf_log master", "libfort v0.4.2")

for _, version in ipairs(SPINE_VERSIONS) do
    add_requires("spine_runtime " .. version, {alias = "sp" .. version:gsub("%.", "")})
end

if has_config("ffmpeg") then
    add_requires("ffmpeg 8.1.2", {configs = {gpl = has_config("gpl")}})
end

package("spine_runtime")
    set_urls("https://github.com/EsotericSoftware/spine-runtimes.git", {includes = {"spine-c"}})

    on_install(function (package)
        local version = package:version_str()
        local tag = version:gsub("%.", "_")
        local prefixfile = "spine_prefix_" .. tag .. ".h"
        os.cd(path.join("spine-c", "spine-c"))

        local prefix = import("prefix", {rootdir = path.join(os.projectdir(), "xmake")})
        io.writefile(prefixfile, prefix("sp" .. version:gsub("%.", ""),
                                       table.join(os.files("src/spine/*.c"),
                                                  os.files("include/spine/*.h"))))

        io.writefile("xmake.lua", format([[
            target("spine_runtime_%s")
                set_kind("static")
                set_languages("c99")
                set_warnings("none")
                add_files("src/spine/*.c")
                add_includedirs(".", "include")
                add_forceincludes("%s")
                add_cflags("-Wno-deprecated-declarations", {force = true})
                add_headerfiles("include/(spine/*.h)", "%s")
        ]], tag, prefixfile, prefixfile))
        import("package.tools.xmake").install(package)
    end)
package_end()

package("zf_log")
    set_urls("https://github.com/wonder-mice/zf_log.git")

    on_install(function (package)
        io.writefile("xmake.lua", [[
            target("zf_log")
                set_kind("static")
                set_languages("c99")
                set_warnings("none")
                add_files("zf_log/zf_log.c")
                add_headerfiles("(zf_log/zf_log.h)")
        ]])
        import("package.tools.xmake").install(package)
    end)
package_end()

package("libfort")
    set_urls("https://github.com/seleznevae/libfort.git")

    on_install(function (package)
        io.writefile("xmake.lua", [[
            target("fort")
                set_kind("static")
                set_languages("c99")
                set_warnings("none")
                add_files("lib/fort.c")
                add_headerfiles("lib/fort.h")
        ]])
        import("package.tools.xmake").install(package)
    end)
package_end()

package("ffmpeg")
    set_urls("https://ffmpeg.org/releases/ffmpeg-$(version).tar.xz")
    add_versions("8.1.2", "464beb5e7bf0c311e68b45ae2f04e9cc2af88851abb4082231742a74d97b524c")

    add_configs("gpl", {description = "Enable GPL encoders such as libx264.", default = false, type = "boolean"})

    add_deps("nasm")
    add_links("avformat", "avcodec", "swscale", "avutil")
    if is_plat("windows", "mingw", "msys") then
        add_syslinks("bcrypt", "secur32", "ws2_32", "user32")
    elseif is_plat("macosx") then
        add_frameworks("VideoToolbox", "CoreVideo", "CoreMedia", "CoreFoundation")
    else
        add_syslinks("m")
    end

    on_load(function (package)
        if package:config("gpl") then
            package:add("deps", "x264")
        end
        if is_subhost("windows") then
            package:add("deps", "msys2", {configs = {msystem = "MINGW64", base_devel = true, make = true}})
        end
    end)

    on_install("windows", "mingw", "msys", "linux", "macosx", function (package)
        local configs = {"--disable-shared",
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
                         "--enable-muxer=gif"}
        configs.host = ""
        if package:config("gpl") then
            table.join2(configs, {"--enable-gpl",
                                  "--enable-libx264",
                                  "--enable-encoder=libx264",
                                  "--pkg-config-flags=--static",
                                  "--extra-cflags=-DX264_API="})
        end
        if package:is_plat("windows") then
            table.join2(configs, {"--toolchain=msvc",
                                  "--target-os=win32",
                                  "--enable-w32threads",
                                  "--extra-cflags=-" .. package:runtimes()})
        elseif package:is_plat("mingw", "msys") then
            table.insert(configs, "--target-os=" .. (package:is_arch("x86", "i386") and "mingw32" or "mingw64"))
        end
        import("package.tools.autoconf").install(package, configs)
    end)
package_end()
