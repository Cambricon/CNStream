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

CURRENT_FILE=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
SAMPLES_DIR=$CURRENT_FILE/../../..
MODEL_PATH=$CURRENT_FILE/../../../../data/models/MLU270/Classification/resnet50
MODEL_NAME=resnet50_offline_v1.3.0.cambricon
mkdir -p $MODEL_PATH

cd $MODEL_PATH
    if [ ! -f "${MODEL_NAME}" ]; then
      wget -O resnet50_offline_v1.3.0.cambricon  http://video.cambricon.com/models/MLU270/Classification/resnet50/${MODEL_NAME}
      wget -c http://video.cambricon.com/models/MLU270/Classification/resnet50/synset_words.txt
    else
      echo "${MODEL_NAME} offline model exists."
    fi
cd -

source ${SAMPLES_DIR}/demo/env.sh
mkdir -p $CURRENT_FILE/output
${SAMPLES_DIR}/bin/demo  \
    --data_path ${SAMPLES_DIR}/demo/files.list_image \
    --src_frame_rate 60   \
    --wait_time 0 \
    --loop=false \
    --config_fname "$CURRENT_FILE/classification_resnet50_mlu270_config_infer2.json" \
    --log_to_stderr=true
