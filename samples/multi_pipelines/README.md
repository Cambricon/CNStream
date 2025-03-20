# Multiple Pipelines #

In this sample, the first pipeline runs on card 0 while the second works on card 1. Two pipeline is built to show how to create multiple pipelines in one application. Add input streams to both pipelines. Wait until all EOS are received from pipelines. This sample is useful for users who want to run pipeline on different devices or separate input streams into different pipelines in one process.

In the directory of this sample there is a shell script [run.sh](./run.sh) and a template json configuration file [config_template.json](./config_template.json) . Users can run sample directly through script ``run.sh``,  and pass parameters platform and sinker to the script.

``${CNSTREAM_DIR}`` represents CNStream source directory.

After users run the script, the following steps will be done:

- Setup environment.
- Generate input file list files ``files.list_image`` , ``files.list_video`` , ``files.list_pose_image`` and ``files.list_sensor`` at ``${CNSTREAM_DIR}/samples`` , unless they are existed.
- Download necessary Cambricon models and label files to ``${CNSTREAM_DIR}/data/models`` .
- Create two directories under ``${CNSTREAM_DIR}/samples/multi_pipelines`` , ``first`` and ``second`` to store configuration files.
- Copy files ``decode_config.json`` , ``yolov3_object_detection_${PLATFORM}.json`` , ``sinker_configs/${SINKER}.json`` from ``${CNSTREAM_DIR}/samples/cns_launcher/configs``  to the directories created at step 4. And set the ``device_id`` to 0 and 1 for the first and the second pipeline. ( ``${PLATFORM}`` and ``${SINKER}`` are chosen by user)
- - Replace multiple kinds of ``PLACE_HOLDER`` in ``config_template.json`` with parameters passed by user to generate file ``first_config.json`` and ``second_config.json`` for the first and second pipeline, which are the configuration file used in this sample. The configuration could be seen as a graph. A graph may contains modules and subgraphs. Subgraphs are other json configuration files. In this case they are located at ``${CNSTREAM_DIR}/samples/multi_pipelines/first`` for the first pipeline and ``${CNSTREAM_DIR}/samples/multi_pipelines/second`` for the second one.
- Run ``multi_pipelines`` executable file which is at ``${CNSTREAM_DIR}/samples/bin`` with input file paths or input file list file, source frame rate, two configuration file paths and so on.



Some parameters of the executable file ``multi_pipelines``:

- data_path: A file that contains a list of input resources url. Each line is one input stream. Inputs could be local files, RTSP urls and usb camera devices.
- data_name: The video path or image url, for example: ``/path/to/your/video.mp4 ``,  ``/path/to/your/images/%d.jpg`` .
- config_fname: A file that contains configuration in json format. The first pipeline will be built with the configuration.
- config_fname1:  A file that contains configuration in json format. The second pipeline will be built with the configuration.
- src_frame_rate: It is used to control the input frame rate. The default value is 25, and value -1 means feed data to pipeline as quickly as possible. This parameter is not valid for RTSP streams.
- wait_time: Stop the pipeline after a period of time (in second). The default value is 0 which means the pipeline will be stoped after all streams are processed.
- loop: Whether loop the input streams. It is false by default.

For more details, please check [source code](multi_pipelines.cpp).



**How to run:**

```shell
cd ${CNSTREAM_DIR}/samples/multi_pipelines
# Usages: run.sh [mlu370] [encode_jpeg/encode_video/rtsp]
# For example, if the platform is mlu370 and the sinker is rtsp
./run.sh mlu370 rtsp
```
