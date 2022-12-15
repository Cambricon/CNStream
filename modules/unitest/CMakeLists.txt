# ---[ gtest
if(NOT build_gtest_already)  # set by parent
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/googletest ${PROJECT_BINARY_DIR}/googletest)
  set(build_gtest_already ON)
endif()
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/googletest/googletest/include)

# ---[ gflags
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FindGFlags.cmake)
include_directories(${GFLAGS_INCLUDE_DIRS})

# ---[ glog
include(${CNSTREAM_ROOT_DIR}/cmake/FindGlog.cmake)
include_directories(${GLOG_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(3RDPARTY_LIBS "")
set(test_srcs "")
list(APPEND test_srcs ${CMAKE_CURRENT_SOURCE_DIR}/test_base.cpp)
list(APPEND test_srcs ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp)
list(APPEND test_srcs ${CMAKE_CURRENT_SOURCE_DIR}/test_frame.cpp)

if(BUILD_VENCODE)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../encode/src)
  file(GLOB_RECURSE test_encode_srcs ${CMAKE_CURRENT_SOURCE_DIR}/encode/*.cpp)
  list(APPEND test_srcs ${test_encode_srcs})
endif()

list(APPEND 3RDPARTY_LIBS ${FFMPEG_LIBRARIES})
list(APPEND 3RDPARTY_LIBS ${OpenCV_LIBS})
list(APPEND 3RDPARTY_LIBS ${GLOG_LIBRARIES})
list(APPEND 3RDPARTY_LIBS ${GFLAGS_LIBRARIES})
list(APPEND 3RDPARTY_LIBS cnstream_va cnstream_core easydk ${CNRT_LIBS})

# ---[ kafka
if(BUILD_KAFKA)
  find_package(LibRDKafka)
  if(LIBRDKAFKA_FOUND)
    include_directories(${LibRDKafka_INCLUDE_DIR})
    list(APPEND 3RDPARTY_LIBS ${LibRDKafka_C_LIBRARIES})
    message(STATUS "LibRDKafka include: ${LibRDKafka_INCLUDE_DIR}")
    message(STATUS "LibRDKafka libraries: ${LibRDKafka_C_LIBRARIES}")
  else()
    message(FATAL_ERROR "rdkafka not found!")
  endif()
endif()


# ---[ live555
if(BUILD_VENCODE)
  list(APPEND ADDITIONAL_MAKE_CLEAN_FILES_LIST "${CNSTREAM_ROOT_DIR}/3rdparty/live555")
  list(APPEND DEPENDENCIES live555)

  include_directories(${CNSTREAM_ROOT_DIR}/3rdparty/live555/include/BasicUsageEnvironment)
  include_directories(${CNSTREAM_ROOT_DIR}/3rdparty/live555/include/groupsock)
  include_directories(${CNSTREAM_ROOT_DIR}/3rdparty/live555/include/liveMedia)
  include_directories(${CNSTREAM_ROOT_DIR}/3rdparty/live555/include/UsageEnvironment)
  set(Live555_LIBS ${CNSTREAM_ROOT_DIR}/3rdparty/live555/lib/libliveMedia.so
	           ${CNSTREAM_ROOT_DIR}/3rdparty/live555/lib/libUsageEnvironment.so
		   ${CNSTREAM_ROOT_DIR}/3rdparty/live555/lib/libBasicUsageEnvironment.so
		   ${CNSTREAM_ROOT_DIR}/3rdparty/live555/lib/libgroupsock.so)
  list(APPEND 3RDPARTY_LIBS ${Live555_LIBS})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_LIVE555 -DNO_OPENSSL")
endif()

if(BUILD_INFERENCE)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../inference/src)
  file(GLOB_RECURSE test_infer_srcs ${CMAKE_CURRENT_SOURCE_DIR}/inference/*.cpp)
  file(GLOB preprocess ${CMAKE_CURRENT_SOURCE_DIR}/../cnstream_preproc.cpp)
  file(GLOB postrocess ${CMAKE_CURRENT_SOURCE_DIR}/../cnstream_postproc.cpp)
  file(GLOB_RECURSE preproc_infer ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/common/preprocess/*.cpp)
  file(GLOB_RECURSE postproc_infer ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/common/postprocess/*.cpp)
  list(APPEND test_srcs ${test_infer_srcs} ${preproc_infer} ${postproc_infer} ${preprocess} ${postrocess})
endif()

if(BUILD_OSD)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../osd/src)
  file(GLOB_RECURSE test_osd_srcs ${CMAKE_CURRENT_SOURCE_DIR}/osd/*.cpp)
  list(APPEND test_srcs ${test_osd_srcs})
endif()

if(BUILD_SOURCE)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../source/src)
  file(GLOB_RECURSE test_source_srcs ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
  list(APPEND test_srcs ${test_source_srcs})
endif()

if(BUILD_TRACK)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../track/src)
  file(GLOB_RECURSE test_track_srcs ${CMAKE_CURRENT_SOURCE_DIR}/track/*.cpp)
  list(APPEND test_srcs ${test_track_srcs})
endif()

add_executable(cnstream_va_test ${test_srcs})
add_dependencies(cnstream_va_test cnstream_va gtest)
target_link_libraries(cnstream_va_test gtest dl ${3RDPARTY_LIBS} pthread rt )
add_test(cnstream_va_test ${EXECUTABLE_OUTPUT_PATH}/cnstream_va_test)
