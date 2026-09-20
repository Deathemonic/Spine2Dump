SPINE_VERSIONS = { "3.5", "3.6", "3.7", "3.8", "4.0", "4.1", "4.2" }

option("ffmpeg", { default = true, showmenu = true, description = "Enable in-process FFmpeg media export" })
option("gpl", { default = false, showmenu = true, description = "Enable GPL FFmpeg codecs such as x264" })
option("static", { default = false, showmenu = true, description = "Build a static release binary" })
option("warn_as_err", { default = false, showmenu = true, description = "Treat project warnings as build errors" })
