#!/bin/bash
#*************************************************************************#
# @param
# drop_rate: Decode Drop frame rate (0~1)
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# model_path: offline model path
# label_path: label path
# postproc_name: postproc class name (PostprocSsd)
# wait_time: time of one test case. When set tot 0, it will automatically exit after the eos signal arrives
# rtsp = true: use rtsp
# dump_dir: dump result videos to this directory
# loop = true: loop through video
# device_id: mlu device id
#
# @notice: other flags see ./../bin/demo --help
#*************************************************************************#
source env.sh
mkdir -p output
./../bin/demo  \
    --data_path ./files.list_video \
    --src_frame_rate 60   \
    --wait_time 0 \
    --rtsp=false \
    --loop=false \
    --config_fname "resnet50_config_mlu270.json" \
    --alsologtostderr
