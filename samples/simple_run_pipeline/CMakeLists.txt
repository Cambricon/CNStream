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

if(USE_libstdcpp)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
  message("-- Warning: forcing libstdc++ (controlled by USE_libstdcpp option in cmake)")
endif()

# compile flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D_REENTRANT -fPIC -Wall -Werror")

set(CNSTREAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SAMPLES_ROOT_DIR ${CNSTREAM_ROOT_DIR}/samples)

set(CMAKE_MODULE_PATH ${CNSTREAM_ROOT_DIR}/cmake)
# ---[ neuware
find_package(Neuware)
include_directories(${NEUWARE_INCLUDE_DIR})

# ---[ modules
include_directories(${CNSTREAM_ROOT_DIR}/modules)
include_directories(${CNSTREAM_ROOT_DIR}/modules/inference/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/util/include)
include_directories(${CNSTREAM_ROOT_DIR}/modules/source/include)
include(${CNSTREAM_ROOT_DIR}/cmake/have_cnstream_target.cmake)
have_framework_target(${CNSTREAM_ROOT_DIR})
have_easydk_target(${CNSTREAM_ROOT_DIR})
have_modules_target(${CNSTREAM_ROOT_DIR})

# ---[ gflags
include(${CNSTREAM_ROOT_DIR}/cmake/FindGFlags.cmake)
include_directories(${GFLAGS_INCLUDE_DIRS})

# ---[ Opencv
find_package(OpenCV REQUIRED core imgproc highgui features2d)
if(OpenCV_FOUND)
  include_directories(${OpenCV_INCLUDE_DIRS})
  message(STATUS "opencv include: ${OpenCV_INCLUDE_DIRS}")
  message(STATUS "opencv libraries: ${OpenCV_LIBS}")
else()
  message(FATAL_ERROR "opencv not found!")
endif()

# ---[ add target
set(EXECUTABLE_OUTPUT_PATH ${SAMPLES_ROOT_DIR}/bin)
add_executable(simple_run_pipeline simple_run_pipeline.cpp)
if(HAVE_FRAMEWORK_TARGET)
  add_dependencies(simple_run_pipeline cnstream_core)
endif()
if(HAVE_EASYDK_TARGET)
  add_dependencies(simple_run_pipeline easydk)
endif()
if(HAVE_MODULES_TARGET)
  add_dependencies(simple_run_pipeline cnstream_va)
endif()
target_link_libraries(simple_run_pipeline cnstream_va cnstream_core easydk ${GFLAGS_LIBRARIES} ${OpenCV_LIBS} pthread dl)
