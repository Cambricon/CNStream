#!/bin/bash
#*************************************************************************#
# @param
# data_path: Video or image list path
# wait_time: When set to 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
#
# @notice: other flags see ./../../bin/demo --help
#*************************************************************************#
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
SAMPLES_DIR=$CURRENT_DIR/../..
MODEL_PATH=$CURRENT_DIR/../../../data/models/MLU270/Classification/resnet50
mkdir -p $MODEL_PATH

cd $MODEL_PATH
    if [ ! -f "resnet50_offline_v1.3.0.cambricon" ]; then
      wget -O resnet50_offline_v1.3.0.cambricon  http://video.cambricon.com/models/MLU270/Classification/resnet50/resnet50_offline_v1.3.0.cambricon
    else
      echo "resnet50 offline model exists."
    fi
cd -


source ../env.sh
mkdir -p output
./../../bin/demo  \
    --data_path ./../files.list_video \
    --src_frame_rate 100   \
    --wait_time 0 \
    --loop=false \
    --config_fname "config_process2.json" \
    --alsologtostderr \
    --perf_db_dir="./process2"
