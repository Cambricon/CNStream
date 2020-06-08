#this file is only an example to configure CMAKE_TOOLCHAIN_FILE for cross compile CNStream

SET(CMAKE_SYSTEM_NAME Linux)

SET(CROSS_PREFIX /opt/x86-arm/aarch64-arm-linux/bin/aarch64-arm-linux-)
SET(CMAKE_C_COMPILER  /opt/x86-arm/aarch64-arm-linux/bin/aarch64-arm-linux-gcc)
SET(CMAKE_CXX_COMPILER /opt/x86-arm/aarch64-arm-linux/bin/aarch64-arm-linux-g++)

SET(CMAKE_FIND_ROOT_PATH  /opt/x86-arm/aarch64-arm-linux/)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
