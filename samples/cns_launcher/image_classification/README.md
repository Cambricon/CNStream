# Classification

Classify the input images by ResNet50 network on MLU.

``${CNSTREAM_DIR}`` represents CNStream source directory.

## Script and Configuration File

[Shell Script](./run.sh)

[Template Configuration File](./config_template.json)

**Configuration File:**

 ``${CNSTREAM_DIR}/samples/cns_launcher/image_classification/config.json``

## Supported Platform

- MLU590
- MLU370
- CE3226

## Parameters

- data path: ``${CNSTREAM_DIR}/samples/files.list_image``

- src_frame_rate: 25

- configuration: DataSource->Inferencer->Osd->Encode

## Models

- ResNet50
  - model:
    - For MLU590 platform, [model](http://video.cambricon.com/models/magicmind/v0.14.0/resnet50_v0.14.0_4b_rgb_uint8.magicmind)
    - For MLU370 platform, [model](http://video.cambricon.com/models/magicmind/v0.13.0/resnet50_v0.13.0_4b_rgb_uint8.magicmind)
    - For CE3226 platform, [model](http://video.cambricon.com/models/magicmind/v0.13.0/resnet50_v0.13.0_4b_rgb_uint8.magicmind)
  - preprocessing: [PreprocClassification](../../common/preprocess/preprocess_classification.cpp)
  - postprocessing: [PostprocClassification](../../common/postprocess/postprocess_classification.cpp)

## How to run

```sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/image_classification
# Usages: ./run.sh [mlu590/mlu370/ce3226]
# For example, if the platform is mlu370
./run.sh mlu370
```


After users run the script, the following steps will be done:

- Setup environment.
- Generate input file list files ``files.list_image`` , ``files.list_video`` ``files.list_pose_image`` and ``files.list_sensor`` at ``${CNSTREAM_DIR}/samples`` , unless they are existed.
- Download necessary Cambricon models and label files to ``${CNSTREAM_DIR}/data/models`` .
- Replace multiple kinds of ``PLACE_HOLDER`` in ``config_template.json`` with parameters passed by user to generate file ``config.json`` , which is the configuration file used in the sample. The configuration could be seen as a graph. A graph may contains modules and subgraphs. Subgraphs are other json configuration files. Basically they are common and may not be used in only one sample, they are located at ``${CNSTREAM_DIR}/samples/cns_launcher/configs`` .
- Run ``cns_launcher`` executable file which is at ``${CNSTREAM_DIR}/samples/bin`` with input list file, source frame rate, configuration file and so on.



For more details, please refer to the [source code](../cns_launcher.cpp)

Also users could modify the existing configuration files or create a new configuration file and pass it to ``cns_launcher`` through ``config_fname`` parameter. By doing this users do not need to recompile the project.
