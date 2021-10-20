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

# compile flags
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-as-needed")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D_REENTRANT -fPIC -Wall -Werror")

set(CNSTREAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(LIBRARY_OUTPUT_PATH ${CNSTREAM_ROOT_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin/)

set(CMAKE_MODULE_PATH ${CNSTREAM_ROOT_DIR}/cmake)
# ---[ neuware
find_package(Neuware)
include_directories(${NEUWARE_INCLUDE_DIR})

# ---[ Google-gflags
include(${CNSTREAM_ROOT_DIR}/cmake/FindGFlags.cmake)
include_directories(${GFLAGS_INCLUDE_DIRS})

# ---[ rapidjson
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/rapidjson/include)

set(VERSION_HEAD_FILE ${CMAKE_CURRENT_SOURCE_DIR}/core/include/cnstream_version.hpp)
execute_process(COMMAND awk /CNSTREAM_MAJOR_VERSION/ ${VERSION_HEAD_FILE} OUTPUT_VARIABLE VERSION_STR)
string(REGEX MATCH "[0-9]+"  MAJOR_VERSION ${VERSION_STR})
execute_process(COMMAND awk /CNSTREAM_MINOR_VERSION/ ${VERSION_HEAD_FILE} OUTPUT_VARIABLE VERSION_STR)
string(REGEX MATCH "[0-9]+"  MINOR_VERSION ${VERSION_STR})
execute_process(COMMAND awk /CNSTREAM_PATCH_VERSION/ ${VERSION_HEAD_FILE} OUTPUT_VARIABLE VERSION_STR)
string(REGEX MATCH "[0-9]+"  PATCH_VERSION ${VERSION_STR})
set(CNSTREAM_TARGET_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")

# ---[ source
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core/include)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/core/src core_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/core/src/profiler profiler_srcs)

if(build_tests)
  add_definitions(-DUNIT_TEST)
endif()

add_library(cnstream_core SHARED ${core_srcs} ${profiler_srcs})
target_link_libraries(cnstream_core dl pthread ${CNRT_LIBS} ${GFLAGS_LIBRARIES})
set_target_properties(cnstream_core PROPERTIES LINK_FLAGS_RELEASE -s)
set_target_properties(cnstream_core PROPERTIES VERSION ${CNSTREAM_TARGET_VERSION})

# install
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")  #set runtime path
install(TARGETS cnstream_core LIBRARY DESTINATION lib)
install(DIRECTORY core/include/ DESTINATION include)

if(build_tests)
  add_subdirectory(unitest)
endif()
