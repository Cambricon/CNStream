#!/bin/bash
#************************************************************************************#
# @param
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# wait_time: When set to 0, it will automatically exit after the eos signal arrives
# loop = true: loop through video
#
# @notice: other flags see ${SAMPLES_DIR}/bin/cns_launcher --help
#************************************************************************************#
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)

source ${CURRENT_DIR}/../../env.sh

PrintUsages(){
    echo "Usages: run.sh [mlu220/mlu270/mlu370] [encode_jpeg/encode_video/display/rtsp/kafka] [yolov3/yolov5]"
}

${SAMPLES_DIR}/generate_file_list.sh

if [[ $# != 3 ]];then
    PrintUsages
    exit 1
fi

if [[ ${3} != "yolov3" && ${3} != "yolov5" ]]; then
    PrintUsages
    exit 1
fi

if [[ ${1} == "mlu220" ]]; then
    if [[ ${3} == "yolov3" ]]; then
        MODEL_PATH=${MODELS_DIR}/yolov3_b4c4_argb_mlu220.cambricon
        REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU220/yolov3_b4c4_argb_mlu220.cambricon
    else
        MODEL_PATH=${MODELS_DIR}/yolov5_b4c4_rgb_mlu220.cambricon
        REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU220/yolov5/yolov5_b4c4_rgb_mlu220.cambricon
    fi
elif [[ ${1} == "mlu270" ]]; then
    if [[ ${3} == "yolov3" ]]; then
        MODEL_PATH=${MODELS_DIR}/yolov3_b4c4_argb_mlu270.cambricon
        REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU270/yolov3_b4c4_argb_mlu270.cambricon
    else
        MODEL_PATH=${MODELS_DIR}/yolov5_b4c4_rgb_mlu270.cambricon
        REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU270/yolov5/yolov5_b4c4_rgb_mlu270.cambricon
    fi
elif [[ ${1} == "mlu370" ]]; then
    MM_VER=v1.1.0
    if [[ ${3} == "yolov3" ]]; then
        MODEL_PATH=${MODELS_DIR}/yolov3_${MM_VER}_4b_rgb_uint8.magicmind
        REMOTE_MODEL_PATH=http://video.cambricon.com/models/magicmind/${MM_VER}/yolov3_${MM_VER}_4b_rgb_uint8.magicmind
    else
        MODEL_PATH=${MODELS_DIR}/yolov5m_${MM_VER}_4b_rgb_uint8.magicmind
        REMOTE_MODEL_PATH=http://video.cambricon.com/models/magicmind/${MM_VER}/yolov5m_${MM_VER}_4b_rgb_uint8.magicmind
    fi
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

# generate config file with selected sinker
pushd ${CURRENT_DIR}
    sed 's/__PLATFORM_PLACEHOLDER__/'"${1}"'/g;s/__NN__/'"${3}"'/g' config_template.json | sed 's/__SINKER_PLACEHOLDER__/'"${2}"'.json/g' &> config.json
popd


mkdir -p output
${SAMPLES_DIR}/bin/cns_launcher  \
    --data_path ${SAMPLES_DIR}/files.list_video \
    --src_frame_rate 25 \
    --config_fname ${CURRENT_DIR}/config.json \
    --logtostderr=true

