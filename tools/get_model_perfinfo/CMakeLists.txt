cmake_minimum_required(VERSION 2.8.7)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

# compile flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D_REENTRANT -fPIC -Wno-deprecated-declarations -Wall -Werror")

set(CNSTREAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CNSTREAM_TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../)

set(EXECUTABLE_OUTPUT_PATH ${CNSTREAM_TOOLS_DIR}/bin/)

# ---[ Google-gflags
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FindGFlags.cmake")
include_directories(${GFLAGS_INCLUDE_DIRS})

set(CMAKE_MODULE_PATH ${CNSTREAM_ROOT_DIR}/cmake)
# ---[ neuware
find_package(Neuware)
include_directories(${NEUWARE_INCLUDE_DIR})

# ---[ easydk
include(${CNSTREAM_ROOT_DIR}/cmake/have_cnstream_target.cmake)
have_easydk_target(${CNSTREAM_ROOT_DIR})

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")
add_executable(get_model_perfinfo get_model_perfinfo.cpp)

if(HAVE_EASYDK_TARGET)
  add_dependencies(get_model_perfinfo easydk)
endif()

target_link_libraries(get_model_perfinfo easydk ${CNCODEC_LIBS} ${CNRT_LIBS} ${GFLAGS_LIBRARIES} pthread dl)
