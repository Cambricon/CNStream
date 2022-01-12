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
    echo "Usages: run.sh mlu220/mlu270/mlu370"
}

if [ $# -ne 1 ]; then
    PrintUsages
    exit 1
fi

if [[ ${1} == "mlu220" ]]; then
    MODEL_PATH=${MODELS_ROOT}/resnet18_b4c4_bgra_mlu220.cambricon
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU220/resnet18_b4c4_bgra_mlu220.cambricon
elif [[ ${1} == "mlu270" ]]; then
    MODEL_PATH=${MODELS_ROOT}/resnet50_b16c16_bgra_mlu270.cambricon
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU270/resnet50_b16c16_bgra_mlu270.cambricon
elif [[ ${1} == "mlu370" ]]; then
    MODEL_PATH=${MODELS_ROOT}/resnet50_nhwc.model
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU370/resnet50_nhwc_tfu_0.5_int8_fp16.model
else
    PrintUsages
    exit 1
fi

LABEL_PATH=${MODELS_ROOT}/synset_words.txt
REMOTE_LABEL_PATH=http://video.cambricon.com/models/labels/synset_words.txt
mkdir -p ${MODELS_ROOT}
if [[ ! -f ${MODEL_PATH} ]]; then
    wget -O ${MODEL_PATH} ${REMOTE_MODEL_PATH}
    if [ $? -ne 0 ]; then
        echo "Download ${REMOTE_MODEL_PATH} to ${MODEL_PATH} failed."
        exit 1
    fi
fi

if [[ ! -f ${LABEL_PATH} ]]; then
    wget -O ${LABEL_PATH} ${REMOTE_LABEL_PATH}
    if [ $? -ne 0 ]; then
        echo "Download ${REMOTE_LABEL_PATH} to ${LABEL_PATH} failed."
        exit 1
    fi
fi

# generate config file with selected platform
pushd ${CURRENT_DIR}
    sed 's/__PLATFORM_PLACEHOLDER__/'"${1}"'/g' config_template.json &> config.json
popd

${SAMPLES_ROOT}/generate_file_list.sh
mkdir -p output
${SAMPLES_ROOT}/bin/cns_launcher  \
    --data_path ${SAMPLES_ROOT}/files.list_image \
    --src_frame_rate 25   \
    --config_fname ${CURRENT_DIR}/config.json \
    --log_to_stderr=true
