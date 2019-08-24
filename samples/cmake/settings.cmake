set(Example_DIR "${PROJECT_SOURCE_DIR}/samples")

# ---[ Google-gflags
include("${PROJECT_SOURCE_DIR}/cmake/FindGFlags.cmake")
list(APPEND Example_INCLUDE_DIRS ${GFLAGS_INCLUDE_DIRS})
list(APPEND Example_LINKER_3RDPARTY_LIBS ${GFLAGS_LIBRARIES})

# ---[ Google-glog
include("${PROJECT_SOURCE_DIR}/cmake/FindGlog.cmake")
list(APPEND Example_INCLUDE_DIRS ${GLOG_INCLUDE_DIRS})
list(APPEND Example_LINKER_3RDPARTY_LIBS ${GLOG_LIBRARIES})

# ---[ Google-gtest
list(APPEND Example_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/3rdparty/googletest/include/)

# ---[ OpenCV
find_package(OpenCV REQUIRED)
list(APPEND Example_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS})
list(APPEND Example_LINKER_3RDPARTY_LIBS ${OpenCV_LIBS})

# ---[ thread
list(APPEND Example_LINKER_3RDPARTY_LIBS pthread dl)

include(FindPkgConfig)

if(NOT DEFINED ENV{NEUWARE_HOME})
  set(ENV{NEUWARE_HOME} /usr/local/neuware)
endif()
include_directories("$ENV{NEUWARE_HOME}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/decode/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/inference/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/osd/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/source/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../modules/encode/include")

link_directories("$ENV{NEUWARE_HOME}/lib64")
link_directories("${Example_DIR}/../lib")
link_directories("${Example_DIR}/../mlu/${CMAKE_SYSTEM_PROCESSOR}/libs/")

list(APPEND Example_INCLUDE_DIRS "${Example_DIR}/../include")
list(APPEND Example_INCLUDE_DIRS "${Example_DIR}/../modules/core/include")
list(APPEND Example_INCLUDE_DIRS "${Example_DIR}/../mlu/${CMAKE_SYSTEM_PROCESSOR}/include/")
list(APPEND Example_LINKER_TOOLKIT_LIBS cnbase cnpreproc cnpostproc cndecode cninfer cnosd cncodec cnrt)
list(APPEND Example_LINKER_LIBS cnstream cns-decode cns-inference cns-encoder)
list(APPEND Example_LINKER_LIBS cns-osd cns-source)
list(APPEND Example_LINKER_LIBS ${Example_LINKER_3RDPARTY_LIBS} ${Example_LINKER_TOOLKIT_LIBS})
