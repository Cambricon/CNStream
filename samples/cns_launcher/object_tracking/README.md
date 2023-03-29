# Object Tracking

 Detect objects in each frame of the input videos by Yolov3 or Yolov5 network on MLU. Extract features of objects on MLU by feature_extract model and track them.

``${CNSTREAM_DIR}`` represents CNStream source directory.

## Script and Configuration File

[Shell Script](./run.sh)

[Template Configuration File](./config_template.json)

**Configuration File:**

 ``${CNSTREAM_DIR}/samples/cns_launcher/object_tracking/config.json``

## Supported Platform

- MLU220
- MLU270
- MLU370

## Parameters

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Tracker->Osd->Sinker

  (Inferencer2 is used instead Inferencer if Yolov5 is used or the platform is MLU370.)

  (Sinker is chosen by users from Encode, RtspSink and Displayer module.)

## Models

- For MLU220:
  - [Yolov3](http://video.cambricon.com/models/MLU220/yolov3_b4c4_argb_mlu220.cambricon)
    - preprocessing: EasyBang ResizeConvert operator
    - postprocessing: [PostprocYolov3](../../common/postprocess/postprocess_yolov3.cpp)
  - [Yolov5](http://video.cambricon.com/models/MLU220/yolov5/yolov5_b4c4_rgb_mlu220.cambricon)
    - preprocessing: [VideoPreprocYolov5](../../common/video_preprocess/video_preprocess_yolov5.cpp)
    - postprocessing: [VideoPostprocYolov5](../../common/video_postprocess/video_postprocess_yolov5.cpp)
  - [FeatureExtract](http://video.cambricon.com/models/MLU220/feature_extract_for_tracker_b4c4_argb_mlu220.cambricon)
- For MLU270:
  - [Yolov3](http://video.cambricon.com/models/MLU270/yolov3_b4c4_argb_mlu270.cambricon)
    - preprocessing: EasyBang ResizeConvert operator
    - postprocessing: [PostprocYolov3](../../common/postprocess/postprocess_yolov3.cpp)
  - [Yolov5](http://video.cambricon.com/models/MLU270/yolov5/yolov5_b4c4_rgb_mlu270.cambricon)
    - preprocessing: [VideoPreprocYolov5](../../common/video_preprocess/video_preprocess_yolov5.cpp)
    - postprocessing: [VideoPostprocYolov5](../../common/video_postprocess/video_postprocess_yolov5.cpp)
  - [FeatureExtract](http://video.cambricon.com/models/MLU270/feature_extract_for_tracker_b4c4_argb_mlu270.cambricon)
- For MLU370:
  - [Yolov3](http://video.cambricon.com/models/magicmind/v1.1.0/yolov3_v1.1.0_4b_rgb_uint8.magicmind)
    - preprocessing: CNCV (Cambricon CV library) operator(s)
    - postprocessing: [VideoPostprocYolov3MM](../../common/video_postprocess/video_postprocess_yolov3_mm.cpp)
  - [Yolov5](http://video.cambricon.com/models/magicmind/v1.1.0/yolov5m_v1.1.0_4b_rgb_uint8.magicmind)
    - preprocessing: CNCV (Cambricon CV library) operator(s)
    - postprocessing: [VideoPostprocYolov5MM](../../common/video_postprocess/video_postprocess_yolov5_mm.cpp)
  - [FeatureExtract](http://video.cambricon.com/models/magicmind/v1.1.0/feature_extract_v1.1.0_4b_rgb_fp32.magicmind)

## Sinker

**encode_jpeg**:

``${CNSTREAM_DIR}/samples/cns_launcher/configs/sinker_configs/encode_jpeg.json``

**encode_video**

``${CNSTREAM_DIR}/samples/cns_launcher/configs/sinker_configs/encode_video.json``

**display**

``${CNSTREAM_DIR}/samples/cns_launcher/configs/sinker_configs/display.json``

**rtsp**

``${CNSTREAM_DIR}/samples/cns_launcher/configs/sinker_configs/rtsp.json``

## How to run

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/object_tracking
# Usages: run.sh [mlu220/mlu270/mlu370] [encode_jpeg/encode_video/display/rtsp] [yolov3/yolov5]
# For example, if the platform is mlu370 and we choose Yolov5 as the object detection network. Also we want to encode the results to videos.
./run.sh mlu370 encode_video yolov5
```



After users run the script, the following steps will be done:

- Setup environment.
- Generate input file list files ``files.list_image`` , ``files.list_video`` and ``files.list_pose_image`` at ``${CNSTREAM_DIR}/samples`` , unless they are existed.
- Download necessary Cambricon models and label files to ``${CNSTREAM_DIR}/data/models`` .
- Replace multiple kinds of ``PLACE_HOLDER`` in ``config_template.json`` with parameters passed by user to generate file ``config.json`` , which is the configuration file used in the sample. The configuration could be seen as a graph. A graph may contains modules and subgraphs. Subgraphs are other json configuration files. Basically they are common and may not be used in only one sample, they are located at ``${CNSTREAM_DIR}/samples/cns_launcher/configs`` .
- Run ``cns_launcher`` executable file which is at ``${CNSTREAM_DIR}/samples/bin`` with input list file, source frame rate, configuration file and so on.



For more details, please refer to the [source code](../cns_launcher.cpp)

Also users could modify the existing configuration files or create a new configuration file and pass it to ``cns_launcher`` through ``config_fname`` parameter. By doing this users do not need to recompile the project.
