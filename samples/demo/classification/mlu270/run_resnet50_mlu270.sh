#!/bin/bash
#*************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# wait_time: When set to 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
#
# @notice: other flags see ./../../../bin/demo --help
#*************************************************************************#

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
SAMPLES_DIR=$CURRENT_DIR/../../..
MODEL_PATH=$CURRENT_DIR/../../../../data/models/MLU270/Classification/resnet50
mkdir -p $MODEL_PATH

cd $MODEL_PATH
    if [ ! -f "resnet50_offline_v1.3.0.cambricon" ]; then
      wget -O resnet50_offline_v1.3.0.cambricon  http://video.cambricon.com/models/MLU270/Classification/resnet50/resnet50_offline_v1.3.0.cambricon
    else
      echo "resnet50 offline model exists."
    fi
cd -

source ${SAMPLES_DIR}/demo/env.sh
mkdir -p output
${SAMPLES_DIR}/bin/demo  \
    --data_path ${SAMPLES_DIR}/demo/files.list_image \
    --src_frame_rate 60   \
    --wait_time 0 \
    --loop=false \
    --config_fname "classification_resnet50_mlu270_config.json" \
    --alsologtostderr
