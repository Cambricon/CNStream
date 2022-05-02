# Body Pose

Detect the body pose in each input image by OpenPose network on MLU.

``${CNSTREAM_DIR}`` represents CNStream source directory.

## Script and Configuration File

[Shell Script](./run.sh)

[Template Configuration File](./config_template.json)

**Configuration File:**

 ``${CNSTREAM_DIR}/samples/cns_launcher/body_pose/config.json``

## Supported Platform

- MLU220
- MLU270

## Parameters

- data path: ``${CNSTREAM_DIR}/samples/files.list_pose_image``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->PoseOsd->Sinker

  (PoseOsd is a custom module defined at ``${CNSTREAM_DIR}/samples/common/cns_openpose/pose_osd_module.cpp``)

  (Sinker is chosen by users from Encode, RtspSink and Displayer module.)

## Models

- For MLU220:
  - [OpenPose](http://video.cambricon.com/models/MLU220/coco_pose_b4c4_bgra_mlu220.cambricon)
    - preprocessing: EasyBang ResizeConvert operator
    - postprocessing: [PostprocPose](../../common/cns_openpose/postprocess_body_pose.cpp)
- For MLU270:
  - [OpenPose](http://video.cambricon.com/models/MLU270/coco_pose_b4c4_bgra_mlu270.cambricon)
    - preprocessing: EasyBang ResizeConvert operator
    - postprocessing: [PostprocPose](../../common/cns_openpose/postprocess_body_pose.cpp)

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
cd ${CNSTREAM_DIR}/samples/cns_launcher/body_pose
# Usages: run.sh [mlu220/mlu270] [encode_jpeg/encode_video/display/rtsp]
# For example, if the platform is mlu270 and the sinker is RtspSink
./run.sh mlu270 rtsp
```



After users run the script, the following steps will be done:

- Setup environment.
- Generate input file list files ``files.list_image`` , ``files.list_video`` and ``files.list_pose_image`` at ``${CNSTREAM_DIR}/samples`` , unless they are existed.
- Download necessary Cambricon models and label files to ``${CNSTREAM_DIR}/data/models`` .
- Replace multiple kinds of ``PLACE_HOLDER`` in ``config_template.json`` with parameters passed by user to generate file ``config.json`` , which is the configuration file used in the sample. The configuration could be seen as a graph. A graph may contains modules and subgraphs. Subgraphs are other json configuration files. Basically they are common and may not be used in only one sample, they are located at ``${CNSTREAM_DIR}/samples/cns_launcher/configs`` .
- Run ``cns_launcher`` executable file which is at ``${CNSTREAM_DIR}/samples/bin`` with input list file, source frame rate, configuration file and so on.



For more details, please refer to the [source code](../cns_launcher.cpp)

Also users could modify the existing configuration files or create a new configuration file and pass it to ``cns_launcher`` through ``config_fname`` parameter. By doing this users do not need to recompile the project.
