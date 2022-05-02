# CNStream Launcher #

CNStream provides a series of programming samples (known as CNStream launcher) for common scenarios, including classification, object detection, object tracking, vehicle recognition, vehicle license plate recognization, vehicle color, type and side recognization and body pose. These samples could help users quickly experience how to use CNStream to develop applications. Users can run samples directly through scripts without modifying any configuration.

Samples for common scenarios are at directory ``${CNSTREAM_DIR}/samples/cns_launcher`` . In the directories of most of the samples there are a shell script ``run.sh`` and a template json configuration file ``config_template.json`` . Users can run samples directly through script ``run.sh``,  and pass parameters like platform, network or so to the script. 

``${CNSTREAM_DIR}`` represents CNStream source directory.

After users run the script, the following steps will be done:

- Setup environment.
- Generate input file list files ``files.list_image`` , ``files.list_video`` and ``files.list_pose_image`` at ``${CNSTREAM_DIR}/samples`` , unless they are existed.
- Download necessary Cambricon models and label files to ``${CNSTREAM_DIR}/data/models`` .
- Replace multiple kinds of ``PLACE_HOLDER`` in ``config_template.json`` with parameters passed by user to generate file ``config.json`` , which is the configuration file used in the sample. The configuration could be seen as a graph. A graph may contains modules and subgraphs. Subgraphs are other json configuration files. Basically they are common and may not be used in only one sample, they are located at ``${CNSTREAM_DIR}/samples/cns_launcher/configs`` .
- Run ``cns_launcher`` executable file which is at ``${CNSTREAM_DIR}/samples/bin`` with input list file, source frame rate, configuration file and so on.

A few samples do not have a template configuration file, like ``decoding`` , its configuration ``${CNSTREAM_DIR}/samples/cns_launcher/decode/config.json`` is very simple so we do not need a template . And ``Vehicle Recognization`` too , its configuration file is at ``${CNSTREAM_DIR}/samples/cns_launcher/configs`` and its shell script is named ``run_370.sh``. 

Some parameters of the executable file ``cns_launcher``:

- data_path: A file that contains a list of input resources url. Each line is one input stream. Inputs could be local files, RTSP urls and usb camera devices.
- config_fname: A file that contains configuration in json format. A pipeline will be built with the configuration.
- src_frame_rate: It is used to control the input frame rate. The default value is 25, and value -1 means feed data to pipeline as quickly as possible. This parameter is not valid for RTSP stream, video file with h264 extension and jpeg files with ``jpeg_from_mem`` or  ``raw_img_input`` parameter is set to true.
- wait_time: Stop the pipeline after a period of time (in second). The default value is 0 which means the pipeline will be stoped after all streams are processed.
- loop: Whether loop the input streams. It is false by default.
- perf_level: Filter the performance printing. The level range is 0 to 3. By default it is 0. Increase the number to print more detailed performance.
- jpeg_from_mem: Whether use ``EsJpegMemHandler`` if the input is jpeg images. Read jpeg files to memory and send to pipeline.
- raw_img_input: Whether use ``RawImgMemHandler`` if the input is jpeg images. Decode jpeg files and send to pipeline.
- use_cv_mat: Send cv::Mat to pipeline or Send raw data pointer to pipeline. Only valid when ``raw_img_input`` is true.
- maximum_video_width: For video with non-fixed resolution, set the maximum width of the video. Not valid on MLU220 and MLU270 platform.
- maximum_video_height: For video with non-fixed resolution, set the maximum height of the video. Not valid on MLU220 and MLU270 platform.
- maximum_image_width: Set the maximum image width. If MLU decoder is used, the decoder buffer size will be allocated according to ``maximum_image_width`` and ``maximum_image_height``.  It will affects MLU memory usage. Only valid when ``jpeg_from_mem`` is true.
- maximum_image_height: Set the maximum image height. If MLU decoder is used, the decoder buffer size will be allocated according to ``maximum_image_width`` and ``maximum_image_height``.  It will affects MLU memory usage. Only valid when ``jpeg_from_mem`` is true.



For more details, please refer to the [source code](./cns_launcher.cpp)

Also users could modify the existing configuration files or create a new configuration file and pass it to ``cns_launcher`` through ``config_fname`` parameter. By doing this users do not need to recompile the project.

## [Decoding](./decode/README.md)

Decode the input videos by MLU decoder.

**Supported Platform:**  

- MLU220
- MLU270
- MLU370

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``
- src_frame_rate: -1
- configuration: DataSource

