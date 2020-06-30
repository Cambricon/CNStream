#!/bin/bash
#*************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# wait_time: time of one test case. When set tot 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
# device_id: mlu device id
#
# @notice: other flags see ../../bin/demo --help
#*************************************************************************#
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
SAMPLES_DIR=$CURRENT_DIR/../..
MODEL_PATH=$CURRENT_DIR/../../../data/models/MLU270/Classification/resnet50
mkdir -p $MODEL_PATH

cd $MODEL_PATH
    if [ ! -f "resnet50_offline_v1.3.0.cambricon" ]; then
      wget -c http://video.cambricon.com/models/MLU270/Classification/resnet50/resnet50_offline_v1.3.0.cambricon
      wget -c http://video.cambricon.com/models/MLU270/Classification/resnet50/synset_words.txt
    else
      echo "resnet50 offline model exists."
    fi
cd -
MODEL_PATH=$CURRENT_DIR/../../../data/models/MLU270/Primary_Detector/ssd
mkdir -p $MODEL_PATH

cd $MODEL_PATH
    if [ ! -f "resnet34_ssd.cambricon" ]; then
      wget -c  http://video.cambricon.com/models/MLU270/Primary_Detector/ssd/resnet34_ssd.cambricon
      wget -c  http://video.cambricon.com/models/MLU270/Primary_Detector/ssd/label_voc.txt
    else
      echo "resnet34_ssd.cambricon offline model exists."
    fi
cd -

source ../env.sh
mkdir -p output
../../bin/demo  \
    --data_path ../files.list_video \
    --src_frame_rate 60  \
    --wait_time 0 \
    --loop=false \
    --config_fname "ssd_resnet34_and_resnet50_mlu270_config.json" \
    --alsologtostderr
