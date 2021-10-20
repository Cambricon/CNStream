# ==============================================
# Try to find Cambricon Neuware libraries:
# - cnrt (required)
# - cndrv (required)
# - ion (required on mlu220 edge)
# - cncodec (required)
# - cncodec_v3 (optional)
#
# SET NEUWARE_INCLUDE_DIR with neuware include directory
# SET CNRT_LIBS with cnrt path and cndrv path
# SET CNCODEC_LIBS with cncodec path and ion(if has) path
# SET CNCODECV3_LIBS with cncodec_v3 path
# SET VARIABLE = VARIABLE-NOTFOUND if library not found,
#   eg. CNCODECV3_LIBS = CNCODECV3_LIBS-NOTFOUND
# ==============================================

if(NOT DEFINED ENV{NEUWARE_HOME})
  set(ENV{NEUWARE_HOME} /usr/local/neuware)
endif()

if((NOT EXISTS $ENV{NEUWARE_HOME}) OR (NOT EXISTS $ENV{NEUWARE_HOME}/include) OR (NOT EXISTS $ENV{NEUWARE_HOME}/lib64))
  message(FATAL_ERROR "NEUWARE_HOME: $ENV{NEUWARE_HOME} not exists!")
else()
  set(NEUWARE_INCLUDE_DIR $ENV{NEUWARE_HOME}/include)
endif()

# ---[ cnrt
find_library(CNRT_LIB_T
             NAMES cnrt
             PATHS $ENV{NEUWARE_HOME}/lib64
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
find_library(CNDRV_LIB_T
             NAMES cndrv
             PATHS $ENV{NEUWARE_HOME}/lib64
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
if(CNRT_LIB_T AND CNDRV_LIB_T)
  set(CNRT_LIBS ${CNRT_LIB_T} ${CNDRV_LIB_T})
else()
  message(STATUS "NEUWARE_HOME:$ENV{NEUWARE_HOME}")
  message(FATAL_ERROR "cnrt or cndrv not found!")
endif()

# ---[ cncodec
find_library(CNCODEC_LIB_T
             NAMES cncodec
             PATHS $ENV{NEUWARE_HOME}/lib64
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
find_library(ION_LIB_T
             NAMES ion
             PATHS $ENV{NEUWARE_HOME}/lib64
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
if(CNCODEC_LIB_T)
  set(CNCODEC_LIBS ${CNCODEC_LIB_T})
else()
  message(FATAL_ERROR "cncodec not found!")
endif()

# ---[ ion
find_library(ION_LIB_T
             NAMES ion
             PATHS $ENV{NEUWARE_HOME}/lib64
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
if(ION_LIB_T)
  list(APPEND CNCODEC_LIBS ${ION_LIB_T})
endif()

# ---[ cncodecv3
find_library(CNCODECV3_LIBS
             NAMES cncodec_v3
             PATHS $ENV{NEUWARE_HOME}/lib64
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)

# ---[ cncv
find_library(CNCV_LIB_T
             NAMES cncv
             PATHS $ENV{NEUWARE_HOME}/lib64
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
if(CNCV_LIB_T)
  set(CNCV_LIBS ${CNCV_LIB_T})
endif()