#######################################################################
# -- Build Flags

option(RELEASE "Build type" ON)
option(CODE_COVERAGE_TEST "Build code coverage test" OFF)

option(WITH_CODEC "Build toolkit codec" ON)
option(WITH_INFER "Build toolkit infer" ON)
option(WITH_TRACKER "Build toolkit tracker" ON)
option(WITH_BANG "Build Bang operator" ON)
option(ENABLE_KCF "Build with KCF track" ON)

option(SANITIZE_MEMORY "Enable MemorySanitizer for sanitized targets." OFF)
option(SANITIZE_ADDRESS "Enable AddressSanitizer for sanitized targets." OFF)
option(SANITIZE_THREAD "Enable ThreadSanitizer for sanitized targets." OFF)
option(SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer for sanitized targets." OFF)

if (SANITIZE_MEMORY OR SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_UNDEFINED) 
  set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
  find_package(Sanitizers)
endif()

#######################################################################

set(MLU_LIBS_PATH $ENV{NEUWARE_HOME}/lib64)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib/) 

# ---[ cnrt
list(APPEND CNRT_LIBS "${MLU_LIBS_PATH}/libcnrt.so")

# ---[ cncodec & kcf
if(WITH_CODEC)
  list(APPEND CNCODEC_LIBS "${MLU_LIBS_PATH}/libcncodec.so" "${MLU_LIBS_PATH}/libcndrv.so")
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
  message(STATUS "Build with EasyInfer")
  file(GLOB infer_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easyinfer/*.cpp)
endif()

if(WITH_CODEC)
  message(STATUS "Build with EasyCodec")
  file(GLOB codec_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easycodec/*.cpp)
endif()

if(WITH_TRACKER AND WITH_INFER)
  message(STATUS "Build with EasyTrack")
  file(GLOB_RECURSE track_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easytrack/*.cpp)
  if(ENABLE_KCF)
    add_definitions(-DENABLE_KCF)
  endif()
endif()

if(WITH_BANG AND WITH_INFER)
  message(STATUS "Build with EasyBang")
  file(GLOB_RECURSE bang_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easybang/resize_and_convert/kernel_*.o ${CMAKE_CURRENT_SOURCE_DIR}/src/easybang/*.cpp)
  file(GLOB_RECURSE bang_srcs ${bang_srcs} ${CMAKE_CURRENT_SOURCE_DIR}/src/easybang/resize/ResizeKernel.o ${CMAKE_CURRENT_SOURCE_DIR}/src/easybang/*.cpp)
endif()

message(STATUS "Target : cnstream-toolkit")
add_library(cnstream-toolkit SHARED ${cxxutil_srcs} ${infer_srcs} ${codec_srcs} ${bang_srcs}
                                    ${track_srcs})

if (SANITIZE_MEMORY OR SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_UNDEFINED)
  add_sanitizers(cnstream-toolkit)
endif()

target_link_libraries(cnstream-toolkit ${CNRT_LIBS} ${CNCODEC_LIBS} ${KCF_LIBS})

