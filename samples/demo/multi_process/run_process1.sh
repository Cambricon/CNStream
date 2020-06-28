#!/bin/bash
#*************************************************************************#
# @param
# drop_rate: Decode Drop frame rate (0~1)
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# model_path: offline model path
# label_path: label path
# postproc_name: postproc class name (PostprocYolov3)
# wait_time: time of one test case. When set tot 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
# device_id: mlu device id
#
# @notice: other flags see ./../../bin/demo --help
#*************************************************************************#
source ../env.sh
mkdir -p output
./../../bin/demo  \
    --data_path ./../files.list_video \
    --src_frame_rate 100   \
    --wait_time 0 \
    --loop=false \
    --config_fname "config_process1.json" \
    --alsologtostderr \
    --perf_db_dir="./process1"
