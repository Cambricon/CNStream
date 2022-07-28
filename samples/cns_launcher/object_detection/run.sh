#!/bin/bash
#************************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# wait_time: When set to 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
#
# @notice: other flags see ${SAMPLES_DIR}/bin/cns_launcher --help
#          when USB camera is the input source, please add 'usb' as the third parameter
#************************************************************************************#
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)

source ${CURRENT_DIR}/../../env.sh

PrintUsages(){
    echo "Usages: run.sh [mlu220/mlu270/mlu370] [encode_jpeg/encode_video/display/rtsp/kafka] $1"
}

${SAMPLES_DIR}/generate_file_list.sh

if [[ $# == 3 ]];then
    if [[ ${3} == "usb" ]];then
        #change the input URL to usb device.
        echo "/dev/video0" > ${SAMPLES_DIR}/files.list_video
    else
        PrintUsages "usb"
        exit 1
    fi
else
    if [[ $# != 2 ]];then
        PrintUsages
        exit 1
    fi
fi

if [[ ${1} == "mlu220" ]]; then
    MODEL_PATH=${MODELS_DIR}/yolov3_b4c4_argb_mlu220.cambricon
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU220/yolov3_b4c4_argb_mlu220.cambricon
elif [[ ${1} == "mlu270" ]]; then
    MODEL_PATH=${MODELS_DIR}/yolov3_b4c4_argb_mlu270.cambricon
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU270/yolov3_b4c4_argb_mlu270.cambricon
elif [[ ${1} == "mlu370" ]]; then
    MODEL_PATH=${MODELS_DIR}/yolov3_nhwc.model
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU370/yolov3_nhwc_tfu_0.8.2_uint8_int8_fp16.model
else
    PrintUsages
    exit 1
fi

if [[ ${2} != "encode_jpeg" && ${2} != "encode_video" && ${2} != "display" && ${2} != "rtsp"  && ${2} != "kafka" ]]; then
    PrintUsages
    exit 1
fi

LABEL_PATH=${MODELS_DIR}/label_map_coco.txt
REMOTE_LABEL_PATH=http://video.cambricon.com/models/labels/label_map_coco.txt

mkdir -p ${MODELS_DIR}

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

# generate config file with selected sinker and selected platform
pushd ${CURRENT_DIR}
    if [[ $# == 2 ]];then
        sed 's/__PLATFORM_PLACEHOLDER__/'"${1}"'/g' config_template.json | sed 's/__SINKER_PLACEHOLDER__/'"${2}"'.json/g' &> config.json
    else
        #Because MLU may not support some usb cameras' codec format like AV_CODEC_ID_MSMPEG4V1, here we prefer to select CPU decoder.
        sed 's/decoder_type.*mlu/decoder_type\"\:\"cpu/g' ${CONFIGS_DIR}/decode_config.json &> ${CONFIGS_DIR}/cpu_decode_config.json
        sed 's/__PLATFORM_PLACEHOLDER__/'"${1}"'/g' config_template.json | sed 's/__SINKER_PLACEHOLDER__/'"${2}"'.json/g' | sed 's/decode_config/cpu_decode_config/g' &> config.json
    fi
popd


mkdir -p output
${SAMPLES_DIR}/bin/cns_launcher  \
    --data_path ${SAMPLES_DIR}/files.list_video \
    --src_frame_rate 25 \
    --config_fname ${CURRENT_DIR}/config.json \
    --logtostderr=true

