#!/bin/bash
#*************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# wait_time: When set to 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
#
# @notice: other flags see ${SAMPLES_DIR}/bin/cns_launcher --help
#*************************************************************************#
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
CNSTREAM_ROOT=${CURRENT_DIR}/../../..
SAMPLES_ROOT=${CNSTREAM_ROOT}/samples
MODELS_ROOT=${CNSTREAM_ROOT}/data/models
CONFIGS_ROOT=${SAMPLES_ROOT}/cns_launcher/configs

PrintUsages(){
    echo "Usages: run.sh [mlu220/mlu270] [encode_jpeg/encode_video/display/rtsp]"
}

if [ $# -ne 2 ]; then
    PrintUsages
    exit 1
fi

if [[ ${1} == "mlu220" ]]; then
    MODEL_PATH=${MODELS_ROOT}/coco_pose_b4c4_bgra_mlu220.cambricon
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU220/coco_pose_b4c4_bgra_mlu220.cambricon
elif [[ ${1} == "mlu270" ]]; then
    MODEL_PATH=${MODELS_ROOT}/coco_pose_b4c4_bgra_mlu270.cambricon
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU270/coco_pose_b4c4_bgra_mlu270.cambricon
else
    PrintUsages
    exit 1
fi

if [[ ${2} != "encode_jpeg" && ${2} != "encode_video" && ${2} != "display" && ${2} != "rtsp" ]]; then
    PrintUsages
    exit 1
fi
mkdir -p ${MODELS_ROOT}
if [[ ! -f ${MODEL_PATH} ]]; then
    wget -O ${MODEL_PATH} ${REMOTE_MODEL_PATH}
    if [ $? -ne 0 ]; then
        echo "Download ${REMOTE_MODEL_PATH} to ${MODEL_PATH} failed."
        exit 1
    fi
fi

# generate config file with selected sinker
pushd ${CURRENT_DIR}
    sed 's/__PLATFORM_PLACEHOLDER__/'"${1}"'/g' config_template.json | sed 's/__SINKER_PLACEHOLDER__/'"${2}"'.json/g' &> config.json
popd

${SAMPLES_ROOT}/generate_file_list.sh
mkdir -p output
${SAMPLES_ROOT}/bin/cns_launcher  \
    --data_path ${SAMPLES_ROOT}/files.list_pose_image \
    --src_frame_rate 25 \
    --config_fname ${CURRENT_DIR}/config.json \
    --log_to_stderr=true

