include(FindPkgConfig)
# TODO : difficult to find on windows 32. Remove,  or implement alternatives (standalone ffmpeg binary? )
pkg_check_modules(AVUTIL libavutil REQUIRED)
pkg_check_modules(AVFORMAT libavformat REQUIRED)
pkg_check_modules(AVCODEC libavcodec REQUIRED)
pkg_check_modules(SWSCALE libswscale REQUIRED)
pkg_check_modules(LIBPNG libpng REQUIRED)