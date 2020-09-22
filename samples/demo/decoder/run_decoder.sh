#!/bin/bash
#*************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# wait_time: When set to 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
#
# @notice: other flags see ../../bin/demo --help
#*************************************************************************#
CURRENT_FILE=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
source $CURRENT_FILE/../env.sh
$CURRENT_FILE/../../bin/demo  \
    --data_path $CURRENT_FILE/../files.list_video \
    --src_frame_rate 3000   \
    --wait_time 0 \
    --loop=false \
    --config_fname "$CURRENT_FILE/decoder_config.json" \
    --alsologtostderr
