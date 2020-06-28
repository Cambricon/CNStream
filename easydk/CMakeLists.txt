#######################################################################
# -- Build Flags

option(WITH_CODEC "Build toolkit codec" ON)
option(WITH_INFER "Build toolkit infer" ON)
option(WITH_TRACKER "Build toolkit tracker" ON)
option(WITH_BANG "Build Bang operator" ON)
option(ENABLE_KCF "Build with KCF track" ON)
option(WITH_CNPLUGIN "Build CNPlugin operator" OFF)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#######################################################################

set(MLU_LIBS_PATH $ENV{NEUWARE_HOME}/lib64)

# ---[ cnrt
list(APPEND CNRT_LIBS "${MLU_LIBS_PATH}/libcnrt.so")

# ---[ cncodec & kcf
if(WITH_CODEC)
  list(APPEND CNCODEC_LIBS "${MLU_LIBS_PATH}/libcncodec.so")
endif()
if(WITH_TRACKER)
  list(APPEND KCF_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/src/easytrack/kcf/libkcf_mlu270.a")
endif()

if(WITH_CNPLUGIN)
  # ---[ cnml
  list(APPEND CNML_LIBS "${MLU_LIBS_PATH}/libcnml.so")
  # ---[ cnplugin
  list(APPEND CNPLUGIN_LIBS "${MLU_LIBS_PATH}/libcnplugin.so")
endif()

# ---[ Includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include
                    ${PROJECT_SOURCE_DIR}/mlu/${MLU_PLATFORM}/include)


# ---[ Sources
file(GLOB cxxutil_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/cxxutil/*.cpp)

if(WITH_INFER)
  message(STATUS "Build with EasyInfer")
  file(GLOB infer_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easyinfer/*.cpp)
endif()

if(WITH_CODEC)
  message(STATUS "Build with EasyCodec")
  file(GLOB codec_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easycodec/*.cpp)
endif()

if(WITH_TRACKER AND WITH_INFER)
  message(STATUS "Build with EasyTrack")
  file(GLOB_RECURSE track_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easytrack/*.cpp)
  if(ENABLE_KCF)
    add_definitions(-DENABLE_KCF)
  endif()
endif()

if(WITH_BANG AND WITH_INFER)
  message(STATUS "Build with EasyBang")
  file(GLOB_RECURSE bang_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easybang/resize_and_convert/kernel_*.o ${CMAKE_CURRENT_SOURCE_DIR}/src/easybang/resize_and_convert/*.cpp)
endif()

if(WITH_CNPLUGIN)
  file(GLOB_RECURSE plugin_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/easyplugin/*.cpp)
endif()

message(STATUS "Target : easydk")
add_library(easydk SHARED ${cxxutil_srcs} ${infer_srcs} ${codec_srcs} ${bang_srcs}
                                    ${track_srcs} ${plugin_srcs})

target_link_libraries(easydk ${CNRT_LIBS} ${CNCODEC_LIBS} ${CNML_LIBS} ${CNPLUGIN_LIBS} ${KCF_LIBS} ${GlogLib})
install(TARGETS easydk LIBRARY DESTINATION lib)

