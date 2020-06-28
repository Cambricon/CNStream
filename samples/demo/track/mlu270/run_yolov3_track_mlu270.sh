#!/bin/bash
#*************************************************************************#
# @param
# drop_rate: Decode Drop frame rate (0~1)
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# model_path: offline model path
# wait_time: time of one test case. When set tot 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
#
# @notice: other flags see ${SAMPLES_DIR}/bin/demo --help
#*************************************************************************#

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
SAMPLES_DIR=$CURRENT_DIR/../../..
MODEL_PATH=$CURRENT_DIR/../../../../data/models/MLU270/yolov3/
MODEL_NAME=yolov3_offline_u4_v1.3.0.cambricon
mkdir -p $MODEL_PATH
cd $MODEL_PATH
    if [ ! -f "${MODEL_NAME}" ]; then
      wget -O ${MODEL_NAME} http://video.cambricon.com/models/MLU270/yolov3/${MODEL_NAME} 
    else
      echo "${MODEL_NAME} exists."
    fi
cd -

source ${SAMPLES_DIR}/demo/env.sh
MODEL_PATH=$CURRENT_DIR/../../data/models/MLU270/feature_extract
mkdir -p $MODEL_PATH
cd $MODEL_PATH
    if [ ! -f "feature_extract_v1.3.0.cambricon" ]; then
      wget -O feature_extract_v1.3.0.cambricon  http://video.cambricon.com/models/MLU270/feature_extract/feature_extract_v1.3.0.cambricon
    else
      echo "feature extarct offline model exists."
    fi
cd -

mkdir -p output
${SAMPLES_DIR}/bin/demo  \
    --data_path ${SAMPLES_DIR}/demo/files.list_video \
    --src_frame_rate 60  \
    --wait_time 0 \
    --loop=false \
    --config_fname "yolov3_track_mlu270.json" \
    --alsologtostderr
