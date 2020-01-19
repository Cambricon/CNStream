#######################################################################
# -- Build Flags

option(BUILD_TESTS "Build all of modules' unit-tests" ON)
option(BUILD_SAMPLES "Build samples" OFF)
option(RELEASE "Build type" ON)
option(CODE_COVERAGE_TEST "Build code coverage test" OFF)

option(WITH_CODEC "Build toolkit codec" ON)
option(WITH_INFER "Build toolkit infer" ON)
option(WITH_TRACKER "Build toolkit tracker" ON)
option(WITH_BANG "Build Bang operator" ON)

#######################################################################

set(MLU_LIBS_PATH ${PROJECT_SOURCE_DIR}/mlu/${MLU_PLATFORM}/libs/${CMAKE_SYSTEM_PROCESSOR})
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib/) 

# ---[ cnrt
list(APPEND CNRT_LIBS "${MLU_LIBS_PATH}/libcnrt.so")

# ---[ cncodec & kcf
if(WITH_CODEC)
  list(APPEND CNCODEC_LIBS "${MLU_LIBS_PATH}/libcncodec.so")
  list(APPEND CNCODEC_LIBS "${MLU_LIBS_PATH}/libmsgq.so")
  list(APPEND CNCODEC_LIBS "${MLU_LIBS_PATH}/librpc.so")
endif()
if(WITH_TRACKER)
  list(APPEND KCF_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/src/easytrack/kcf/libkcf_mlu270.a")
endif()

# ---[ Includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include
                    ${PROJECT_SOURCE_DIR}/mlu/${MLU_PLATFORM}/include)

# ---[ Sources
file(GLOB cxxutil_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/cxxutil/*.cpp)
if(WITH_INFER)
  file(GLOB infer_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easyinfer/*.cpp)
endif()
if(WITH_CODEC)
  file(GLOB codec_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easycodec/*.cpp)
endif()
if(WITH_TRACKER)
  file(GLOB_RECURSE track_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easytrack/*.cpp)
endif()

if(WITH_BANG)
  file(GLOB_RECURSE bang_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easybang/resize_and_convert/kernel_*.o ${CMAKE_CURRENT_SOURCE_DIR}/src/easybang/*.cpp)
endif()

message("target : cnstream-toolkit")
add_library(cnstream-toolkit SHARED ${cxxutil_srcs} ${infer_srcs} ${codec_srcs} ${bang_srcs}
                                    ${track_srcs})

target_link_libraries(cnstream-toolkit ${CNRT_LIBS} ${CNCODEC_LIBS} ${KCF_LIBS})

if(BUILD_SAMPLES AND WITH_INFER AND WITH_CODEC AND WITH_TRACKER AND WITH_BANG)
  message("Build samples")
  add_subdirectory(samples/stream-app)
endif()

if(BUILD_TESTS)
  message("Build tests")
  add_subdirectory(tests)
  # This must be set in the root directory for the tests to be run by
  # 'make test' or ctest.
  enable_testing()
endif()
