# Cambricon CNStream #

CNStream is a streaming framework with plug-ins. It is used to connect other modules, includes basic functionalities, libraries,
and essential elements.

CNStream provides the following plug-in modules:

- source: Supports RTSP, video file, and images（H.264, H.265, and JPEG decoding.） 
- inference: MLU-based inference accelerator for detection and classification.
- osd (On-screen display): Module for highlighting objects and text overlay.
- encode: Encodes on CPU.
- display: Display the video on screen.
- tracker: Multi-object tracking.
- rtsp_sink：Push RTSP stream to internet
- ipc: Make pipeline across process

## **Cambricon Dependencies** ##

CNStream depends on the CNCodec library and the CNRT library which are packed in Cambricon neuware package.
Therefore, the lastest Cambricon neuware package is required. If you do not have one, please feel free to contact with us.
Our mailbox: service@cambricon.com

### Install Cambricon neuware package ###

#### Ubuntu or Debian ####

```bash
  dpkg -i neuware-mluxxx-x.x.x_Ubuntuxx.xx_amd64.deb
  cd /var/neuware-mluxxx-x.x.x
  dpkg -i cncodec-xxx.deb cnrt_xxx.deb
```

#### Centos ####

```bash
  yum -y install neuware-mluxxx-x.x.x.el7.x86_64.rpm
  cd /var/neuware-mluxxx-x.x.x
  yum -y install cncodec-xxx.rpm cnrt-xxx.rpm
```
After that, Cambricon dependencies that CNStream needed are installed at path '/usr/loacl/neuware'.

Please make sure you must ``not`` install cnstream_xxx.deb or cnstream-xxx.rpm.

### Quick Start ###

This section introduces how to quickly build instructions on CNStream and how to develop your own applications based on CNStream. **We strongly recommend you execute ``pre_required_helper.sh`` to prepare for the environment.** If not, please follow the commands below.

#### **Required environments** ####

Before building instructions, you need to install the following software:

- OpenCV2.4.9+
- GFlags2.1.2
- GLog0.3.4
- Cmake2.8.7+
- SDL22.0.4+ &emsp;&emsp;  // If build_display=ON

#### Ubuntu or Debian ####

If you are using Ubuntu or Debian, run the following commands:

```bash
  OpenCV2.4.9+  >>>>>>>>>   sudo apt-get install libopencv-dev
  GFlags2.1.2   >>>>>>>>>   sudo apt-get install libgflags-dev
  GLog0.3.4     >>>>>>>>>   sudo apt-get install libgoogle-glog-dev
  Cmake2.8.7+   >>>>>>>>>   sudo apt-get install cmake
  SDL22.0.4+    >>>>>>>>>   sudo apt-get install libsdl2-dev
```

#### Centos ####

If you are using Centos, run the following commands:

```bash
  OpenCV2.4.9+  >>>>>>>>>   sudo yum install opencv-devel.x86_64
  GFlags2.1.2   >>>>>>>>>   sudo yum install gflags.x86_64
  GLog0.3.4     >>>>>>>>>   sudo yum install glog.x86_64
  Cmake2.8.7+   >>>>>>>>>   sudo yum install cmake3.x86_64
  SDL22.0.4+    >>>>>>>>>   sudo yum install SDL2_gfx-devel.x86_64
```

## Build Instructions Using CMake ##

After finished prerequisites, you can build instructions with the following steps:

1. Run the following command to save a directory for saving the output.

   ```bash
   mkdir build       # Create a directory to save the output.
   ```

   A Makefile is generated in the build folder.

