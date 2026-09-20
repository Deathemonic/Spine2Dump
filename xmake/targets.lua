local include_dirs = os.dirs(path.join(os.projectdir(), "includes", "*"))

local versioned_sources = {
	"$(projectdir)/src/core/spine_backend_impl.c",
	"$(projectdir)/src/render/cpu_renderer.c",
	"$(projectdir)/src/render/gpu_renderer.c",
	"$(projectdir)/src/render/spine_slot_walk.c",
}

local common_sources = "$(projectdir)/src/**.c"
for _, source in ipairs(versioned_sources) do
	common_sources = common_sources .. "|" .. source:sub(#"$(projectdir)/src/" + 1)
end

target("spine2dump", {
	kind = "binary",
	files = common_sources,
	includedirs = include_dirs,
	packages = { "argtable3", "libfort", "libspng", "libuv", "sokol", "zf_log" },
})

if has_config("ffmpeg") then
	target("spine2dump", { packages = "ffmpeg", defines = "HAVE_FFMPEG=1" })
end

if is_plat("windows", "mingw", "msys") then
	target("spine2dump", {
		syslinks = { "opengl32", "gdi32", "user32", "ws2_32", "iphlpapi", "userenv" },
		cxflags = { "-fopenmp", { force = true } },
		ldflags = { "-fopenmp", { force = true } },
	})
	if has_config("static") then
		target("spine2dump", { ldflags = { "-static", { force = true } } })
	end
elseif is_plat("macosx") then
	target("spine2dump", {
		frameworks = "OpenGL",
		rpathdirs = "@executable_path",
		cxflags = { "-fopenmp", { force = true } },
		ldflags = { "-fopenmp", { force = true } },
	})
else
	target("spine2dump", {
		syslinks = { "EGL", "GL" },
		rpathdirs = "$ORIGIN",
		cxflags = { "-fopenmp", { force = true } },
		ldflags = { "-fopenmp", "-static-libgcc", { force = true } },
	})
	if has_config("static") then
		target("spine2dump", { ldflags = { "-static-libgcc", { force = true } } })
	end
end

target("spine2dump", {
	on_config = function(target)
		if not has_config("static") then
			return
		end
		if target:is_plat("macosx") then
			raise("static=y is unsupported on macOS: Apple's linker cannot produce fully static executables")
		end
		if target:is_plat("linux") then
			local machine = os.iorunv(target:tool("cc"), { "-dumpmachine" })
			if not (machine and machine:find("musl", 1, true)) then
				raise("static=y on Linux expects a musl compiler target, got '%s'", (machine or "unknown"):trim())
			end
		end
	end,
})

for _, version in ipairs(SPINE_VERSIONS) do
	local tag = version:gsub("%.", "_")
	local prefix = "sp" .. version:gsub("%.", "")
	local major, minor = version:match("(%d+)%.(%d+)")

	target("spine2dump-" .. tag, {
		kind = "object",
		files = versioned_sources,
		includedirs = include_dirs,
		packages = { prefix, "zf_log" },
		forceincludes = "spine_prefix_" .. tag .. ".h",
		defines = { 'RUNTIME_VERSION="' .. version .. '"', "RUNTIME_MAJOR=" .. major, "RUNTIME_MINOR=" .. minor },
		cxflags = { "-fopenmp", { force = true } },
	})

	target("spine2dump", { deps = "spine2dump-" .. tag, packages = prefix })
end