# Classification

Classify the input images by ResNet50 (on MLU270 and MLU370) or ResNet18 (on MLU220) network on MLU.

``${CNSTREAM_DIR}`` represents CNStream source directory.

## Script and Configuration File

[Shell Script](./run.sh)

[Template Configuration File](./config_template.json)

**Configuration File:**

 ``${CNSTREAM_DIR}/samples/cns_launcher/image_classification/config.json``

## Supported Platform

- MLU220
- MLU270
- MLU370

## Parameters

- data path: ``${CNSTREAM_DIR}/samples/files.list_image``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Osd->Encode

  (Inferencer2 is used instead of Inferencer on MLU370. )

## Models

- For MLU220:
  - [ResNet18](http://video.cambricon.com/models/MLU220/resnet18_b4c4_bgra_mlu220.cambricon)
    - preprocessing: EasyBang ResizeConvert operator
    - postprocessing: [PostprocClassification](../../common/postprocess/postprocess_classification.cpp)
- For MLU270:
  - [ResNet50](http://video.cambricon.com/models/MLU270/resnet50_b16c16_bgra_mlu270.cambricon)
    - preprocessing: EasyBang ResizeConvert operator
    - postprocessing: [PostprocClassification](../../common/postprocess/postprocess_classification.cpp)
- For MLU370:
  - [ResNet50](http://video.cambricon.com/models/magicmind/v1.1.0/resnet50_v1.1.0_4b_rgb_uint8.magicmind)
    - preprocessing: CNCV (Cambricon CV library) operator(s)
    - postprocessing: [VideoPostprocClassification](../../common/video_postprocess/video_postprocess_classification.cpp)

## How to run

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/image_classification
# Usages: ./run.sh [mlu220/mlu270/mlu370]
./run.sh mlu220  # For MLU220 platform
./run.sh mlu270  # For MLU270 platform
./run.sh mlu370  # For MLU370 platform
```



After users run the script, the following steps will be done:

- Setup environment.
- Generate input file list files ``files.list_image`` , ``files.list_video`` and ``files.list_pose_image`` at ``${CNSTREAM_DIR}/samples`` , unless they are existed.
- Download necessary Cambricon models and label files to ``${CNSTREAM_DIR}/data/models`` .
- Replace multiple kinds of ``PLACE_HOLDER`` in ``config_template.json`` with parameters passed by user to generate file ``config.json`` , which is the configuration file used in the sample. The configuration could be seen as a graph. A graph may contains modules and subgraphs. Subgraphs are other json configuration files. Basically they are common and may not be used in only one sample, they are located at ``${CNSTREAM_DIR}/samples/cns_launcher/configs`` .
- Run ``cns_launcher`` executable file which is at ``${CNSTREAM_DIR}/samples/bin`` with input list file, source frame rate, configuration file and so on.



For more details, please refer to the [source code](../cns_launcher.cpp)

Also users could modify the existing configuration files or create a new configuration file and pass it to ``cns_launcher`` through ``config_fname`` parameter. By doing this users do not need to recompile the project.
