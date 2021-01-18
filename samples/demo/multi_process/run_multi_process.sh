#!/bin/bash
CURRENT_FILE=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
start_process1() {
$CURRENT_FILE/../../bin/demo  \
    --data_path $CURRENT_FILE/../files.list_video \
    --src_frame_rate 100   \
    --wait_time 0 \
    --loop=false \
    --config_fname "$CURRENT_FILE/config_process1.json" \
    --alsologtostderr
}

start_process2() {
$CURRENT_FILE/../../bin/demo  \
    --data_path $CURRENT_FILE/../files.list_video \
    --src_frame_rate 100   \
    --wait_time 0 \
    --loop=false \
    --config_fname "$CURRENT_FILE/config_process2.json" \
    --alsologtostderr
}

SAMPLES_DIR=$CURRENT_FILE/../..
MODEL_PATH=$CURRENT_FILE/../../../data/models/MLU270/Classification/resnet50
mkdir -p $MODEL_PATH

cd $MODEL_PATH
    if [ ! -f "resnet50_offline_v1.3.0.cambricon" ]; then
      wget -O resnet50_offline_v1.3.0.cambricon  http://video.cambricon.com/models/MLU270/Classification/resnet50/resnet50_offline_v1.3.0.cambricon
    else
      echo "resnet50 offline model exists."
    fi
cd -

source $CURRENT_FILE/../env.sh
mkdir -p $CURRENT_FILE/output
start_process1 & start_process2
wait
