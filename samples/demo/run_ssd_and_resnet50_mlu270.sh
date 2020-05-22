#!/bin/bash
#*************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# wait_time: time of one test case. When set tot 0, it will automatically exit after the eos signal arrives
# rtsp = true: use rtsp
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
source env.sh
mkdir -p output
./../bin/demo  \
    --data_path ./files.list_video \
    --src_frame_rate 60  \
    --wait_time 0 \
    --rtsp=false \
    --loop=false \
    --config_fname "ssd_resnet34_and_resnet50_mlu270_config.json" \
    --alsologtostderr