**How to run:**

````sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/decode
./run.sh
````

## [Classification](./image_classification/README.md)

Classify the input images by ResNet50 (on MLU270 and MLU370) or ResNet18 (on MLU220) network on MLU.

**Supported Platform:**  

- MLU220
- MLU270
- MLU370

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_image``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Osd->Encode

  (Inferencer2 is used instead Inferencer on MLU370. )

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/image_classification
# Usages: ./run.sh [mlu220/mlu270/mlu370]
./run.sh mlu220  # For MLU220 platform
./run.sh mlu270  # For MLU270 platform
./run.sh mlu370  # For MLU370 platform
```

## [Object Detection](./object_detection/README.md)

 Detect objects in each frame of the input videos by Yolov3 network on MLU.

**Supported Platform:**  

- MLU220
- MLU270
- MLU370

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Osd->Sinker

  (Inferencer2 is used instead Inferencer on MLU370.)

  (Sinker is chosen by users from Encode, RtspSink, Displayer and Kafka module.)

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/object_detection
# Usages: ./run.sh [mlu220/mlu270/mlu370] [encode_jpeg/encode_video/display/rtsp/kafka]
# For example, if the platform is mlu270 and the sinker is RtspSink
./run.sh mlu270 rtsp
```

## [Object Tracking](./object_tracking/README.md)

 Detect objects in each frame of the input videos by Yolov3 or Yolov5 network on MLU. Extract features of objects on MLU by feature_extract model and track them.

**Supported Platform:**  

- MLU220
- MLU270
- MLU370

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Tracker->Osd->Sinker

  (Inferencer2 is used instead Inferencer if Yolov5 is used or the platform is MLU370.)

  (Sinker is chosen by users from Encode, RtspSink and Displayer module.)

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/object_tracking
# Usages: run.sh [mlu220/mlu270/mlu370] [encode_jpeg/encode_video/display/rtsp] [yolov3/yolov5]
# For example, if the platform is mlu370 and we choose Yolov5 as the object detection network. Also we want to encode the results to videos.
./run.sh mlu370 encode_video yolov5
```

## [Vehicle Recognition](./vehicle_recognization/README.md)

Detect objects in each frame of the input videos by Yolov3 network on MLU. And classify all the vehicles by ResNet50 network on MLU.

**Supported Platform:**  

- MLU370

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``
- src_frame_rate: 25
- configuration: DataSource->Inferencer2->Inferencer2->Osd->Encode

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/vehicle_recognization
./run_370.sh
```

## [Vehicle License Plate Recognization](./license_plate_detection_and_recognition/README.md)

Detect objects in each frame of the input videos by Yolov3 network. Detect license plate of vehicles by Mobilenet. And recognize license plates by Lprnet.

**Supported Platform:**  

- MLU220
- MLU270

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Inferencer->Inferencer->Osd->Sinker

  (Sinker is chosen by users from Encode, RtspSink and Displayer module.)

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/license_plate_detection_and_recognition
# Usages: run.sh [mlu220/mlu270] [encode_jpeg/encode_video/display/rtsp]
# For example, if the platform is mlu270 and the sinker is RtspSink
./run.sh mlu270 rtsp
```

## [Vehicle Color Type and Side Recognization](./vehicle_cts/README.md)

Detect objects in each frame of the input videos by SSD network. And recognize color, type and side of vehicles by Vehicle CTS network.

**Supported Platform:**  

- MLU270

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Inferencer->Osd->Sinker

  (Sinker is chosen by users from Encode, RtspSink and Displayer module.)

**How to run:**

```shell
cd ${CNSTREAM_DIR}/samples/cns_launcher/vehicle_cts
# Usages: run.sh [encode_jpeg/encode_video/display/rtsp]
# For example, if the sinker is RtspSink
./run.sh rtsp
```

## [Body Pose](./body_pose/README.md)

Detect the body pose in each input image by OpenPose network on MLU.

**Supported Platform:**  

- MLU220
- MLU270

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_pose_image``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->PoseOsd->Sinker

  (PoseOsd is a custom module defined at ``${CNSTREAM_DIR}/samples/common/cns_openpose/pose_osd_module.cpp``)

  (Sinker is chosen by users from Encode, RtspSink and Displayer module.)

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/body_pose
# Usages: run.sh [mlu220/mlu270] [encode_jpeg/encode_video/display/rtsp]
# For example, if the platform is mlu270 and the sinker is RtspSink
./run.sh mlu270 rtsp
```
