# ---[ OpenCV
set(OpenCV_FIND_QUIETLY true)
find_package(OpenCV OPTIONAL_COMPONENTS imgcodecs)
set(imgcodecs_libs ${OpenCV_LIBS})
find_package(OpenCV REQUIRED core imgproc highgui features2d)
if(OpenCV_FOUND)
  message(STATUS "OpenCV Found")
else()
  message(FATAL_ERROR "Can not find OpenCV libs!")
endif()
if(imgcodecs_FOUND)
  list(APPEND OpenCV_LIBS ${imgcodecs_libs})
endif()

list(APPEND LINK_LIBS cnstream-toolkit ${OpenCV_LIBS} pthread dl gtest gtest_main)
file(GLOB test_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(tests ${test_srcs})

if (SANITIZE_MEMORY OR SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_UNDEFINED)
    add_sanitizers(tests)
endif()

target_link_libraries(tests ${LINK_LIBS})
target_include_directories(tests PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/googletest/include
                           ${OpenCV_INCLUDE_DIRS})
