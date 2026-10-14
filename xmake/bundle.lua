local function dependencies(file)
	local result = {}
	for line in os.iorunv("otool", { "-L", file }):gmatch("[^\n]+") do
		local name = line:match("^%s+(.+)%s+%(compatibility version")
		if name then
			table.insert(result, name)
		end
	end
	return result
end

local function system_library(name)
	return name:find("/usr/lib/", 1, true) == 1 or name:find("/System/Library/", 1, true) == 1
end

function main(target)
	local bindir = target:bindir()
	local executable = path.join(bindir, target:filename())
	local executable_dir = path.directory(target:targetfile())
	local copied = {}
	local files = { executable }

	local function expand(name, source)
		name = name:gsub("^@loader_path", path.directory(source))
		return name:gsub("^@executable_path", executable_dir)
	end

	local function resolve(name, source)
		if name:find("@rpath/", 1, true) == 1 then
			for _, owner in ipairs({ source, target:targetfile() }) do
				local commands = os.iorunv("otool", { "-l", owner })
				for rpath in commands:gmatch("cmd LC_RPATH.-\n%s*path (.-) %(offset") do
					local candidate = path.join(expand(rpath, owner), name:sub(8))
					if os.isfile(candidate) then
						return candidate
					end
				end
			end
		else
			local candidate = expand(name, source)
			if os.isfile(candidate) then
				return candidate
			end
		end
		raise("Cannot bundle macOS dependency '%s' of '%s'", name, source)
	end

	local function bundle(source, installed)
		local own_id = os.iorunv("otool", { "-D", source }):match("\n([^\n]+)")
		for _, name in ipairs(dependencies(source)) do
			if name ~= own_id and not system_library(name) then
				local library = resolve(name, source)
				local basename = path.filename(library)
				local destination = path.join(bindir, basename)
				if copied[basename] then
					assert(copied[basename] == library, "Conflicting bundled library: " .. basename)
				else
					copied[basename] = library
					os.cp(library, destination)
					os.vrunv("chmod", { "u+w", destination })
					os.vrunv("install_name_tool", { "-id", "@executable_path/" .. basename, destination })
					table.insert(files, destination)
					bundle(library, destination)
				end
				os.vrunv("install_name_tool", { "-change", name, "@executable_path/" .. basename, installed })
			end
		end
	end

	bundle(target:targetfile(), executable)
	for _, file in ipairs(files) do
		os.vrunv("codesign", { "--force", "--sign", "-", file })
	end
end
