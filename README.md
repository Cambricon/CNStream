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
- Live555         // If WITH_RTSP=ON, please run download_live.
- SDL22.0.4+      // If build_display=ON.
- SQLite3         // If build_perf=ON.

#### Ubuntu or Debian ####

If you are using Ubuntu or Debian, run the following commands:

```bash
  OpenCV2.4.9+  >>>>>>>>>   sudo apt-get install libopencv-dev
  GFlags2.1.2   >>>>>>>>>   sudo apt-get install libgflags-dev
  GLog0.3.4     >>>>>>>>>   sudo apt-get install libgoogle-glog-dev
  Cmake2.8.7+   >>>>>>>>>   sudo apt-get install cmake
  SDL22.0.4+    >>>>>>>>>   sudo apt-get install libsdl2-dev
  SQLite3       >>>>>>>>>   sudo apt-get install libsqlite3-dev sqlite3
```

#### Centos ####

If you are using Centos, run the following commands:

```bash
  OpenCV2.4.9+  >>>>>>>>>   sudo yum install opencv-devel.x86_64
  GFlags2.1.2   >>>>>>>>>   sudo yum install gflags.x86_64
  GLog0.3.4     >>>>>>>>>   sudo yum install glog.x86_64
  Cmake2.8.7+   >>>>>>>>>   sudo yum install cmake3.x86_64
  SDL22.0.4+    >>>>>>>>>   sudo yum install SDL2_gfx-devel.x86_64
  SQLite3       >>>>>>>>>   sudo yum install sqlite-devel
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

   | cmake option        | range                                    | default | description                 |
   | ------------------- | ---------------------------------------- | ------- | --------------------------- |
   | build_display       | ON / OFF                                 | ON      | build display module        |
   | build_encode        | ON / OFF                                 | ON      | build encode module         |
   | build_inference     | ON / OFF                                 | ON      | build inference module      |
   | build_osd           | ON / OFF                                 | ON      | build osd module            |
   | build_source        | ON / OFF                                 | ON      | build source module         |
   | build_track         | ON / OFF                                 | ON      | build track module          |
   | build_perf          | ON / OFF                                 | ON      | build performance statistics|
   | build_tests         | ON / OFF                                 | ON      | build tests                 |
   | build_samples       | ON / OFF                                 | ON      | build samples               |
   | build_test_coverage | ON / OFF                                 | OFF     | build test coverage         |
   | MLU                 | MLU270  / MLU220_SOC                     | MLU270  | specify MLU platform        |
   | RELEASE             | ON / OFF                                 | ON      | release / debug             |
   | WITH_FFMPEG         | ON / OFF                                 | ON      | build with FFMPEG           |
   | WITH_OPENCV         | ON / OFF                                 | ON      | build with OPENCV           |
   | WITH_CHINESE        | ON / OFF                                 | OFF     | build with CHINESE          |
   | WITH_RTSP           | ON / OFF                                 | ON      | build with RTSP             |

3. If you want to build CNStream samples:
   a. Run the following command:

      ```bash
      cmake -Dbuild_samples=ON ${CNSTREAM_DIR}
      ```

   b. Run the following command to add the MLU platform definition. If you are using MLU220 SOC:

      ```bash
   -DMLU=MLU220_SOC  // build the software support MLU220 soc
      ```
   
4. Run the following command to build instructions:

      ```bash
      make
      ```

## Samples ##

### **Demo Overview** ###

This demo shows how to detect objects using CNStream. It includes the following plug-in modules: 

- **source**: Decodes video streams with MLU, such as local video files, RTMP stream, and RTSP stream. 
- **inferencer**: Neural Network inference with MLU.
- **osd**: Draws inference results on images.
- **tracker**: Tracks multi-objects.
- **encoder**: Encodes images with inference results, namely the detection result.
- **displayer**: Displays inference results on the screen.

In the run.sh script, ``detection_config.json`` is set as the configuration file. In this configuration file, resnet34_ssd.cambricon is the offline model used for inference, which means, the data will be fed to an SSD model after decoding. And the results will be shown on the screen.

If we build with build_perf on, the performance statistics of each plug-in module and the pipeline will be printed on the terminal.

In addition, see the comments in ``cnstream/samples/demo/run.sh`` for details.

Another script run_yolov3_mlu270.sh, is an example of Yolov3 implementation. The output will be encoded to AVI files, as an encoder plugin is added. The output directory can be specified by the [dump_dir] parameter. In this case, dump_dir is set to 'output', therefore AVI files can be found in the ``cnstream/samples/demo/output`` directory.

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

### **How to replace SSD offline model in a demo?** ###

Modify the value of the `model_path` in `run.sh` and replace it with your own SSD offline model path.

### **How to change the input video file?** ##

Modify the `files.list_video` file, which is under the cnstream/samples/demo directory, to replace the video path. It is recommended to use an absolute path or use a relative path relative to the executor path.

### **How to adapt other networks than SSD?** ###

1. Modify pre-processing(optional). 2. Modify post-processing**.

   ***Prospect Information：*** Currently, the inferencer plugin in CNStream provides two network preprocessing methods:

2. Specifies that `cpu_preproc` preprocesses the input image on the CPU. Applicable to situations where >b cannot complete pre-processing, such as yolov3.

3. If `cpu_preproc` is NULL, the MLU is used for pre-processing. The offline model needs to have the ability to reduce the mean and multiply the scale in the pre-processing. You can achieve the purpose by configuring the first-level convolution of the mean_value and std parameters. The inferencer plugin performs color space conversion (YUV various formats to RGBA format) and image reduction before performing offline inferencing.


   a. Configure the pre-processing based on foreground information.

      If the CPU is used for pre-processing, the corresponding pre-processing function is implemented first. Then modify the `cpu_preproc` parameter specified when creating the inferencer plugin in the demo, so that it points to the implemented pre-processing function.

   b. Configure the post-processing.
   
      1. Implement the post-processing:

          ```code
          #include <cnstream.hpp>
          class MyPostproc : public Postproc, virtual public edk::ReflexObjectEx<Postproc> {
           public:
            void Execute(std::vector<std::pair<float*, uint64_t>> net_outputs, CNFrameInfoPtr data) override {
              /*
               net_outputs : the result of the inference
    		   net_outputs[i].first : The data pointer of the i-th (starting from 0) output of the offline model.
    		   net_outputs[i].second : The length of the output data of the i-th (starting from 0) of the offline model.
               */


			 /*Do something and put the detection information into data*/
	
	        }
	
	        DECLARE_REFLEX_OBJECT_EX(SsdPostproc, Postproc)
	      };  // class MyPostproc
	
	      ```

. Modify the `postproc_name` parameter in `cnstream/samples/demo/detection_config.json` to the post-processing class name (MyPostproc).

## Documentation ##
[CNStream Read-the-Docs](http://cnstream.readthedocs.io) or [Cambricon Forum Docs](http://forum.cambricon.com/index.php?m=content&c=index&a=lists&catid=85)

Check out the Examples page for tutorials on how to use CNStream. Concepts page for basic definitions

## Community forum ##
[Discuss](http://forum.cambricon.com/list-47-1.html) - General community discussion around CNStream
