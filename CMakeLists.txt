# ----------------------------------------------------------------------------
#  Root CMake file for CNStream
# ----------------------------------------------------------------------------

# Disable in-source builds to prevent source tree corruption.
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "Please create a separate directory for build files.")
endif()

cmake_minimum_required(VERSION 2.8.7)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

# ---[ CNStream project
project(CNStream C CXX)
set(CMAKE_VERBOSE_MAKEFILE OFF)

if(CMAKE_TOOLCHAIN_FILE) 
  message("cross compile enabled, CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
  include_directories(${PROJECT_SOURCE_DIR}/include)
endif()

# ---[ Options
option(build_ipc   "build module ipc" ON)
option(build_display   "build module display" OFF)
option(build_encode    "build module encode" ON)
option(build_inference "build module inference" ON)
option(build_osd       "build module osd" ON)
option(build_rtsp_sink "build module rtsp sink" ON)
option(build_source    "build module source" ON)
option(build_track     "build module track" ON)
option(build_tests "build all of modules' unit test" ON)
option(build_samples "build sample programs" ON)
option(build_modules_contrib "build extra modules" ON)
option(build_test_coverage  "Test code coverage" OFF)
option(build_perf  "Record performance" ON)
option(ENABLE_KCF "Build with KCF track" OFF)

option(RELEASE "build type" ON)
option(WITH_FFMPEG "with ffmpeg" ON)
option(WITH_FFMPEG_AVDEVICE "with ffmpeg avdevice" OFF)
option(WITH_OPENCV "with opencv" ON)
option(WITH_CHINESE "with chinese" OFF)
option(WITH_RTSP "with rtsp" ON)

# To use sanitizers, the version of GCC is required to be no less than 4.9
option(SANITIZE_MEMORY "Enable MemorySanitizer for sanitized targets." OFF)
option(SANITIZE_ADDRESS "Enable AddressSanitizer for sanitized targets." OFF)
option(SANITIZE_THREAD "Enable ThreadSanitizer for sanitized targets." OFF)
option(SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer for sanitized targets." OFF)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(COMPILER_SUPPORTS_CXX11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D_REENTRANT")
else()
  message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_REENTRANT")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--as-needed")
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")  #set runtime path

#######################################################################
if(MLU STREQUAL MLU270)
  message("generate MLU270/MLU220 M.2 Makefile")
  set(MLU_PLATFORM MLU270)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCNS_MLU270")
else()
  message("generate default(MLU270) Makefile")
  set(MLU_PLATFORM MLU270)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCNS_MLU270")
endif()

# -- Build Flags
if(CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Wno-sign-compare -Werror")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Werror")
endif()
if(build_test_coverage)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()
if(RELEASE)
  set(CMAKE_BUILD_TYPE Release)
  set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O2 -s")
else()
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -g")
endif()

if(USE_libstdcpp)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
  message("-- Warning: forcing libstdc++ (controlled by USE_libstdcpp option in cmake)")
endif()

# ---[ Google-gflags
if(CMAKE_TOOLCHAIN_FILE)
  set(GFLAGS_LIBRARIES gflags)
else()
  include("${PROJECT_SOURCE_DIR}/cmake/FindGFlags.cmake")
endif()
list(APPEND 3RDPARTY_INCLUDE_DIRS ${GFLAGS_INCLUDE_DIRS})
list(APPEND 3RDPARTY_LIBS ${GFLAGS_LIBRARIES})

# ---[ Google-glog
if(CMAKE_TOOLCHAIN_FILE)
  set(GLOG_LIBRARIES glog)
else()
  include("${PROJECT_SOURCE_DIR}/cmake/FindGlog.cmake")
endif()
list(APPEND 3RDPARTY_INCLUDE_DIRS ${GLOG_INCLUDE_DIRS})
list(APPEND 3RDPARTY_LIBS ${GLOG_LIBRARIES})

## WITH_RTSP
set(Live555_LIBS "")
if(WITH_RTSP)
  include_directories(${PROJECT_SOURCE_DIR}/3rdparty/live555/include/BasicUsageEnvironment)
  include_directories(${PROJECT_SOURCE_DIR}/3rdparty/live555/include/groupsock)
  include_directories(${PROJECT_SOURCE_DIR}/3rdparty/live555/include/liveMedia)
  include_directories(${PROJECT_SOURCE_DIR}/3rdparty/live555/include/UsageEnvironment)
  link_directories(${PROJECT_SOURCE_DIR}/3rdparty/live555/lib)
  set(Live555_LIBS liveMedia UsageEnvironment BasicUsageEnvironment groupsock)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_LIVE555")
endif()

