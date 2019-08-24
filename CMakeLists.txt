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

# ---[ Options
option(cnstream_build_tests "build all of modules' unit test" OFF)
option(cnstream_build_samples "build sample programs" ON)
option(cnstream_build_integrated_lib "build modules into one library" ON)
option(cnstream_build_test_coverage  "Test code coverage" OFF)
option(RELEASE "build type" ON)
option(WITH_FFMPEG "with ffmpeg" ON)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
  message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

#######################################################################
if(MLU STREQUAL MLU100)
  message("generate MLU100 Makefile")
  set(MLU_PLATFORM MLU100)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCNS_MLU100")
elseif(MLU STREQUAL MLU270)
  message("generate MLU270 Makefile")
  set(MLU_PLATFORM MLU270)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCNS_MLU270")
else()
  message("generate default(MLU100) Makefile")
  set(MLU_PLATFORM MLU100)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCNS_MLU100")
endif()

# -- Build Flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Werror")
if(cnstream_build_test_coverage)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()
if(RELEASE)
  set(CMAKE_BUILD_TYPE Release)
  add_definitions(-DNDEBUG)
else()
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(USE_libstdcpp)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
  message("-- Warning: forcing libstdc++ (controlled by USE_libstdcpp option in cmake)")
endif()

# ---[ dependencies
if(NOT DEFINED ENV{NEUWARE_HOME})
  set(ENV{NEUWARE_HOME} /usr/local/neuware)
endif()
include_directories("$ENV{NEUWARE_HOME}/include")
link_directories("$ENV{NEUWARE_HOME}/lib64")

##TODO, ffmpeg version 3.3 or higher needed
if(WITH_FFMPEG)
  if(NOT DEFINED ENV{FFMPEG_PATH})
    message(STATUS "-- Warning: WITH_FFMPEG set, but env FFMPEG_PATH not set,use default")
    set(ENV{FFMPEG_PATH} /usr/local)
  endif()
  ## Only a rough inspection
  if(EXISTS $ENV{FFMPEG_PATH}/lib/libavformat.so.58)
    include_directories("$ENV{FFMPEG_PATH}/include")
    link_directories("$ENV{FFMPEG_PATH}/lib")
    set(HAVE_FFMPEG true)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_FFMPEG")
	message(STATUS "HAVE_FFMPEG enabled")
    # ---[ FFmpeg
    list(APPEND FFMPEG_LIBS avformat)
    list(APPEND FFMPEG_LIBS avcodec)
    list(APPEND FFMPEG_LIBS avutil)
    list(APPEND FFMPEG_LIBS swresample)
    list(APPEND FFMPEG_LIBS avfilter)
    list(APPEND FFMPEG_LIBS swscale)
  else()
    set (HAVE_FFMPEG false)
	message(STATUS "-- Warning: WITH_FFMPEG set, but can't find ffmepg version 3.3 or higher version")
  endif()
else()
  set (HAVE_FFMPEG false)
endif()

include_directories(${PROJECT_SOURCE_DIR}/mlu/${MLU_PLATFORM}/include)
link_directories(${PROJECT_SOURCE_DIR}/mlu/${MLU_PLATFORM}/libs/${CMAKE_SYSTEM_PROCESSOR})

# ---[ Includes
include_directories(${PROJECT_SOURCE_DIR}/modules/core/include)

if(cnstream_build_tests)
  add_definitions(-DUNIT_TEST)
  # This must be set in the root directory for the tests to be run by
  # 'make test' or ctest.
  enable_testing()
endif()

# ---[ Subdirectories
add_subdirectory(3rdparty)
add_subdirectory(modules)
add_subdirectory(tools)
if(cnstream_build_samples)
  add_subdirectory(samples)
endif()

