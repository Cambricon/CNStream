# ==============================================
# Try to find FFmpeg libraries:
# - avcodec
# - avformat
# - avdevice
# - avutil
# - swscale
# - avfilter
#
# FFMPEG_FOUND - system has FFmpeg
# FFMPEG_INCLUDE_DIR - the FFmpeg inc directory
# FFMPEG_LIBRARIES - Link these to use FFmpeg
# ==============================================
# Notice: this original script is from internet.

if (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)
    # in cache already
    set(FFMPEG_FOUND TRUE)
else (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)

    find_path(
            FFMPEG_AVCODEC_INCLUDE_DIR
            NAMES libavcodec/avcodec.h
            PATHS ${_FFMPEG_AVCODEC_INCLUDE_DIRS}
            /usr/include/ffmpeg
            /usr/local/include
   	    /usr/include/x86_64-linux-gnu            
    )

    find_library(
            FFMPEG_LIBAVCODEC
            NAMES avcodec
            PATHS ${_FFMPEG_AVCODEC_LIBRARY_DIRS}
            /usr/lib64
            /usr/local/lib
            /usr/lib/x86_64-linux-gnu
    )
  
    find_library(
            FFMPEG_LIBAVFORMAT
            NAMES avformat
            PATHS ${_FFMPEG_AVFORMAT_LIBRARY_DIRS}
            /usr/lib64
            /usr/local/lib
	    /usr/lib/x86_64-linux-gnu
    )
  
    find_library(
            FFMPEG_LIBSWRESAMPLE
            NAMES swresample
            PATHS ${_FFMPEG_SWRESAMPLE_LIBRARY_DIRS}
            /usr/lib64
            /usr/local/lib
	    /usr/lib/x86_64-linux-gnu
    )
 
    find_library(
            FFMPEG_LIBAVUTIL
            NAMES avutil
            PATHS ${_FFMPEG_AVUTIL_LIBRARY_DIRS}
            /usr/lib64
            /usr/local/lib
	    /usr/lib/x86_64-linux-gnu
    )

    find_library(
            FFMPEG_LIBSWSCALE
            NAMES swscale
            PATHS ${_FFMPEG_SWSCALE_LIBRARY_DIRS}
            /usr/lib64
            /usr/local/lib
	    /usr/lib/x86_64-linux-gnu
    )

    find_library(
            FFMPEG_LIBAVFILTER
            NAMES avfilter
            PATHS ${_FFMPEG_AVFILTER_LIBRARY_DIRS}
            /usr/lib64
            /usr/local/lib
	    /usr/lib/x86_64-linux-gnu
    )


    if (FFMPEG_LIBAVCODEC AND FFMPEG_LIBAVFORMAT AND FFMPEG_LIBAVUTIL)
        set(FFMPEG_FOUND TRUE)
    endif ()

    if (FFMPEG_FOUND)
        set(FFMPEG_INCLUDE_DIR ${FFMPEG_AVCODEC_INCLUDE_DIR})
        set(FFMPEG_LIBRARIES
                ${FFMPEG_LIBAVCODEC}
                ${FFMPEG_LIBAVFORMAT}
                ${FFMPEG_LIBAVUTIL})		
    else (FFMPEG_FOUND)
        message(FATAL_ERROR "Could not find FFmpeg libraries!")
    endif (FFMPEG_FOUND)

endif (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)
