cmake_minimum_required(VERSION 2.8.7)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

set(CNSTREAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# compile flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D_REENTRANT -fPIC -Wno-deprecated-declarations -Wall -Werror")

set(CMAKE_MODULE_PATH ${CNSTREAM_ROOT_DIR}/cmake)
# ---[ neuware
find_package(Neuware)
include_directories(${NEUWARE_INCLUDE_DIR})

# ---[ pybind11
set(PYBIND11_PYTHON_VERSION 3)
add_subdirectory(${CNSTREAM_ROOT_DIR}/3rdparty/pybind11 ${CMAKE_CURRENT_BINARY_DIR}/pybind11)

# ---[ rapidjson
include_directories(${CNSTREAM_ROOT_DIR}/3rdparty/rapidjson/include)

# ---[ opencv
set(OpenCV_FIND_QUIETLY true)
#find_package(OpenCV REQUIRED) //this will include opencv_ts which result in crash on centos
find_package(OpenCV REQUIRED core imgproc highgui features2d)
if(OpenCV_FOUND)
  include_directories(${OpenCV_INCLUDE_DIRS})
  message(STATUS "opencv include: ${OpenCV_INCLUDE_DIRS}")
  message(STATUS "opencv libraries: ${OpenCV_LIBS}")
else()
  message(FATAL_ERROR "opencv not found!")
endif()

set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/)

include_directories(${CNSTREAM_ROOT_DIR}/modules)
include_directories(${CNSTREAM_ROOT_DIR}/framework/core/include)
include_directories(${CNSTREAM_ROOT_DIR}/samples/common)
include_directories(${CNSTREAM_ROOT_DIR}/modules/core/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

list(APPEND module_list source inference inference2 util)
foreach(module ${module_list})
  include_directories(${CNSTREAM_ROOT_DIR}/modules/${module}/include)
endforeach()

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/ cnstream_pyapi_srcs)
aux_source_directory(${CNSTREAM_ROOT_DIR}/samples/common/ sample_common_srcs)
aux_source_directory(${CNSTREAM_ROOT_DIR}/samples/common/preprocess postprocess_srcs)
aux_source_directory(${CNSTREAM_ROOT_DIR}/samples/common/postprocess preprocess_srcs)

include_directories(${CNSTREAM_ROOT_DIR}/3rdparty/pybind11/include)
include(${CNSTREAM_ROOT_DIR}/cmake/have_cnstream_target.cmake)
have_framework_target(${CNSTREAM_ROOT_DIR})
have_easydk_target(${CNSTREAM_ROOT_DIR})
have_modules_target(${CNSTREAM_ROOT_DIR})
have_modules_contrib_target(${CNSTREAM_ROOT_DIR})

pybind11_add_module(cnstream SHARED ${cnstream_pyapi_srcs} ${preprocess_srcs} ${postprocess_srcs} ${sample_common_srcs})
find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_DIR})
# message(${PYTHON_INCLUDE_DIR})
# message(${PYTHON_LIBRARY})

if(HAVE_FRAMEWORK_TARGET)
  add_dependencies(cnstream cnstream_core)
endif()
if(HAVE_EASYDK_TARGET)
  add_dependencies(cnstream easydk)
endif()
if(HAVE_MODULES_TARGET)
  add_dependencies(cnstream cnstream_va)
endif()
if(HAVE_MODULES_CONTRIB_TARGET)
  add_dependencies(cnstream cnstream_contrib)
endif()

target_link_libraries(cnstream PRIVATE cnstream_va cnstream_core ${OpenCV_LIBS})

if(HAVE_MODULES_CONTRIB)
  set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")
  target_link_libraries(cnstream PRIVATE cnstream_contrib)
endif()

if(build_tests)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
  aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/test cnstream_pyapi_cpptest_srcs)
  pybind11_add_module(cnstream_cpptest MODULE ${cnstream_pyapi_cpptest_srcs})
  add_dependencies(cnstream_cpptest cnstream)
  target_link_libraries(cnstream_cpptest PRIVATE cnstream)
endif()

