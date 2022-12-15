cmake_minimum_required(VERSION 3.5)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

set(CNSTREAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SAMPLES_ROOT_DIR ${CNSTREAM_ROOT_DIR}/samples)

set(CMAKE_MODULE_PATH ${CNSTREAM_ROOT_DIR}/cmake/sanitizers ${CNSTREAM_ROOT_DIR}/cmake)
find_package(Sanitizers)

set(3RDPARTY_LIBS "")

# ---[ rapidjson
include_directories(${CNSTREAM_ROOT_DIR}/3rdparty/rapidjson/include)

# ---[ opencv
set(OpenCV_FIND_QUIETLY true)
#find_package(OpenCV REQUIRED) //this will include opencv_ts which result in crash on centos
find_package(OpenCV OPTIONAL_COMPONENTS videoio)
set(optional_opencv_libs ${OpenCV_LIBS})
find_package(OpenCV REQUIRED core imgproc highgui features2d)
if(OpenCV_FOUND)
  if(imgcodecs_FOUND)
    list(APPEND OpenCV_LIBS optional_opencv_libs)
  endif()
  include_directories(${OpenCV_INCLUDE_DIRS})
  list(APPEND 3RDPARTY_LIBS ${OpenCV_LIBS})
  message(STATUS "opencv include: ${OpenCV_INCLUDE_DIRS}")
  message(STATUS "opencv libraries: ${OpenCV_LIBS}")
else()
  message(FATAL_ERROR "opencv not found!")
endif()

# ---[ ffmpeg
find_package(FFmpeg REQUIRED)
if (FFMPEG_FOUND)
  include_directories(${FFMPEG_INCLUDE_DIR})
  set(HAVE_FFMPEG true)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_FFMPEG")
  if (WITH_FFMPEG_AVDEVICE)
      add_definitions(-DHAVE_FFMPEG_AVDEVICE)
  endif ()
  message(STATUS "ffmpeg include: ${FFMPEG_INCLUDE_DIR}")
  message(STATUS "ffmpeg libraries: ${FFMPEG_LIBRARIES}")
else ()
  message(FATAL_ERROR "ffmpeg not found!")
endif ()
list(APPEND 3RDPARTY_LIBS ${FFMPEG_LIBRARIES})

# ---[ libyuv
include_directories(${CNSTREAM_ROOT_DIR}/3rdparty/libyuv/include)
link_directories(${CNSTREAM_ROOT_DIR}/3rdparty/libyuv/lib)
list(APPEND 3RDPARTY_LIBS ${CNSTREAM_ROOT_DIR}/3rdparty/libyuv/lib/libyuv.a)

# ---[ gflags
include(${CNSTREAM_ROOT_DIR}/cmake/FindGFlags.cmake)
include_directories(${GFLAGS_INCLUDE_DIRS})
list(APPEND 3RDPARTY_LIBS ${GFLAGS_LIBRARIES})

# ---[ glog
include(${CNSTREAM_ROOT_DIR}/cmake/FindGlog.cmake)
include_directories(${GLOG_INCLUDE_DIRS})
list(APPEND 3RDPARTY_LIBS ${GLOG_LIBRARIES})

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
if(BUILD_SOURCE OR BUILD_VENCODE)
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

# ---[ modules include dirs
include_directories(${CNSTREAM_ROOT_DIR}/modules)
include_directories(${CNSTREAM_ROOT_DIR}/modules/util/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/inference/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/osd/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/source/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/encode/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/track/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/kafka/include)

# ---[ current include dirs
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)

# ---[ sources
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common common_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/preprocess preprocess_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/postprocess postprocess_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/filter filter_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/cns_openpose cns_openpose_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/osd_handler cns_osd_handler_srcs)
if(BUILD_KAFKA)
  aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/kafka_handler kafka_handler_srcs)
endif()
include(${CNSTREAM_ROOT_DIR}/cmake/have_cnstream_target.cmake)

# ---[ dependencies
set(SAMPLES_DEPENDENCIES "")
have_framework_target(${CNSTREAM_ROOT_DIR})
have_easydk_target(${CNSTREAM_ROOT_DIR})
have_modules_target(${CNSTREAM_ROOT_DIR})
# have_modules_contrib_target(${CNSTREAM_ROOT_DIR})
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

# ---[ add targets
set(EXECUTABLE_OUTPUT_PATH ${SAMPLES_ROOT_DIR}/bin)
add_executable(cns_launcher ${srcs} ${common_srcs} ${preprocess_srcs} ${postprocess_srcs} ${filter_srcs}
                            ${cns_openpose_srcs} ${cns_osd_handler_srcs} ${kafka_handler_srcs})
add_sanitizers(cns_launcher)
if(NOT "${SAMPLES_DEPENDENCIES} " STREQUAL " ")
  add_dependencies(cns_launcher ${SAMPLES_DEPENDENCIES})
endif()
target_link_libraries(cns_launcher cnstream_va cnstream_core easydk ${CNRT_LIBS} ${CNDRV_LIBS} ${3RDPARTY_LIBS}
                      pthread dl)

if(HAVE_MODULES_CONTRIB)
  set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")
  target_link_libraries(cns_launcher cnstream_contrib)
endif()
