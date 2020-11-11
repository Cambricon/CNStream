#!/bin/bash
#*************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# wait_time: time of one test case. When set tot 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
#
# @notice: other flags see ./../bin/demo --help
#*************************************************************************#
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
MODEL_PATH=$CURRENT_DIR/../../data/models/MLU270/Primary_Detector/ssd/
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
source $CURRENT_DIR/env.sh
mkdir -p $CURRENT_DIR/output
$CURRENT_DIR/../bin/demo  \
    --data_path $CURRENT_DIR/files.list_video \
    --src_frame_rate 300000   \
    --wait_time 0 \
    --loop=false \
    --raw_img_input=false \
    --config_fname "$CURRENT_DIR/detection_config.json"
    # uncomment to stop measuring performance
    #--perf=false \
    # uncomment to change directory of database file
    #--perf_db_dir="" \
