if(build_tests)
  include_directories(${PROJECT_SOURCE_DIR}/framework/unitest/include/)
  include_directories(${PROJECT_SOURCE_DIR}/framework/core/src)

  file(GLOB_RECURSE test_core_srcs ${PROJECT_SOURCE_DIR}/framework/unitest/core/*.cpp)
  set(test_srcs ${test_core_srcs})
  list(APPEND test_srcs ${PROJECT_SOURCE_DIR}/framework/unitest/test_base.cpp)
  list(APPEND test_srcs ${PROJECT_SOURCE_DIR}/framework/unitest/test_main.cpp)
  
  include_directories(${PROJECT_SOURCE_DIR}/framework/metrics/src)
  file(GLOB_RECURSE test_metrics_srcs ${PROJECT_SOURCE_DIR}/framework/unitest/metrics/*.cpp)
  list(APPEND test_srcs ${test_metrics_srcs})
  
  add_executable(cnstream_core_test ${test_srcs})

  target_link_libraries(cnstream_core_test gtest dl cnstream_core ${3RDPARTY_LIBS} pthread rt)

  add_test(cnstream_core_test ${EXECUTABLE_OUTPUT_PATH}/cnstream_core_test)
endif()
