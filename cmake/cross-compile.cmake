#this file is only an example to configure CMAKE_TOOLCHAIN_FILE for cross compile CNStream

SET(CMAKE_SYSTEM_NAME Linux)

SET(CROSS_PREFIX /opt/make_tool/aarch64/gcc-linaro-6.2.1-2016.11-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-)
SET(CMAKE_C_COMPILER  /opt/make_tool/aarch64/gcc-linaro-6.2.1-2016.11-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc)
SET(CMAKE_CXX_COMPILER /opt/make_tool/aarch64/gcc-linaro-6.2.1-2016.11-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-g++)
SET(CMAKE_SYSTEM_PROCESSOR aarch64)
SET(CMAKE_FIND_ROOT_PATH  /opt/make_tool/aarch64/gcc-linaro-6.2.1-2016.11-x86_64_aarch64-linux-gnu/bin/)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
