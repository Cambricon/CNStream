# ---[ gtest
if(NOT build_gtest_already)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/googletest ${PROJECT_BINARY_DIR}/googletest)
  set(build_gtest_already ON)
endif()
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/googletest/googletest/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../core/src)

# ---[ gflags
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FindGFlags.cmake)
include_directories(${GFLAGS_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/core core_test_srcs)
list(APPEND core_test_srcs ${CMAKE_CURRENT_SOURCE_DIR}/test_base.cpp)
list(APPEND core_test_srcs ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp)

add_executable(cnstream_core_test ${core_test_srcs})
add_dependencies(cnstream_core_test cnstream_core gtest)
target_link_libraries(cnstream_core_test gtest dl cnstream_core pthread rt ${GFLAGS_LIBRARIES})
add_test(cnstream_core_test ${EXECUTABLE_OUTPUT_PATH}/cnstream_core_test)
