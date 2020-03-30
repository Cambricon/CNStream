#!/bin/bash
#*************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# wait_time: time of one test case. When set tot 0, it will automatically exit after the eos signal arrives
# rtsp = true: use rtsp
# input_image = true: input image
# loop = true: loop through video
# config_fname: pipeline config json path 
#*************************************************************************#
source env.sh
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)

pushd $CURRENT_DIR
    if [ ! -f "$CURRENT_DIR/openpose-1batch.cambricon" ]; then
      wget -O openpose-1batch.cambricon http://video.cambricon.com/models/MLU100/openpose/openpose-1batch.cambricon
    else
      echo "openpose offline model exists."
    fi
popd

mkdir -p output
./../bin/pose_estimate  \
    --data_path ./files.list_video \
    --src_frame_rate 1000   \
    --wait_time 0 \
    --config_fname "./openpose_config_mlu100.json" \
    --alsologtostderr
