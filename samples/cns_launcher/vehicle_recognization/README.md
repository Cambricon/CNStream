# Vehicle Recognition

Detect objects in each frame of the input videos by Yolov3 network on MLU. And classify all the vehicles by ResNet50 network on MLU.

``${CNSTREAM_DIR}`` represents CNStream source directory.

## Script and Configuration File

[Shell Script](./run_370.sh)

[Configuration File](../configs/vehicle_recognization_mlu370.json)

## Supported Platform

- MLU370

## Parameters

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``
- src_frame_rate: 25
- configuration: DataSource->Inferencer2(Yolov3)->Inferencer2(ResNet50)->Osd->Encode

## Models

- For MLU370:
  - [Yolov3](http://video.cambricon.com/models/MLU370/yolov3_nhwc_tfu_0.8.2_uint8_int8_fp16.model)
    - preprocessing: CNCV (Cambricon CV library) operator(s)
    - postprocessing: [VideoPostprocYolov3MM](../../common/postprocess/video_postprocess_yolov3_mm.cpp)
  - [ResNet50](http://video.cambricon.com/models/MLU370/resnet50_nhwc_tfu_0.8.2_uint8_int8_fp16.model)
    - preprocessing: CNCV (Cambricon CV library) operator(s)
    - postprocessing: [VideoPostprocClassification](../../common/postprocess/video_postprocess_classification.cpp)

## How to run

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/vehicle_recognization
./run_370.sh
```



After users run the script, the following steps will be done:

- Setup environment.
- Generate input file list files ``files.list_image`` , ``files.list_video`` and ``files.list_pose_image`` at ``${CNSTREAM_DIR}/samples`` , unless they are existed.
- Download necessary Cambricon models and label files to ``${CNSTREAM_DIR}/data/models`` .s
- Run ``cns_launcher`` executable file which is at ``${CNSTREAM_DIR}/samples/bin`` with input list file, source frame rate, configuration file and so on.



For more details, please refer to the [source code](../cns_launcher.cpp)

Also users could modify the existing configuration files or create a new configuration file and pass it to ``cns_launcher`` through ``config_fname`` parameter. By doing this users do not need to recompile the project.
