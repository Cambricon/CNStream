#!/bin/bash
#*************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
#
# @notice: other flags see ./../bin/multi_pipelines --help
#*************************************************************************#

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
MODEL_PATH=$CURRENT_DIR/../../../data/models/MLU270/Primary_Detector/ssd/
mkdir -p $MODEL_PATH

pushd $MODEL_PATH
    if [ ! -f "resnet34_ssd.cambricon" ]; then
      wget -O resnet34_ssd.cambricon http://video.cambricon.com/models/MLU270/Primary_Detector/ssd/resnet34_ssd.cambricon
    else
      echo "resnet34_ssd.cambricon offline model exists."
    fi
    if [ ! -f "label_voc.txt" ]; then
      wget -O label_voc.txt http://video.cambricon.com/models/MLU270/Primary_Detector/ssd/label_voc.txt
    else
      echo "label_voc.txt exists."
    fi
popd
source $CURRENT_DIR/../env.sh
mkdir -p $CURRENT_DIR/output
$CURRENT_DIR/../bin/multi_pipelines  \
    --data_path $CURRENT_DIR/files.list_video \
    --src_frame_rate 30   \
    --raw_img_input=false \
    --config_fname "$CURRENT_DIR/multi_pipelines/first_pipeline.json" \
    --config_fname1 "$CURRENT_DIR/multi_pipelines/second_pipeline.json"
