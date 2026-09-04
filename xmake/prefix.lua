local seed_symbols = {
    "_spAtlasPage_createTexture",
    "_spAtlasPage_disposeTexture",
    "_spUtil_readFile",
    "_clip",
    "_Entry_create",
    "_Entry_dispose",
    "_FromEntry_create",
    "_FromEntry_dispose",
    "findIkConstraintIndex",
    "findPathConstraintIndex",
    "findTransformConstraintIndex",
    "indexOf",
    "readFloat",
    "readString",
    "_ToEntry_create",
    "_ToEntry_dispose",
    "spine_backend_dump_animations",
    "spine_backend_dump_expressions",
    "spine_backend_inspect",
    "spine_backend_list_expressions",
    "cpu_renderer_render_image",
    "cpu_renderer_render_png",
    "cpu_atlas_pages_load",
    "cpu_atlas_pages_free",
    "gpu_renderer_render_image",
    "spine_slot_walk",
}

local array_functions = {
    "create",
    "dispose",
    "clear",
    "setSize",
    "ensureCapacity",
    "add",
    "addAll",
    "addAllValues",
    "removeAt",
    "contains",
    "pop",
    "peek",
}

local call_patterns = {
    "%f[%w_](sp[%w_]+)%s*%(",
    "%f[%w_](_sp[%w_]+)%s*%(",
    "%f[%w_](Json_[%w_]+)%s*%(",
    "%f[%w_](_[%w]*Entry_[%w_]+)%s*%(",
}

local array_patterns = {
    "_SP_ARRAY_DECLARE_TYPE%s*%(%s*([%w_]+)",
    "_SP_ARRAY_IMPLEMENT_TYPE%s*%(%s*([%w_]+)",
}

local function collect(text, symbols)
    for _, pattern in ipairs(call_patterns) do
        local init = 1
        while true do
            local _, finish, symbol = text:find(pattern, init)
            if not symbol then
                break
            end
            symbols[symbol] = true
            init = finish
        end
    end
    for _, pattern in ipairs(array_patterns) do
        local init = 1
        while true do
            local _, finish, array_type = text:find(pattern, init)
            if not array_type then
                break
            end
            for _, suffix in ipairs(array_functions) do
                symbols[array_type .. "_" .. suffix] = true
            end
            init = finish
        end
    end
end

function main(prefix, files)
    local symbols = {}
    for _, symbol in ipairs(seed_symbols) do
        symbols[symbol] = true
    end
    for _, file in ipairs(files) do
        if os.isfile(file) then
            collect(io.readfile(file), symbols)
        end
    end

    local names = {}
    for symbol in pairs(symbols) do
        table.insert(names, symbol)
    end
    table.sort(names)

    local guard = "SPINE2DUMP_PREFIX_" .. prefix:upper() .. "_H"
    local lines = {"#ifndef " .. guard, "#define " .. guard}
    for _, symbol in ipairs(names) do
        table.insert(lines, "#define " .. symbol .. " " .. prefix .. "_" .. symbol)
    end
    table.insert(lines, "#endif")
    return table.concat(lines, "\n") .. "\n"
end
