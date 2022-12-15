# ---[ gtest
if(NOT build_gtest_already)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/googletest ${PROJECT_BINARY_DIR}/googletest)
  set(build_gtest_already ON)
endif()

# ---[ gflags
include(${CNSTREAM_ROOT_DIR}/cmake/FindGFlags.cmake)
include_directories(${GFLAGS_INCLUDE_DIRS})

# ---[ glog
include(${CNSTREAM_ROOT_DIR}/cmake/FindGlog.cmake)
include_directories(${GLOG_INCLUDE_DIRS})

file(GLOB core_test_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE common_srcs ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp)

add_executable(cnstream_core_test ${core_test_srcs} ${common_srcs})
target_include_directories(cnstream_core_test PUBLIC
                           ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/googletest/googletest/include
                           ${GLOG_INCLUDE_DIRS}
                           ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(cnstream_core_test gtest dl cnstream_core ${CNRT_LIBS} pthread rt ${GFLAGS_LIBRARIES} ${GLOG_LIBRARIES})
add_test(cnstream_core_test ${EXECUTABLE_OUTPUT_PATH}/cnstream_core_test)
