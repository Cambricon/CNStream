#!/bin/bash
#*************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# wait_time:  When set to 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
#
# @notice: other flags see ${SAMPLES_DIR}/bin/demo --help
#*************************************************************************#

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
SAMPLES_DIR=$CURRENT_DIR/../..
MODEL_PATH=$CURRENT_DIR/../../../data/models/MLU270/Primary_Detector/ssd/
MODEL_NAME=resnet34_ssd.cambricon
mkdir -p $MODEL_PATH

cd $MODEL_PATH
    if [ ! -f "${MODEL_NAME}" ]; then
      wget -O ${MODEL_NAME} http://video.cambricon.com/models/MLU270/Primary_Detector/ssd/${MODEL_NAME}
      wget -c http://video.cambricon.com/models/MLU270/Primary_Detector/ssd/label_voc.txt
    else
      echo "${MODEL_NAME} exists."
    fi
cd -

source ${SAMPLES_DIR}/demo/env.sh
touch RTSP_url_names.txt
echo "rtsp url names:">RTSP_url_names.txt
${SAMPLES_DIR}/bin/demo  \
    --data_path ${SAMPLES_DIR}/demo/files.list_video \
    --src_frame_rate 30   \
    --wait_time 0 \
    --loop=false \
    --config_fname "RTSP.json" \
    --alsologtostderr
