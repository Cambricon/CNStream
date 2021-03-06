# Cambricon CNStream #
CNStream is a streaming framework with plug-ins. It is used to connect other modules, includes basic functionalities, libraries,
and essential elements.

CNStream provides the following built-in modules:

- source: Support RTSP, video file,  images and elementary stream in memory （H.264, H.265, and JPEG decoding.） 
- inference: MLU-based inference accelerator for detection and classification.
- inference2: Based on infer server to run inference, preprocess and postprocess.
- osd (On-screen display): Module for highlighting objects and text overlay.
- encode: Encode videos or images.
- display: Display the video on screen.
- tracker: Multi-object tracking.
- rtsp_sink：Push RTSP stream to internet
- ipc: Make pipeline across process

## **Cambricon Dependencies** ##

CNStream depends on the CNCodec library and the CNRT library which are packed in Cambricon CNToolkit package.
Therefore, the lastest Cambricon CNToolkit package is required. If you do not have one, please feel free to contact with us.
Our mailbox: service@cambricon.com

### Install Cambricon CNToolkit package ###

#### Ubuntu  ####

```bash
  dpkg -i cntoolkit-x.x.x_Ubuntuxx.xx_amd64.deb
  cd /var/cntoolkit-x.x.x
  dpkg -i cncodec-xxx.deb cnrt_xxx.deb
```

#### Centos ####

```bash
  yum -y install cntoolkit-x.x.x.el7.x86_64.rpm
  cd /var/cntoolkit-xxx-x.x.x
  yum -y install cncodec-xxx.rpm cnrt-xxx.rpm
```
After that, Cambricon dependencies that CNStream needed are installed at path '/usr/loacl/neuware'.

Please make sure you must ``not`` install cnstream_xxx.deb or cnstream-xxx.rpm.

### Quick Start ###

This section introduces how to quickly build instructions on CNStream and how to develop your own applications based on CNStream. **We strongly recommend you execute ``pre_required_helper.sh`` to prepare for the environment.** If not, please follow the commands below.

#### **Required environments** ####

Before building instructions, you need to install the following software:

- OpenCV&emsp;2.4.9+
- GFlags&emsp;2.1.2
- GLog&emsp;0.3.4
- CMake&emsp;2.8.7+
- SDL2&emsp;2.0.4+ &emsp;&emsp;  // If build_display=ON
- FFmpeg&emsp;2.8 3.4 4.2

#### Ubuntu ####

If you are using Ubuntu, run the following commands:

```bash
  OpenCV 2.4.9+  >>>>>>>>>   sudo apt-get install libopencv-dev
  GFlags 2.1.2   >>>>>>>>>   sudo apt-get install libgflags-dev
  GLog 0.3.4     >>>>>>>>>   sudo apt-get install libgoogle-glog-dev
  CMake 2.8.7+   >>>>>>>>>   sudo apt-get install cmake
  SDL2 2.0.4+    >>>>>>>>>   sudo apt-get install libsdl2-dev
```

#### Centos ####

If you are using Centos, run the following commands:

```bash
  OpenCV 2.4.9+  >>>>>>>>>   sudo yum install opencv-devel.x86_64
  GFlags 2.1.2   >>>>>>>>>   sudo yum install gflags.x86_64
  GLog 0.3.4     >>>>>>>>>   sudo yum install glog.x86_64
  CMake 2.8.7+   >>>>>>>>>   sudo yum install cmake3.x86_64
  SDL2 2.0.4+    >>>>>>>>>   sudo yum install SDL2_gfx-devel.x86_64
```

## Build Instructions Using CMake ##

After finished prerequisites, you can build project with the following steps:

1. clone submodule easydk with command as below

   ```bash
   git submodule  update  --init
   ```

2. Run the following command to create a directory for saving the output.

   ```bash
   mkdir build       # Create a directory to save the output.
   ```

   A Makefile is generated in the build folder.

3. Run the following command to generate a script for building instructions.

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
   | build_modules_contrib| ON / OFF                                 | ON      | build contributed modules   |
   | build_tests          | ON / OFF                                 | ON      | build tests                 |
   | build_samples        | ON / OFF                                 | ON      | build samples               |
   | RELEASE              | ON / OFF                                 | ON      | release / debug             |
   | WITH_FFMPEG          | ON / OFF                                 | ON      | build with FFMPEG           |
   | WITH_OPENCV          | ON / OFF                                 | ON      | build with OPENCV           |
   | WITH_FREETYPE        | ON / OFF                                 | OFF     | build with FREETYPE         |
   | WITH_RTSP            | ON / OFF                                 | ON      | build with RTSP             |

4. If you want to build CNStream samples:

   a. Run the following command:

      ```bash
      cmake -Dbuild_samples=ON ${CNSTREAM_DIR}
      ```

   b. If wanna cross compile, please follow command to:

      ```bash
      cmake -DCMAKE_TOOLCHAIN_FILE=${CNSTREAM_DIR}/cmake/cross-compile.cmake ${CNSTREAM_DIR}
      ```
      Note: you need to configure toolchain by yourself in cross-compile.cmake and cross-compile gflags, glog, opencv, ffmpeg and install them into ${CNSTREAM_DIR}

      take MLU220EDGE as example:

      ```bash
      cmake ${CNSTREAM_DIR} -DCMAKE_TOOLCHAIN_FILE=${CNSTREAM_DIR}/cmake/cross-compile.cmake  -DCNIS_WITH_CURL=OFF -Dbuild_display=OFF -DMLU=MLU220EDGE
      ```

5. Run the following command to build instructions:

      ```bash
      make
      ```
6. If wanna install CNStream's head files and libraries to somewhere, please add ``` CMAKE_INSTALL_PREFIX ``` to cmake command as below:
     ```bash
     cmake {CNSTREAM_DIR} -DCMAKE_INSTALL_PREFIX=/path/to/install
     make
     make install
     ```

## Samples ##

|                        Classification                        |               Object Detection                |
| :----------------------------------------------------------: | :-------------------------------------------: |
| <img src="./docs/images/classification.gif" alt="Classification" style="height=350px" /> | <img src="./docs/images/yolov3.gif" alt="Object Detection" style="height=350px" /> |

|               Object Tracking               |                 Secondary Classification                 |
| :-----------------------------------------: | :------------------------------------------------------: |
| <img src="./docs/images/track.gif" alt="Object Tracking" style="height=350px" /> | <img src="./docs/images/secondary.gif" alt="Secondary Classification" style="height=350px" /> |



### **Demo Overview** ###

This demo shows how to detect objects using CNStream. It includes the following plug-in modules: 

- **source**: Decodes video streams with MLU, such as local video files, RTMP stream, and RTSP stream. 
- **detector**: Neural Network inference with MLU.
- **osd**: Draws inference results on images.
- **displayer**: Displays inference results on the screen.

In the run.sh script, ``detection_config.json`` is set as the configuration file. In this configuration file, resnet34_ssd.cambricon is the offline model used for inference, which means, the data will be fed to an SSD model after decoding. And the results will be shown on the screen.

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
[Cambricon Forum Docs](http://forum.cambricon.com/list-64-1.html) or [CNStream Read-the-Docs](http://cnstream.readthedocs.io)

Check out the Examples page for tutorials on how to use CNStream. Concepts page for basic definitions

## Community forum ##
[Discuss](http://forum.cambricon.com/list-47-1.html) - General community discussion around CNStream
