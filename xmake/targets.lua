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

target("spine2dump")
    set_kind("binary")
    add_files(common_sources)
    add_includedirs(include_dirs)
    add_packages("argtable3", "libfort", "libspng", "libuv", "sokol", "zf_log", "openmp")

    if has_config("ffmpeg") then
        add_packages("ffmpeg")
        add_defines("HAVE_FFMPEG=1")
    end

    if is_plat("windows", "mingw", "msys") then
        add_syslinks("opengl32", "gdi32", "user32")
    elseif is_plat("macosx") then
        add_frameworks("OpenGL")
        add_rpathdirs("@executable_path")
    else
        add_syslinks("EGL", "GL")
        add_rpathdirs("$ORIGIN")
    end

    if has_config("static") then
        add_ldflags("-static", {force = true})
    end

    on_config(function (target)
        if not has_config("static") then
            return
        end
        if target:is_plat("macosx") then
            raise("static=y is unsupported on macOS: Apple's linker cannot produce fully static executables")
        end
        if target:is_plat("linux") then
            local machine = os.iorunv(target:tool("cc"), {"-dumpmachine"})
            if not (machine and machine:find("musl", 1, true)) then
                raise("static=y on Linux expects a musl compiler target, got '%s'", (machine or "unknown"):trim())
            end
        end
    end)

for _, version in ipairs(SPINE_VERSIONS) do
    local tag = version:gsub("%.", "_")
    local prefix = "sp" .. version:gsub("%.", "")
    local major, minor = version:match("(%d+)%.(%d+)")

    target("spine2dump-" .. tag)
        set_kind("object")
        add_files(versioned_sources)
        add_includedirs(include_dirs)
        add_packages(prefix, "zf_log", "openmp")
        add_forceincludes("spine_prefix_" .. tag .. ".h")
        add_defines("RUNTIME_VERSION=\"" .. version .. "\"",
                    "RUNTIME_MAJOR=" .. major,
                    "RUNTIME_MINOR=" .. minor)

    target("spine2dump")
        add_deps("spine2dump-" .. tag)
        add_packages(prefix)
end