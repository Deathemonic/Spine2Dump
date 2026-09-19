SPINE_VERSIONS = { "3.5", "3.6", "3.7", "3.8", "4.0", "4.1", "4.2" }

option("ffmpeg")
set_default(true)
set_showmenu(true)
set_description("Enable in-process FFmpeg media export")

option("gpl")
set_default(false)
set_showmenu(true)
set_description("Enable GPL FFmpeg codecs such as x264")

option("static")
set_default(false)
set_showmenu(true)
set_description("Build a static release binary; expects musl on Linux")

option("warnings_as_errors")
set_default(false)
set_showmenu(true)
set_description("Treat project warnings as build errors")
