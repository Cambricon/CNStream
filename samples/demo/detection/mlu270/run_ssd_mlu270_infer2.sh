#!/bin/bash
#*************************************************************************#
# @param
# data_path: Video or image list path
# wait_time: When set to 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
#
# @notice: other flags see ${SAMPLES_DIR}/bin/demo --help
#*************************************************************************#

CURRENT_FILE=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
SAMPLES_DIR=$CURRENT_FILE/../../..
MODEL_PATH=$CURRENT_FILE/../../../../data/models/MLU270/Primary_Detector/ssd/
MODEL_NAME=resnet34_ssd.cambricon
mkdir -p $MODEL_PATH

cd $MODEL_PATH
    if [ ! -f "${MODEL_NAME}" ]; then
      wget -O ${MODEL_NAME} http://video.cambricon.com/models/MLU270/Primary_Detector/ssd/${MODEL_NAME}
      wget -O label_voc.txt http://video.cambricon.com/models/MLU270/Primary_Detector/ssd/label_voc.txt
    else
      echo "${MODEL_NAME} exists."
    fi
cd -

source ${SAMPLES_DIR}/demo/env.sh
mkdir -p $CURRENT_FILE/output
${SAMPLES_DIR}/bin/demo  \
    --data_path ${SAMPLES_DIR}/demo/files.list_video \
    --src_frame_rate 30   \
    --wait_time 0 \
    --loop=false \
    --config_fname "$CURRENT_FILE/ssd_mlu270_config_infer2.json" \
    --log_to_stderr=true
