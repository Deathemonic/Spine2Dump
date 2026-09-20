package("libuv", {
	homepage = "https://libuv.org",
	description = "A multi-platform support library with a focus on asynchronous I/O",
	license = "MIT",
	urls = "https://github.com/libuv/libuv/archive/refs/tags/v$(version).zip",
	versions = { "1.52.1", "d56767702c5b383a78ee9124dd7bdf684f14c681f725117c3f865637a7152cba" },
	syslinks = { "ws2_32", "iphlpapi", "userenv", "dbghelp", "ole32", "advapi32" },
	install = function(package)
		local configs = {
			"-DLIBUV_BUILD_TESTS=OFF",
			"-DLIBUV_BUILD_BENCH=OFF",
			"-DBUILD_SHARED_LIBS=OFF",
		}
		import("package.tools.cmake").install(package, configs)
	end,
	test = function(package)
		assert(package:has_cfuncs("uv_tcp_init", { includes = "uv.h" }))
	end,
})
