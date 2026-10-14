import("utils.binary.deplibs")

function main(target)
	local source = target:targetfile()
	local executable = path.join(target:bindir(), target:filename())
	local libraries = deplibs(source, { plat = "macosx", resolve_path = true })
	local files = { executable }

	for _, name in ipairs(deplibs(source, { plat = "macosx", resolve_path = false })) do
		if path.filename(name) == "libomp.dylib" then
			local library
			for _, candidate in ipairs(libraries) do
				if path.filename(candidate) == "libomp.dylib" then
					library = candidate
					break
				end
			end
			assert(library and os.isfile(library), "Cannot resolve libomp.dylib")
			local destination = path.join(target:bindir(), "libomp.dylib")
			os.cp(library, destination)
			os.vrunv("chmod", { "u+w", destination })
			os.vrunv("install_name_tool", { "-id", "@executable_path/libomp.dylib", destination })
			os.vrunv("install_name_tool", { "-change", name, "@executable_path/libomp.dylib", executable })
			table.insert(files, destination)
		end
	end
	for _, file in ipairs(files) do
		os.vrunv("codesign", { "--force", "--sign", "-", file })
	end
end
