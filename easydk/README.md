# Cambricon Easy Development Kit

Cambricon Easy Development Kit is a toolkit, which aim at helping with developing software on Cambricon MLU270/MLU220 M2 platform.

Toolkit provides following modules:
- EasyCodec: easy decode and encode on MLU
- EasyInfer: easy inference accelerator on MLU
- EasyTrack: easy track, including feature match track and kcf track
- EasyBang: easy Bang operator

## **Cambricon Dependencies** ##

You can find the cambricon dependencies, including headers and libraries, in the MLU directory.

### Quick Start ###

This section introduces how to quickly build instructions on Toolkit and how to develop your own applications based on CNStream-Toolkit.

#### **Required environments** ####

Before building instructions, you need to install the following software:

- cmake 2.8.7+

samples & tests dependencies:

- OpenCV 2.4.9+
- GFlags 2.1.2
- GLog   0.3.4

#### Ubuntu or Debian ####

If you are using Ubuntu or Debian, run the following commands:

   ```bash
   sudo apt install cmake
   # samples dependencies
   sudo apt install libgoogle-glog-dev libgflags-dev libopencv-dev
   ```

#### Centos ####

If you are using Centos, run the following commands:

   ```bash
   sudo yum install cmake
   # samples dependencies
   sudo yum install glog gflags opencv-devel
   ```

## Build Instructions Using CMake ##

After finished prerequiste, you can build instructions with the following steps:

1. Run the following command to save a directory for saving the output.

   ```bash
   mkdir build       # Create a directory to save the output.
   ```

   A Makefile will be generated in the build folder.

2. Run the following command to generate a script for building instructions.

   ```bash
   cd build
   cmake ${TOOLKIT_DIR}  # Generate native build scripts.
   ```

   Cambricon CNStream-Toolkit provides a CMake script ([CMakeLists.txt](CMakeLists.txt)) to build instructions. You can download CMake for free from <http://www.cmake.org/>.

   `${TOOLKIT_DIR}` specifies the directory where CNStream-Toolkit saves for.

   | cmake option  | range           | default | description          |
   | ------------- | --------------- | ------- | -------------------- |
   | BUILD_SAMPLES | ON / OFF        | ON      | build with samples   |
   | BUILD_TESTS   | ON / OFF        | ON      | build with tests     |
   | RELEASE       | ON / OFF        | ON      | release / debug      |
   | WITH_CODEC    | ON / OFF        | ON      | build codec          |
   | WITH_INFER    | ON / OFF        | ON      | build infer          |
   | WITH_TRACKER  | ON / OFF        | ON      | build tracker        |
   | WITH_BANG     | ON / OFF        | ON      | build bang           |

   Example:

   ```bash
   cd build
   # build without samples and tests
   cmake ${TOOLKIT_DIR}      \
        -DBUILD_SAMPLES=OFF  \
        -DBUILD_TESTS=OFF
   ```

3. Run the following command to build instructions:

   ```bash
   make
   ```