##OpenCV
if(WITH_OPENCV)
  if(CMAKE_TOOLCHAIN_FILE)
    if(EXISTS ${PROJECT_SOURCE_DIR}/include/opencv)
       set(OpenCV_LIBS opencv_core opencv_imgproc opencv_highgui opencv_features2d opencv_flann)
       if(EXISTS ${PROJECT_SOURCE_DIR}/include/opencv2/imgcodecs)
         list(APPEND OpenCV_LIBS opencv_imgcodecs)
       endif()
       message(STATUS "HAVE_OPENCV enabled")
       message(STATUS "opencv libraries: ${OpenCV_LIBS}")
       set(HAVE_OPENCV true)
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_OPENCV")
    else()
      set (HAVE_OPENCV false)
    endif()
  else()
     set(OpenCV_FIND_QUIETLY true)
     #find_package(OpenCV REQUIRED) //this will include opencv_ts which result in crash on centos
     find_package(OpenCV OPTIONAL_COMPONENTS imgcodecs)
     set(imgcodecs_libs ${OpenCV_LIBS})
     find_package(OpenCV REQUIRED core imgproc highgui features2d)
     if(OpenCV_FOUND)
       if(imgcodecs_FOUND)
         list(APPEND OpenCV_LIBS imgcodecs_libs)
       endif()
       message(STATUS "HAVE_OPENCV enabled")
       message(STATUS "opencv libraries: ${OpenCV_LIBS}")
       set(HAVE_OPENCV true)
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_OPENCV")
     else()
       set (HAVE_OPENCV false)
     endif()
   endif()
else()
  set (HAVE_OPENCV false)
endif()

##FFmpeg
if(WITH_FFMPEG)
  if(CMAKE_TOOLCHAIN_FILE)
    if(EXISTS ${PROJECT_SOURCE_DIR}/include/libavcodec)
      set(FFMPEG_LIBRARIES avformat avcodec avutil swscale swresample)
      if (WITH_FFMPEG_AVDEVICE)
        list(APPEND FFMPEG_LIBRARIES avdevice)
      endif ()
      set(HAVE_FFMPEG true)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_FFMPEG")
      message(STATUS "HAVE_FFMPEG enabled")
      message(STATUS "ffmpeg libraries: ${FFMPEG_LIBRARIES}")
    else()
      set (HAVE_FFMPEG false)
    endif()
  else()
    set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
    find_package(FFmpeg REQUIRED)
    if (FFMPEG_FOUND)
      include_directories(${FFMPEG_INCLUDE_DIR})
      set(HAVE_FFMPEG true)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_FFMPEG")
      message(STATUS "HAVE_FFMPEG enabled")
      message(STATUS "ffmpeg include: ${FFMPEG_INCLUDE_DIR}")
      message(STATUS "ffmpeg libraries: ${FFMPEG_LIBRARIES}")
    else ()
      message(FATAL_ERROR "Can't find FFmpeg libs!")
      set (HAVE_FFMPEG false)
    endif ()
  endif()
else()
  set (HAVE_FFMPEG false)
endif()

if(build_ipc)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_IPC")
endif()

if(build_track)
  if(ENABLE_KCF)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_KCF")
  endif()
endif()

if(build_display)
  find_package(SDL2 REQUIRED sdl2)
  if(SDL2_FOUND)
    message(STATUS "SDL2 Found")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_SDL")
    string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
  else()
    message(FATAL_ERROR "Can't find SDL2 libs")
  endif()
endif()

## perf
if(build_perf)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_SQLITE")
endif()

##Chinses label
if(WITH_CHINESE)
  find_package(Freetype REQUIRED)
  if (FREETYPE_FOUND)
    include_directories(${FREETYPE_INCLUDE_DIR_ft2build})
    message(STATUS "freetype include: ${FREETYPE_INCLUDE_DIR_ft2build}")
    message(STATUS "freetype libraries: ${FREETYPE_LIBRARIES}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_FREETYPE")
  else (FREETYPE_FOUND)
    message(FATAL_ERROR "Can't find Freetype libs!")
  endif (FREETYPE_FOUND)
endif()

if (SANITIZE_MEMORY OR SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_UNDEFINED) 
    set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sanitizers" ${CMAKE_MODULE_PATH})
    find_package(Sanitizers)
endif()

# ---[ dependencies
# neuware include/libs
if(NOT DEFINED ENV{NEUWARE_HOME})
  set(ENV{NEUWARE_HOME} /usr/local/neuware)
endif()
  
if(MLU_PLATFORM STREQUAL MLU270)
  list(APPEND CN_LIBS easydk cnrt cncodec cndrv)
else()
  message(FATAL_ERROR "MLU_PLATFORM not supported!")
endif()

include_directories("$ENV{NEUWARE_HOME}/include")
link_directories("$ENV{NEUWARE_HOME}/lib64")

# ---[ easydk Includes
include_directories(${PROJECT_SOURCE_DIR}/easydk/include)

# cns modules libs
link_directories(${PROJECT_SOURCE_DIR}/lib)

# ---[ Includes
include_directories(${PROJECT_SOURCE_DIR}/framework/core/include)

# ---[ Rapidjson
include_directories(${PROJECT_SOURCE_DIR}/3rdparty/rapidjson/include)

if(build_tests)
  add_definitions(-DUNIT_TEST)
  # This must be set in the root directory for the tests to be run by
  # 'make test' or ctest.
  enable_testing()
endif()
if(WITH_FFMPEG_AVDEVICE)
  add_definitions(-DHAVE_FFMPEG_AVDEVICE)
endif()
# ---[ Subdirectories
add_subdirectory(3rdparty)
add_subdirectory(easydk)
add_subdirectory(modules)
add_subdirectory(framework)
if(build_modules_contrib)
  add_subdirectory(modules_contrib)
endif()
add_subdirectory(tools)
if(build_samples)
  add_subdirectory(samples)
endif()

