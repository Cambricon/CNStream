cmake_minimum_required(VERSION 2.8.7)

find_package(OpenCV REQUIRED)
# ---[ Google-gflags
include("${PROJECT_SOURCE_DIR}/cmake/FindGFlags.cmake")

AUX_SOURCE_DIRECTORY(./common DIR_SRCS_common)
AUX_SOURCE_DIRECTORY(./image DIR_SRCS_image)
AUX_SOURCE_DIRECTORY(./demo DIR_SRCS_demo)

include_directories(
../include
./image
./common
)

link_directories("${PROJECT_BINARY_DIR}/3rdparty/lib/")
add_executable(deepsort_demo
${DIR_SRCS_common}
${DIR_SRCS_image}
${DIR_SRCS_demo}
)
target_link_libraries(deepsort_demo deepsort ${OpenCV_LIBS} ${GFLAGS_LIBRARIES} "pthread")

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
  message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()
