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

#######################################################################
# find Glog
set(GLOG_ROOT_DIR "" CACHE PATH "Folder contains Google glog")

if(WIN32)
    find_path(GLOG_INCLUDE_DIR glog/logging.h
        PATHS ${GLOG_ROOT_DIR}/src/windows)
else()
    find_path(GLOG_INCLUDE_DIR glog/logging.h
        PATHS ${GLOG_ROOT_DIR})
endif()

if(MSVC)
    find_library(GLOG_LIBRARY_RELEASE libglog_static
        PATHS ${GLOG_ROOT_DIR}
        PATH_SUFFIXES Release)

    find_library(GLOG_LIBRARY_DEBUG libglog_static
        PATHS ${GLOG_ROOT_DIR}
        PATH_SUFFIXES Debug)

    set(GLOG_LIBRARY optimized ${GLOG_LIBRARY_RELEASE} debug ${GLOG_LIBRARY_DEBUG})
else()
    find_library(GLOG_LIBRARY glog
        PATHS ${GLOG_ROOT_DIR}
        PATH_SUFFIXES lib lib64)
endif()

find_package_handle_standard_args(Glog DEFAULT_MSG GLOG_INCLUDE_DIR GLOG_LIBRARY)

if(GLOG_FOUND)
  set(GLOG_INCLUDE_DIRS ${GLOG_INCLUDE_DIR})
  set(GLOG_LIBRARIES ${GLOG_LIBRARY})
  message(STATUS "Found glog    (include: ${GLOG_INCLUDE_DIR}, library: ${GLOG_LIBRARY})")
  mark_as_advanced(GLOG_ROOT_DIR GLOG_LIBRARY_RELEASE GLOG_LIBRARY_DEBUG
                                 GLOG_LIBRARY GLOG_INCLUDE_DIR)
endif()

######################################################################

list(APPEND LINK_LIBS cnstream-toolkit ${GLOG_LIBRARIES} ${OpenCV_LIBS} pthread dl gtest gtest_main)
file(GLOB test_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(tests ${test_srcs})

target_link_libraries(tests ${LINK_LIBS})
target_include_directories(tests PRIVATE
                           ${GLOG_INCLUDE_DIRS}
                           ${CMAKE_CURRENT_SOURCE_DIR}/googletest/include
                           ${OpenCV_INCLUDE_DIRS})
