add_requires("argtable3", "libspng 0.7.4", "libuv 1.52.1")
add_requires("sokol master", "zf_log master", "libfort v0.4.2")

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
