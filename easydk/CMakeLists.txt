cmake_minimum_required(VERSION 2.8.7)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

# ---[ CNStream-Toolkit project
project(CNStream-Toolkit C CXX)

#######################################################################
# -- Build Flags

option(BUILD_TESTS "Build all of modules' unit-tests" OFF)
option(BUILD_SAMPLES "Build samples" OFF)
option(RELEASE "Build type" ON)
option(CODE_COVERAGE_TEST "Build code coverage test" OFF)

option(WITH_CODEC "Build toolkit codec" ON)
option(WITH_INFER "Build toolkit infer" ON)
option(WITH_TRACKER "Build toolkit tracker" ON)
option(WITH_BANG "Build Bang operator" ON)
option(ENABLE_KCF "Build with KCF track" ON)
option(WITH_CNPLUGIN "Build CNPlugin operator" OFF)

option(SANITIZE_MEMORY "Enable MemorySanitizer for sanitized targets." OFF)
option(SANITIZE_ADDRESS "Enable AddressSanitizer for sanitized targets." OFF)
option(SANITIZE_THREAD "Enable ThreadSanitizer for sanitized targets." OFF)
option(SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer for sanitized targets." OFF)

if (RELEASE)
  set(CMAKE_BUILD_TYPE RELEASE)
else()
  set(CMAKE_BUILD_TYPE DEBUG)
endif()

set(CMAKE_CXX_FLAGS "-fPIC -Wall -Werror -std=c++11")
if(CODE_COVERAGE_TEST)
  message(STATUS "Build with code coverage test")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -g")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3")

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

message("----------------------------- BUILD ${CMAKE_BUILD_TYPE} -----------------------------")

if(USE_libstdcpp)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
  message(WARNING "Warning: forcing libstdc++ (controlled by USE_libstdcpp option in cmake)")
endif()

if (SANITIZE_MEMORY OR SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_UNDEFINED) 
  set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
  find_package(Sanitizers)
endif()

#######################################################################

# neuware include/libs
if(NOT DEFINED ENV{NEUWARE_HOME})
  set(ENV{NEUWARE_HOME} /usr/local/neuware)
endif()

set(MLU_PLATFORM MLU270)
#cp neuware/.h or .so for cnstream-toolkit
execute_process(COMMAND mkdir -p ${PROJECT_SOURCE_DIR}/mlu/${MLU_PLATFORM}/include)
execute_process(COMMAND mkdir -p ${PROJECT_SOURCE_DIR}/mlu/${MLU_PLATFORM}/libs/${CMAKE_SYSTEM_PROCESSOR})
execute_process(COMMAND sh ${PROJECT_SOURCE_DIR}/tools/copy_neuware_mlu270.sh $ENV{NEUWARE_HOME} ${PROJECT_SOURCE_DIR}/mlu/${MLU_PLATFORM}/include  ${PROJECT_SOURCE_DIR}/mlu/${MLU_PLATFORM}/libs/${CMAKE_SYSTEM_PROCESSOR} ${WITH_CNPLUGIN})
message(STATUS "MLU270 neuware files link DONE")

set(MLU_LIBS_PATH $ENV{NEUWARE_HOME}/lib64)

# ---[ cnrt
list(APPEND CNRT_LIBS "${MLU_LIBS_PATH}/libcnrt.so")

# ---[ cncodec & kcf
if(WITH_CODEC)
  list(APPEND CNCODEC_LIBS "${MLU_LIBS_PATH}/libcncodec.so")
endif()
if(WITH_TRACKER)
  list(APPEND KCF_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/src/easytrack/kcf/libkcf_mlu270.a")
endif()

# ---[ Google-glog
include("${PROJECT_SOURCE_DIR}/cmake/FindGlog.cmake")
# list(APPEND 3RDPARTY_INCLUDE_DIRS ${GLOG_INCLUDE_DIRS})
list(APPEND GlogLib ${GLOG_LIBRARIES})

if(WITH_CNPLUGIN)
  # ---[ cnml
  list(APPEND CNML_LIBS "${MLU_LIBS_PATH}/libcnml.so")
  # ---[ cnplugin
  list(APPEND CNPLUGIN_LIBS "${MLU_LIBS_PATH}/libcnplugin.so")
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

if(WITH_CNPLUGIN)
  file(GLOB_RECURSE plugin_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easyplugin/*.cpp)
endif()

message(STATUS "Target : cnstream-toolkit")
add_library(cnstream-toolkit SHARED ${cxxutil_srcs} ${infer_srcs} ${codec_srcs} ${bang_srcs}
                                    ${track_srcs} ${plugin_srcs})

if (SANITIZE_MEMORY OR SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_UNDEFINED)
  add_sanitizers(cnstream-toolkit)
endif()

target_link_libraries(cnstream-toolkit ${CNRT_LIBS} ${CNCODEC_LIBS} ${CNML_LIBS} ${CNPLUGIN_LIBS} ${KCF_LIBS} ${GlogLib})

if(BUILD_SAMPLES AND WITH_INFER AND WITH_CODEC AND WITH_TRACKER AND WITH_BANG)
  message(STATUS "Build samples")
  add_subdirectory(samples/stream-app)
endif()

if(BUILD_TESTS)
  message(STATUS "Build tests")
  add_subdirectory(3rdparty/googletest)
  add_subdirectory(tests)
  # This must be set in the root directory for the tests to be run by
  # 'make test' or ctest.
  enable_testing()
endif()
