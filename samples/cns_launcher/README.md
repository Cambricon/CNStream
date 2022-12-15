# CNStream Launcher #

Samples for common scenarios are at directory ``${CNSTREAM_DIR}/samples/cns_launcher`` . In the directories of most of the samples there are a shell script ``run.sh`` and a template json configuration file ``config_template.json`` . Users can run samples directly through script ``run.sh``,  and pass parameters like platform, network or so to the script. 

``${CNSTREAM_DIR}`` represents CNStream source directory.

After users run the script, the following steps will be done:

- Setup environment.
- Generate input file list files ``files.list_image`` , ``files.list_video`` ``files.list_pose_image`` and ``files.list_pose_sensor`` at ``${CNSTREAM_DIR}/samples`` , unless they are existed.
- Download necessary Cambricon models and label files to ``${CNSTREAM_DIR}/data/models`` .
- Replace multiple kinds of ``PLACE_HOLDER`` in ``config_template.json`` with parameters passed by user to generate file ``config.json`` , which is the configuration file used in the sample. The configuration could be seen as a graph. A graph may contains modules and subgraphs. Subgraphs are other json configuration files. Basically they are common and may not be used in only one sample, they are located at ``${CNSTREAM_DIR}/samples/cns_launcher/configs`` .
- Run ``cns_launcher`` executable file which is at ``${CNSTREAM_DIR}/samples/bin`` with input list file, source frame rate, configuration file and so on.

A few samples do not have a template configuration file, like ``decoding`` , its configuration ``${CNSTREAM_DIR}/samples/cns_launcher/decode/config.json`` is very simple so we do not need a template .

Some parameters of the executable file ``cns_launcher``:

- data_path: A file that contains a list of input resources url. Each line is one input stream. Inputs could be local files, RTSP urls and sensors.
- config_fname: A file that contains configuration in json format. A pipeline will be built with the configuration.
- src_frame_rate: It is used to control the input frame rate. The default value is 25, and value -1 means feed data to pipeline as quickly as possible. This parameter is not valid for RTSP stream and sensors.
- wait_time: Stop the pipeline after a period of time (in second). The default value is 0 which means the pipeline will be stopped after all streams are processed.
- loop: Whether loop the input streams. It is false by default.
- perf_level: Filter the performance printing. The level range is 0 to 3. By default it is 0. Increase the number to print more detailed performance.
- maximum_width: For variable video resolutions and Jpeg decoding.
- maximum_height: For variable video resolutions and Jpeg decoding.
- codec_id_start: Set the start codec id. By default, it is 0.
- enable_vin: Enable vin. Only valid on CE3226 platform.
- enable_vout: Enable vout. Only valid on CE3226 platform.



For more details, please refer to the source file ``${CNSTREAM_DIR}/samples/cns_launcher/cns_launcher.cpp``

Also users could modify the existing configuration files or create a new configuration file and pass it to ``cns_launcher`` through ``config_fname`` parameter. By doing this users do not need to recompile the project.

## [Decoding](./decode/README.md)

Decode the input videos by MLU decoder.

**Supported Platform:**  

- MLU590
- MLU370
- CE3226

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

Classify the input images by ResNet50 network on MLU.

**Supported Platform:**  

- MLU590
- MLU370
- CE3226

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_image``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Osd->Encode

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/image_classification
# Usages: ./run.sh [mlu590/mlu370/ce3226]
# For example, if the platform is mlu370
./run.sh mlu370
```

## [Object Detection](./object_detection/README.md)

 Detect objects in each frame of the input videos by Yolov3 or Yolov5 network on MLU.

**Supported Platform:**  

- MLU590
- MLU370
- CE3226

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Osd->Sinker

  (Sinker is chosen by users from Encode and Vout module.)

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/object_detection
# Usages: ./run.sh [mlu590/mlu370/ce3226] [encode_jpeg/encode_video/rtsp/vout] [yolov3/yolov5]
# For example, if the platform is mlu370, we choose Yolov5 as the object detection network, and the sinker is rtsp
./run.sh mlu370 rtsp yolov5
```

## [Object Tracking](./object_tracking/README.md)

 Detect objects in each frame of the input videos by Yolov3 or Yolov5 network on MLU. Extract features of objects on MLU by feature_extract model and track them.

**Supported Platform:**  

- MLU370
- CE3226

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Tracker->Osd->Sinker

  (Sinker is chosen by users from Encode and Vout module.)

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/object_tracking
# Usages: run.sh [mlu370/ce3226] [encode_jpeg/encode_video/rtsp/vout] [yolov3/yolov5]
# For example, if the platform is mlu370 and we choose Yolov5 as the object detection network. Also we want to encode the results to videos.
./run.sh mlu370 encode_video yolov5
```

## [Vehicle Recognition](./vehicle_recognition/README.md)

Detect objects in each frame of the input videos by Yolov3 network on MLU. And classify all the vehicles by ResNet50 network on MLU.

**Supported Platform:**  

- MLU370
- CE3226

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``
- src_frame_rate: 25
- configuration: DataSource->Inferencer->Inferencer->Osd->Sinker

  (Sinker is chosen by users from Encode and Vout module.)

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/vehicle_recognition
# Usages: ./run.sh [mlu370/ce3226] [encode_jpeg/encode_video/rtsp/vout]
# For example, if the platform is mlu370 and the sinker is rtsp
./run.sh mlu370 rtsp
```

## [Vehicle License Plate Recognition](./license_plate_detection_and_recognition/README.md)

Detect objects in each frame of the input videos by Yolov3 network. Detect license plate of vehicles by Mobilenet. And recognize license plates by Lprnet.

**Supported Platform:**  

- MLU370
- CE3226

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Inferencer->Inferencer->Osd->Sinker

  (Sinker is chosen by users from Encode and Vout module.)

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/license_plate_detection_and_recognition
# Usages: ./run.sh [mlu370/ce3226] [encode_jpeg/encode_video/rtsp/vout]
# For example, if the platform is mlu370 and the sinker is rtsp
./run.sh mlu370 rtsp
```

## [Body Pose](./body_pose/README.md)

Detect the body pose in each input image by OpenPose network on MLU.

**Supported Platform:**  

- MLU370
- CE3226

**Parameters:**

- data path: ``${CNSTREAM_DIR}/samples/files.list_pose_image``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->PoseOsd->Sinker

  (PoseOsd is a custom module defined at ``${CNSTREAM_DIR}/samples/common/cns_openpose/pose_osd_module.cpp``)

  (Sinker is chosen by users from Encode and Vout module.)

**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/body_pose
# Usages: ./run.sh [mlu370/ce3226] [encode_jpeg/encode_video/rtsp/vout]
# For example, if the platform is mlu370 and the sinker is rtsp
./run.sh mlu370 rtsp
```
