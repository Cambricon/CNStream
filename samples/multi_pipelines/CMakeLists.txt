cmake_minimum_required(VERSION 2.8.7)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(NOT COMPILER_SUPPORTS_CXX11)
  message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

if(USE_libstdcpp)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
  message("-- Warning: forcing libstdc++ (controlled by USE_libstdcpp option in cmake)")
endif()

set(CNSTREAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SAMPLES_ROOT_DIR ${CNSTREAM_ROOT_DIR}/samples)

# To use sanitizers, the version of GCC is required to be no less than 4.9
option(SANITIZE_MEMORY "Enable MemorySanitizer for sanitized targets." OFF)
option(SANITIZE_ADDRESS "Enable AddressSanitizer for sanitized targets." OFF)
option(SANITIZE_THREAD "Enable ThreadSanitizer for sanitized targets." OFF)
option(SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer for sanitized targets." OFF)

# compile flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D_REENTRANT -fPIC -Wall -Werror")

set(CMAKE_MODULE_PATH ${CNSTREAM_ROOT_DIR}/cmake/sanitizers ${CNSTREAM_ROOT_DIR}/cmake)
find_package(Sanitizers)

# ---[ neuware
find_package(Neuware)
include_directories(${NEUWARE_INCLUDE_DIR})

# ---[ rapidjson
include_directories(${CNSTREAM_ROOT_DIR}/3rdparty/rapidjson/include)

# ---[ opencv
set(OpenCV_FIND_QUIETLY true)
#find_package(OpenCV REQUIRED) //this will include opencv_ts which result in crash on centos
find_package(OpenCV REQUIRED core imgproc highgui features2d)
if(OpenCV_FOUND)
  include_directories(${OpenCV_INCLUDE_DIRS})
  message(STATUS "opencv include: ${OpenCV_INCLUDE_DIRS}")
  message(STATUS "opencv libraries: ${OpenCV_LIBS}")
else()
  message(FATAL_ERROR "opencv not found!")
endif()

# ---[ gflags
include(${CNSTREAM_ROOT_DIR}/cmake/FindGFlags.cmake)
include_directories(${GFLAGS_INCLUDE_DIRS})

# ---[ modules include dirs
include_directories(${CNSTREAM_ROOT_DIR}/modules)
include_directories(${CNSTREAM_ROOT_DIR}/modules/util/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/inference/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/inference2/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/osd/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/source/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/encode/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/track/include)
if(build_display)
  include_directories(${CNSTREAM_ROOT_DIR}/modules/display/include)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_DISPLAY")
  message(STATUS "Samples build with display module.")
endif()
include_directories(${CNSTREAM_ROOT_DIR}/modules/rtsp_sink/include)

# ---[ current include dirs
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)

# ---[ sources
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common common_srcs)
if(build_inference AND (NOT CNIS_USE_MAGICMIND))
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/preprocess preprocess_srcs)
  aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/postprocess postprocess_srcs)
endif()
if(build_inference2)
  aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/video_preprocess video_preprocess_srcs)
  aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/video_postprocess video_postprocess_srcs)
endif()
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/obj_filter obj_filter_srcs)

# ---[ dependencies
set(SAMPLES_DEPENDENCIES "")
include(${CNSTREAM_ROOT_DIR}/cmake/have_cnstream_target.cmake)
have_framework_target(${CNSTREAM_ROOT_DIR})
have_easydk_target(${CNSTREAM_ROOT_DIR})
have_modules_target(${CNSTREAM_ROOT_DIR})
have_modules_contrib_target(${CNSTREAM_ROOT_DIR})
if(HAVE_FRAMEWORK_TARGET)
  list(APPEND SAMPLES_DEPENDENCIES cnstream_core)
endif()
if(HAVE_EASYDK_TARGET)
  list(APPEND SAMPLES_DEPENDENCIES easydk)
endif()
if(HAVE_MODULES_TARGET)
  list(APPEND SAMPLES_DEPENDENCIES cnstream_va)
endif()
if(HAVE_MODULES_CONTRIB_TARGET)
  list(APPEND SAMPLES_DEPENDENCIES cnstream_contrib)
endif()

# ---[ add target
set(EXECUTABLE_OUTPUT_PATH ${SAMPLES_ROOT_DIR}/bin)
add_executable(multi_pipelines ${srcs} ${common_srcs} ${preprocess_srcs} ${postprocess_srcs} ${video_preprocess_srcs} ${video_postprocess_srcs} ${obj_filter_srcs})
add_sanitizers(multi_pipelines)
if(NOT "${SAMPLES_DEPENDENCIES} " STREQUAL " ")
  add_dependencies(multi_pipelines ${SAMPLES_DEPENDENCIES})
endif()
target_link_libraries(multi_pipelines cnstream_va cnstream_core easydk ${OpenCV_LIBS} ${GFLAGS_LIBRARIES} pthread dl)

if(HAVE_MODULES_CONTRIB)
  set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")
  target_link_libraries(multi_pipelines cnstream_contrib)
endif()
