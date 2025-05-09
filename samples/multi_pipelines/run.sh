#!/bin/bash
#*************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
#
# @notice: other flags see ./../bin/multi_pipelines --help
#*************************************************************************#

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
source ${CURRENT_DIR}/../env.sh
PrintUsages(){
    echo "Usages: run.sh [mlu370] [encode_jpeg/encode_video/rtsp]"
}

if [ $# -ne 2 ]; then
    PrintUsages
    exit 1
fi

if [[ ${1} == "mlu370" ]]; then
    MODEL_PATH=${MODELS_DIR}/yolov3_v0.13.0_4b_rgb_uint8.magicmind
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/magicmind/v0.13.0/yolov3_v0.13.0_4b_rgb_uint8.magicmind
else
    PrintUsages
    exit 1
fi

if [[ ${2} != "encode_jpeg" && ${2} != "encode_video" && ${2} != "rtsp" ]]; then
    PrintUsages
    exit 1
fi

LABEL_PATH=${MODELS_DIR}/label_map_coco.txt
REMOTE_LABEL_PATH=http://video.cambricon.com/models/labels/label_map_coco.txt

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

FIRST_CONIFG_PATH=${SAMPLES_DIR}/multi_pipelines/first
SECOND_CONIFG_PATH=${SAMPLES_DIR}/multi_pipelines/second
mkdir -p ${FIRST_CONIFG_PATH}
mkdir -p ${SECOND_CONIFG_PATH}

cp ${CONFIGS_DIR}/decode_config.json ${FIRST_CONIFG_PATH}/
cp ${CONFIGS_DIR}/yolov3_object_detection_${1}.json ${FIRST_CONIFG_PATH}/
cp ${CONFIGS_DIR}/sinker_configs/${2}.json ${FIRST_CONIFG_PATH}/

cp ${CONFIGS_DIR}/decode_config.json ${SECOND_CONIFG_PATH}/
sed "s/\"device_id\".*0/\"device_id\"\ :\ 1/g" ${CONFIGS_DIR}/decode_config.json &>  ${SECOND_CONIFG_PATH}/decode_config.json
sed "s/\"device_id\".*0/\"device_id\"\ :\ 1/g" ${CONFIGS_DIR}/yolov3_object_detection_${1}.json &>  ${SECOND_CONIFG_PATH}/yolov3_object_detection_${1}.json
sed "s/\"device_id\".*0/\"device_id\"\ :\ 1/g;s/\"rtsp_port\".*/\"rtsp_port\"\ :\ 8554,/g" ${CONFIGS_DIR}/sinker_configs/${2}.json  &>  ${SECOND_CONIFG_PATH}/${2}.json

# generate config file with selected sinker and selected platform
pushd ${CURRENT_DIR}
    sed 's/__PLATFORM_PLACEHOLDER__/'"${1}"'/g;s/__SINKER_PLACEHOLDER__/'"${2}"'/g' config_template.json &> ${FIRST_CONIFG_PATH}/first_config.json
popd

pushd ${CURRENT_DIR}
    sed 's/__PLATFORM_PLACEHOLDER__/'"${1}"'/g;s/__SINKER_PLACEHOLDER__/'"${2}"'/g' config_template.json &> ${SECOND_CONIFG_PATH}/second_config.json
popd

${SAMPLES_DIR}/generate_file_list.sh
mkdir -p $CURRENT_DIR/output
${SAMPLES_DIR}/bin/multi_pipelines  \
    --data_path ${SAMPLES_DIR}/files.list_video \
    --src_frame_rate 25   \
    --config_fname "${FIRST_CONIFG_PATH}/first_config.json" \
    --config_fname1 "${SECOND_CONIFG_PATH}/second_config.json"
