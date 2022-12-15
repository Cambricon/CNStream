# ==============================================
# Try to find Cambricon MPS libraries:
# - cnrt
# - cndrv
# - cn_codec
# - cn_vo
# - cn_vps
# - cn_vgu
# - cn_g2d
# - cn_sys
#
# SET MPS_INCLUDE_DIR with neuware include directory
# ==============================================

if(MPS_HOME)
  get_filename_component(MPS_HOME ${MPS_HOME} ABSOLUTE)
  message(STATUS "MPS_HOME: ${MPS_HOME}")
elseif(DEFINED ENV{MPS_HOME})
  get_filename_component(MPS_HOME $ENV{MPS_HOME} ABSOLUTE)
  message(STATUS "ENV{MPS_HOME}: ${MPS_HOME}")
else()
  set(MPS_HOME "/mps")
  message(STATUS "Default MPS_HOME: ${MPS_HOME}")
endif()

if((NOT EXISTS ${MPS_HOME}) OR (NOT EXISTS ${MPS_HOME}/include) OR (NOT EXISTS ${MPS_HOME}/lib))
  message(FATAL_ERROR "MPS_HOME: ${MPS_HOME} not exists!")
else()
  set(MPS_INCLUDE_DIR ${MPS_HOME}/include)
endif()

# ---[ cnrt
find_library(CNRT_LIBS
             NAMES cnrt
             PATHS ${MPS_HOME}/lib
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
find_library(CNDRV_LIBS
             NAMES cndrv
             PATHS ${MPS_HOME}/lib
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)

# ---[ cnsys
find_library(CNSYS_LIBS
             NAMES cn_sys
             PATHS ${MPS_HOME}/lib
             NO_CMAKE_FIND_ROOT_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
if (CNSYS_LIBS)
  message(STATUS "Found CNSYS: ${CNSYS_LIBS}")
else()
  message(FATAL_ERROR "CNSYS NOT FOUND")
endif()

# ---[ cncodec
find_library(CNCODEC_LIBS
             NAMES cn_codec
             PATHS ${MPS_HOME}/lib
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
if (CNCODEC_LIBS)
  message(STATUS "Found CNCODEC: ${CNCODEC_LIBS}")
else()
  message(FATAL_ERROR "CNCODEC NOT FOUND")
endif()

# ---[ cnvo
find_library(CNVO_LIBS
             NAMES cn_vo
             PATHS ${MPS_HOME}/lib
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
if (CNVO_LIBS)
  message(STATUS "Found CNVO: ${CNVO_LIBS}")
else()
  message(FATAL_ERROR "CNVO NOT FOUND")
endif()

           
# ---[ cnvps
find_library(CNVPS_LIBS
             NAMES cn_vps
             PATHS ${MPS_HOME}/lib
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
if (CNVPS_LIBS)
  message(STATUS "Found CNVPS: ${CNVPS_LIBS}")
else()
  message(FATAL_ERROR "CNVPS NOT FOUND")
endif()

# ---[ cnvgu
find_library(CNVGU_LIBS
             NAMES cn_vgu
             PATHS ${MPS_HOME}/lib
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
if (CNVGU_LIBS)
  message(STATUS "Found CNVGU: ${CNVGU_LIBS}")
else()
  message(FATAL_ERROR "CNVGU NOT FOUND")
endif()

# ---[ cng2d
find_library(CNG2D_LIBS
             NAMES cn_g2d
             PATHS ${MPS_HOME}/lib
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
if (CNG2D_LIBS)
  message(STATUS "Found CNG2D: ${CNG2D_LIBS}")
else()
  message(FATAL_ERROR "CNG2D NOT FOUND")
endif()

# ---[ sensor
if(SENSOR)
find_library(SENSOR_LIBS
             NAMES cn_${SENSOR}
             PATHS ${MPS_HOME}/lib
             NO_CMAKE_FIND_ROOT_PATH
             NO_CMAKE_PATH
             NO_DEFAULT_PATH
             NO_CMAKE_SYSTEM_PATH)
if (SENSOR_LIBS)
  message(STATUS "Found SENSOR: ${SENSOR_LIBS}")
else()
  message(FATAL_ERROR "SENSOR NOT FOUND")
endif()
endif()