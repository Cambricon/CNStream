#!/bin/bash
#*************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# wait_time:  When set to 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
#
# @notice: other flags see ${SAMPLES_DIR}/detection --help
#*************************************************************************#

CURRENT_FILE=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
SAMPLES_DIR=$CURRENT_FILE/../../..
MODEL_PATH=$CURRENT_FILE/../../../../data/models/MLU220/Primary_Detector/YOLOv3/
MODEL_NAME=yolov3_argb_bs4core4.cambricon
mkdir -p $MODEL_PATH

cd $MODEL_PATH
    if [ ! -f "${MODEL_NAME}" ]; then
      wget -O ${MODEL_NAME} http://video.cambricon.com/models/MLU220/Primary_Detector/YOLOv3/${MODEL_NAME} 
    else
      echo "${MODEL_NAME} exists."
    fi
cd -

source ${SAMPLES_DIR}/demo/env.sh
mkdir -p $CURRENT_FILE/output
${SAMPLES_DIR}/bin/demo \
    --data_path ${SAMPLES_DIR}/demo/files.list_video \
    --src_frame_rate 30 \
    --wait_time 0 \
    --loop=false \
    --config_fname "$CURRENT_FILE/track_SORT_mlu220_config.json" \
    --alsologtostderr
