cmake_minimum_required(VERSION 2.8.7)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

set(CNSTREAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(NOT COMPILER_SUPPORTS_CXX11)
  message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

if(USE_libstdcpp)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
  message("-- Warning: forcing libstdc++ (controlled by USE_libstdcpp option in cmake)")
endif()

# compile flags
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-as-needed")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D_REENTRANT -fPIC -Wno-deprecated-declarations -Wall -Werror")

set(LIBRARY_OUTPUT_PATH ${CNSTREAM_ROOT_DIR}/lib)

set(contrib_modules_list fakesink)

# ---[ cnstream_va
include_directories(${CNSTREAM_ROOT_DIR}/modules)
include_directories(${CNSTREAM_ROOT_DIR}/modules/util/include)

foreach(contrib_module ${contrib_modules_list})
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/${contrib_module}/include)
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${contrib_module}/include/ DESTINATION include)
  file(GLOB_RECURSE contrib_module_src ${CMAKE_CURRENT_SOURCE_DIR}/${contrib_module}/*.cpp)
  list(APPEND contrib_module_srcs ${contrib_module_src})
endforeach()

if(DEFINED contrib_modules_list)
  include(${CNSTREAM_ROOT_DIR}/cmake/have_cnstream_target.cmake)
  have_framework_target(${CNSTREAM_ROOT_DIR})
  have_modules_target(${CNSTREAM_ROOT_DIR})
  add_library(cnstream_contrib SHARED ${contrib_module_srcs})
  set_target_properties(cnstream_contrib PROPERTIES LINK_FLAGS_RELEASE -s)
  if(HAVE_FRAMEWORK_TARGET)
    add_dependencies(cnstream_contrib cnstream_core)
  endif()
  if(HAVE_MODULES_TARGET)
    add_dependencies(cnstream_contrib cnstream_va)
  endif()
  target_link_libraries(cnstream_contrib cnstream_va)
  install(TARGETS cnstream_contrib LIBRARY DESTINATION lib)
endif()