2. Run the following command to generate a script for building instructions.

   ```bash
   cd build
   cmake ${CNSTREAM_DIR}  # Generate native build scripts.
   ```

   Cambricon CNStream provides a CMake script ([CMakeLists.txt](CMakeLists.txt)) to build instructions. You can download CMake for free from <http://www.cmake.org/>.

   `${CNSTREAM_DIR}` specifies the directory where CNStream saves for.

   | cmake option         | range                                    | default | description                 |
   | -------------------- | ---------------------------------------- | ------- | --------------------------- |
   | build_display        | ON / OFF                                 | ON      | build display module        |
   | build_ipc            | ON / OFF                                 | ON      | build ipc module            |
   | build_encode         | ON / OFF                                 | ON      | build encode module         |
   | build_inference      | ON / OFF                                 | ON      | build inference module      |
   | build_osd            | ON / OFF                                 | ON      | build osd module            |
   | build_rtsp_sink      | ON / OFF                                 | ON      | build rtsp_sink module      |
   | build_source         | ON / OFF                                 | ON      | build source module         |
   | build_track          | ON / OFF                                 | ON      | build track module          |
   | build_perf           | ON / OFF                                 | ON      | build performance statistics|
   | build_modules_contrib| ON / OFF                                 | ON      | build contributed modules   |
   | build_tests          | ON / OFF                                 | ON      | build tests                 |
   | build_samples        | ON / OFF                                 | ON      | build samples               |
   | RELEASE              | ON / OFF                                 | ON      | release / debug             |
   | WITH_FFMPEG          | ON / OFF                                 | ON      | build with FFMPEG           |
   | WITH_OPENCV          | ON / OFF                                 | ON      | build with OPENCV           |
   | WITH_CHINESE         | ON / OFF                                 | OFF     | build with CHINESE          |
   | WITH_RTSP            | ON / OFF                                 | ON      | build with RTSP             |

3. If you want to build CNStream samples:
   a. Run the following command:

      ```bash
      cmake -Dbuild_samples=ON ${CNSTREAM_DIR}
      ```

   b. If wanna cross compile, please follow command to:

      ```bash
      cmake -DCMAKE_TOOLCHAIN_FILE=${CNSTREAM_DIR}/cmake/cross-compile.cmake ${CNSTREAM_DIR}
      ```
      Note: you need to configure toolchain by yourself in cross-compile.cmake
4. Run the following command to build instructions:

      ```bash
      make
      ```
5. If wanna install CNStream's head files and libraries to somewhere, please add ``` CMAKE_INSTALL_PREFIX ``` to cmake command as below:
     ```bash
     cmake {CNSTREAM_DIR} -DCMAKE_INSTALL_PREFIX=/path/to/install
     make
     make install
     ```
## Samples ##

### **Demo Overview** ###

![demo](./data/images/demo.gif)

This demo shows how to detect objects using CNStream. It includes the following plug-in modules: 

- **source**: Decodes video streams with MLU, such as local video files, RTMP stream, and RTSP stream. 
- **detector**: Neural Network inference with MLU.
- **osd**: Draws inference results on images.
- **displayer**: Displays inference results on the screen.

In the run.sh script, ``detection_config.json`` is set as the configuration file. In this configuration file, resnet34_ssd.cambricon is the offline model used for inference, which means, the data will be fed to an SSD model after decoding. And the results will be shown on the screen.

If we build with build_perf on, the performance statistics of each plug-in module and the pipeline will be printed on the terminal.

In addition, see the comments in ``cnstream/samples/demo/run.sh`` for details.

Also there are several demos as located under ***classification***, ***detection***, ***track***, ***secondary***, ***rtsp*** etc.

### **Run samples** ###

To run the CNStream sample:

1. Follow the steps above to build instructions.
2. Run the demo using the list below:

   ```bash
   cd ${CNSTREAM_DIR}/samples/demo

   ./run.sh
   ```

## Best Practices ##

### **How to create an application based on CNStream?** ###

You should find a sample from ``samples/example/example.cpp`` that helps developers easily understand how to develop an application based on CNStream pipeline.

### **How to change the input video file?** ##

Modify the `files.list_video` file, which is under the cnstream/samples/demo directory, to replace the video path. Each line represents one stream. It is recommended to use an absolute path or use a relative path relative to the executor path.


## Documentation ##
[CNStream Read-the-Docs](http://cnstream.readthedocs.io) or [Cambricon Forum Docs](http://forum.cambricon.com/index.php?m=content&c=index&a=lists&catid=85)

Check out the Examples page for tutorials on how to use CNStream. Concepts page for basic definitions

## Community forum ##
[Discuss](http://forum.cambricon.com/list-47-1.html) - General community discussion around CNStream
