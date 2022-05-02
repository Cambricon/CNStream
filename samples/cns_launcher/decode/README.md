# Decoding

Decode the input videos by MLU decoder.

``${CNSTREAM_DIR}`` represents CNStream source directory.

## Script and Configuration File

[Shell Script](./run.sh)

[Configuration File](config.json)

## Supported Platform

- MLU220
- MLU270
- MLU370

## Parameters

- data path: ``${CNSTREAM_DIR}/samples/files.list_video``
- src_frame_rate: -1
- configuration: DataSource

## How to run

````sh
cd ${CNSTREAM_DIR}/samples/cns_launcher/decode
./run.sh
````



After users run the script, the following steps will be done:

- Setup environment.
- Generate input file list files ``files.list_image`` , ``files.list_video`` and ``files.list_pose_image`` at ``${CNSTREAM_DIR}/samples`` , unless they are existed.
- Download necessary Cambricon models and label files to ``${CNSTREAM_DIR}/data/models`` .s
- Run ``cns_launcher`` executable file which is at ``${CNSTREAM_DIR}/samples/bin`` with input list file, source frame rate, configuration file and so on.



For more details, please refer to the [source code](../cns_launcher.cpp)

Also users could modify the existing configuration files or create a new configuration file and pass it to ``cns_launcher`` through ``config_fname`` parameter. By doing this users do not need to recompile the project.
