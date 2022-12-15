cmake_minimum_required(VERSION 2.8.7)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

set(CNSTREAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(LIBRARY_OUTPUT_PATH ${CNSTREAM_ROOT_DIR}/lib)

set(contrib_modules_list fakesink selector)

include_directories(${CNSTREAM_ROOT_DIR}/modules)
include_directories(${CNSTREAM_ROOT_DIR}/modules/util/include)
# ---[ cnstream_va
have_modules_target(${CNSTREAM_ROOT_DIR})
# ---[ easydk
have_easydk_target(${CNSTREAM_ROOT_DIR})


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
  message(STATUS "@@@@@@@@@@@ Target : cnstream_contrib")
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
