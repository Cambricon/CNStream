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

# ---[ librdkafka
if(build_kafka)
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
include_directories(${CNSTREAM_ROOT_DIR}/modules/kafka/include)
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
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/preprocess preprocess_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/postprocess postprocess_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/obj_filter obj_filter_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/cns_openpose cns_openpose_srcs)
if(build_kafka)
  aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../common/kafka_handler kafka_handler_srcs)
endif()

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
  list(APPEND SAMPLES_DEPENDENCIES easydk cnis)
endif()
if(HAVE_MODULES_TARGET)
  list(APPEND SAMPLES_DEPENDENCIES cnstream_va)
endif()
if(HAVE_MODULES_CONTRIB_TARGET)
  list(APPEND SAMPLES_DEPENDENCIES cnstream_contrib)
endif()

# ---[ add targets
set(EXECUTABLE_OUTPUT_PATH ${SAMPLES_ROOT_DIR}/bin)
add_executable(cns_launcher ${srcs} ${common_srcs} ${preprocess_srcs} ${postprocess_srcs} ${obj_filter_srcs} ${cns_openpose_srcs} ${kafka_handler_srcs})
add_sanitizers(cns_launcher)
if(NOT "${SAMPLES_DEPENDENCIES} " STREQUAL " ")
  add_dependencies(cns_launcher ${SAMPLES_DEPENDENCIES})
endif()
target_link_libraries(cns_launcher cnstream_va cnstream_core easydk cnis ${OpenCV_LIBS} ${GFLAGS_LIBRARIES} pthread dl)

if(HAVE_MODULES_CONTRIB)
  set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")
  target_link_libraries(cns_launcher cnstream_contrib)
endif()
