#!/bin/bash
#*************************************************************************#
# @param
# drop_rate: Decode Drop frame rate (0~1)
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# model_path: offline model path
# label_path: label path
# postproc_name: postproc class name (PostprocClassification)
# wait_time: time of one test case. When set tot 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
# device_id: mlu device id
#
# @notice: other flags see ./../../../bin/demo --help
#*************************************************************************#
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
SAMPLES_DIR=$CURRENT_DIR/../../..
MODEL_PATH=$CURRENT_DIR/../../data/models/MLU220/classification/resnet18/
mkdir -p $MODEL_PATH

cd $MODEL_PATH
    if [ ! -f "resnet18_bs4_c4.cambricon" ]; then
      wget -O resnet18_bs4_c4.cambricon  http://video.cambricon.com/models/MLU220/classification/resnet18/resnet18_bs4_c4.cambricon
    else
      echo "resnet18 offline model exists."
    fi
cd -

source ${SAMPLES_DIR}/demo/env.sh
mkdir -p output
${SAMPLES_DIR}/bin/demo  \
    --data_path ${SAMPLES_DIR}/demo/files.list_image \
    --src_frame_rate 30 \
    --wait_time 0 \
    --loop=false \
    --config_fname "classification_resnet18_mlu220_config.json" \
    --alsologtostderr
