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

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../bin/)

# ---[ rapidjson
include_directories(${CNSTREAM_ROOT_DIR}/3rdparty/rapidjson/include)

# ---[ add target
include(${CNSTREAM_ROOT_DIR}/cmake/have_cnstream_target.cmake)
have_framework_target(${CNSTREAM_ROOT_DIR})
have_modules_target(${CNSTREAM_ROOT_DIR})
have_modules_contrib_target(${CNSTREAM_ROOT_DIR})

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")

add_executable(cnstream_inspect cnstream_inspect.cpp)
if(HAVE_FRAMEWORK_TARGET)
  add_dependencies(cnstream_inspect cnstream_core)
endif()
if(HAVE_MODULES_TARGET)
  add_dependencies(cnstream_inspect cnstream_va)
endif()
if(HAVE_MODULES_CONTRIB_TARGET)
  add_dependencies(cnstream_inspect cnstream_contrib)
endif()

if(HAVE_MODULES_CONTRIB)
  target_link_libraries(cnstream_inspect cnstream_contrib)
endif()
target_link_libraries(cnstream_inspect cnstream_va cnstream_core pthread dl)


