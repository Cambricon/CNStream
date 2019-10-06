# Cambricon CNStream #

CNStream is a streaming framework with plug-ins. It is used to connect other modules, includes basic functionality, libraries,
and essential elements.
CNStream provides following plug-in modules:

- source: Supports RTSP, video file, and images（H.264, H.265, and JPEG decoding.） 
- inference: MLU-based inference accelerator for detection and classification.
- osd (On-screen display): Module for highlighting objects and text overlay.
- encode: Encodes on CPU.
- display: Display the video on screen
- tracker: Multi object tracking

## **Cambricon Dependencies** ##

You can find the cambricon dependencies, including headers and libraries, in the MLU directory.

### Quick Start ###

This section introduces how to quickly build instructions on CNStream and how to develop your own applications based on CNStream.

#### **Required environments** ####

Before building instructions, you need to install the following software:

- OpenCV2.4.9
- GFlags2.1.2
- GLog0.3.4
- Cmake2.8.7+

#### Ubuntu or Debian ####

If you are using Ubuntu or Debian, run the following commands:

```bash
  OpenCV2.4.9   >>>>>>>>>   sudo apt-get install libopencv-dev
  GFlags2.1.2   >>>>>>>>>   sudo apt-get install libgflags-dev
  GLog0.3.4     >>>>>>>>>   sudo apt-get install libgoogle-glog-dev
  Cmake2.8.7+   >>>>>>>>>   sudo apt-get install cmake
```

#### Centos ####

If you are using Centos, run the following commands:

```bash
  OpenCV2.4.9   >>>>>>>>>   sudo yum install opencv-devel.i686
  GFlags2.1.2   >>>>>>>>>   sudo yum install gflags.x86_64
  GLog0.3.4     >>>>>>>>>   sudo yum install glog.x86_64
  Cmake2.8.7+   >>>>>>>>>   sudo yum install cmake3.x86_64
```

## Build Instructions Using CMake ##

After finished prerequiste, you can build instructions with the following steps:

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

3. If you want to build CNStream samples:
   a. Run the following command:

      ```bash
      cmake -Dcnstream_build_samples=ON ${CNSTREAM_DIR}
      ```

   b. Run the following command to add the MLU platform definition. If you are using MLU100:

      ```bash
      -DMLU=MLU100  // build the software support MLU100
      ```

      If you are using MLU270:

      ```bash
      -DMLU=MLU270  // build the software support MLU270

      ```

4. Run the following command to build instructions:

      ```bash
      make
      ```

## Error Handling ##

  If a compilation error occurred, perform the follow steps to resolve it:

  1. Go into the include directory of the neuware package and delete the files for cnstream.

      ```bash
      cd /usr/local/neuware/include
      rm -rf cnbase cndecode cnencode cnosd cntiler cninfer cnpostproc cnpreproc cnvformat cntrack cnstream.hpp  cnstream_base.hpp  cnstream_error.hpp  cnstream_types.hpp  ddr_copyout.hpp  decoder.hpp  decoder_p2p.hpp  duplicator.hpp  connector.hpp  fps_calculator.hpp inferencer.hpp  inferencer_p2p.hpp module.hpp p2p_decoder_inferencer.hpp pipeline.hpp resize_and_convert.hpp sync.hpp tensor.hpp version.hpp
      ```

  2. Go into the lib64 directory of the neuware package and delete the files for cnstream.

      ```bash
      cd /usr/local/neuware/lib64
      rm libcnbase.so libcndecode.so libcnencode.so libcninfer.so libcnosd.so libcnpostproc.so libcnpreproc.so libcnstream.so libcntiler.so libcntrack.so libcnstream-toolkit.so
      ```
  3. Follow the steps above to build the instructions again.

If you still have questions, go to <http://forum.cambricon.com> to get more help.

## Samples ##

### **Demo Overview** ###

The samples/detection-demo is a cnstream-based target detection demo, which includes the following Plug-in modules：

- source: With MLU to decode video streams, such as local video files, rtmp, and rtsp.
- inferencer: With MLU for Neural Network Inferencing.
- osd: Draws Inferencing results on images.
- encoder: Encodes images with inferencing results(detection result).


In this demo, resnet34_ssd.cambricon that is an offline model used for inference.
Output AVI file is in cnstream/samples/detection-demo/output directory. The output directory can be specified by the [dump_dir] parameter.
in addition,See the comments in cnstream/samples/detection-demo/run.sh for details.)

### **Run samples** ###

To run the CNStream sample:

1. Follow the steps above to build instructions.
2. Run the demo using the list below:

   ```bash
   cd ${CNSTREAM_DIR}/samples/detection-demo

   ./run.sh
   ```

## Best Practices ##

### **How to create an application based on CNStream?** ###

you should find a sample from "samples/example/example.cpp",that help developer eassily understand how to develop an application based on cnstream pipeline.

### **How to replace SSD offline model in a demo?** ###

Modify the value of the `model_path` in `run.sh` and replace it with your own SSD offline model path.

### **How to change the input video file?** ##

Modify the `files.list_video` file, which is under the cnstream/samples/detection-demo directory, to replace the video path. It is recommended to use an absolute path or use a relative path relative to the executor path.

### **How to adapt other networks than SSD?** ###

1. Modify pre-processing(optional). 2. Modify post-processing**.

   ***Prospect Information：*** Currently, the inferencer plugin in CNStream provides two network preprocessing methods:

2. Specifies that `cpu_preproc` preprocesses the input image on the CPU. Applicable to situations where >b cannot complete pre-processing, such as yolov3.

3. If `cpu_preproc` is NULL, the MLU is used for pre-processing. The offline model needs to have the ability to reduce the mean and multiply the scale in the pre-processing. You can achieve the purpose by configuring the first-level convolution of the mean_value and std parameters. The inferencer plugin performs color space conversion (yuv various formats to RGBA format) and image reduction before performing offline inferencing.


   a. Configure the pre-processing based on foreground information.

      If the CPU is used for pre-processing, the corresponding pre-processing function is implemented first. Then modify the `cpu_preproc` parameter specified when creating the inferencer plugin in the demo, so that it points to the implemented pre-processing function.

   b. Configure the post processing.
      1. Implement the post-processing:

          ```code
          #include <cnstream.hpp>
          class MyPostproc : public Postproc, virtual public libstream::ReflexObjectEx<Postproc> {
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


      2. Modify the `postproc_name` parameter in `cnstream/samples/detection_demo/run.sh` to the post-processing class name (MyPostproc).
