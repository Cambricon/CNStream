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
option(cnstream_build_apps "build apps" ON)
option(cnstream_build_integrated_lib "build modules into one library" ON)
option(cnstream_build_test_coverage  "Test code coverage" OFF)
option(RELEASE "build type" ON)
option(WITH_FFMPEG "with ffmpeg" ON)
option(WITH_OPENCV "with opencv" ON)

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

# ---[ Google-gflags
include("${PROJECT_SOURCE_DIR}/cmake/FindGFlags.cmake")
list(APPEND 3RDPARTY_INCLUDE_DIRS ${GFLAGS_INCLUDE_DIRS})
list(APPEND 3RDPARTY_LIBS ${GFLAGS_LIBRARIES})

# ---[ Google-glog
include("${PROJECT_SOURCE_DIR}/cmake/FindGlog.cmake")
list(APPEND 3RDPARTY_INCLUDE_DIRS ${GLOG_INCLUDE_DIRS})
list(APPEND 3RDPARTY_LIBS ${GLOG_LIBRARIES})

##OpenCV
if(WITH_OPENCV)
  set(OpenCV_FIND_QUIETLY true)
  #find_package(OpenCV REQUIRED) //this will include opencv_ts which result in crash on centos
  find_package(OpenCV OPTIONAL_COMPONENTS imgcodecs)
  set(imgcodecs_libs ${OpenCV_LIBS})
  find_package(OpenCV REQUIRED core imgproc highgui features2d)
  if(OpenCV_FOUND)
    message(STATUS "OpenCV Found")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_OPENCV")
  endif()
  if(imgcodecs_FOUND)
    list(APPEND OpenCV_LIBS imgcodecs_libs)
  endif()
endif()

##FFmpeg
if(WITH_FFMPEG)
  set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
  find_package(FFmpeg REQUIRED)
  if (FFMPEG_FOUND)
    include_directories(${FFMPEG_INCLUDE_DIR})
    message(STATUS "ffmpeg include: ${FFMPEG_INCLUDE_DIR}")
    message(STATUS "ffmpeg libraries: ${FFMPEG_LIBRARIES}")
    set(HAVE_FFMPEG true)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_FFMPEG")
    message(STATUS "HAVE_FFMPEG enabled")
  else (FFMPEG_FOUND)
    message(FATAL_ERROR "Can't find FFmpeg libs!")
    set (HAVE_FFMPEG false)
  endif (FFMPEG_FOUND)
else()
  set (HAVE_FFMPEG false)
endif()

include_directories(${PROJECT_SOURCE_DIR}/mlu/${MLU_PLATFORM}/include)
link_directories(${PROJECT_SOURCE_DIR}/mlu/${MLU_PLATFORM}/libs/${CMAKE_SYSTEM_PROCESSOR})

# ---[ Includes
include_directories(${PROJECT_SOURCE_DIR}/modules/core/include)

# ---[ Rapidjson
include_directories(${PROJECT_SOURCE_DIR}/3rdparty/rapidjson/include)

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

if(EXISTS ${PROJECT_SOURCE_DIR}/apps)
  if(cnstream_build_apps)
    message(STATUS "generate apps' makefile")
    add_subdirectory(apps)
  endif()
endif()
