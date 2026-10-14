-- Release artifacts must not silently link libraries installed on the runner.
-- Apply this to transitive dependencies (notably zlib and x264) as well.
add_requireconfs("*", "**.zlib", "**.x264", { system = false, configs = { shared = false } })

add_requires("argtable3", "libspng 0.7.4", "libuv 1.52.1")
-- Pin the buffer-update API used by the renderer instead of tracking master.
add_requires("sokol 2026.09.14")
add_requires("zf_log master", "libfort v0.4.2")

for _, version in ipairs(SPINE_VERSIONS) do
	add_requires("spine_runtime " .. version, { alias = "sp" .. version:gsub("%.", "") })
end

if has_config("ffmpeg") then
	add_requires("ffmpeg 9.0.2", { configs = { gpl = has_config("gpl") } })
end

if is_plat("windows", "mingw", "msys") then
	includes("packages/libuv.lua")
end

includes("packages/ffmpeg.lua", "packages/spine_runtime.lua", "packages/zf_log.lua", "packages/libfort.lua")
