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
source ${CURRENT_DIR}/../../env.sh

PrintUsages(){
    echo "Usages: run.sh [mlu370/ce3226] [encode_jpeg/encode_video/rtsp/vout] [yolov3/yolov5]"
}

if [ $# -ne 3 ]; then
    PrintUsages
    exit 1
fi

if [[ ${3} != "yolov3" && ${3} != "yolov5" ]]; then
    PrintUsages
    exit 1
fi

mkdir -p ${MODELS_DIR}
if [[ ${1} == "ce3226" ]]; then
    MM_VER=v0.13.0
    if [[ ${3} == "yolov3" ]]; then
        LOCAL_PATH[0]=${MODELS_DIR}/yolov3_${MM_VER}_4b_rgb_uint8.magicmind
        REMOTE_PATH[0]=http://video.cambricon.com/models/magicmind/${MM_VER}/yolov3_${MM_VER}_4b_rgb_uint8.magicmind
    else
        LOCAL_PATH[0]=${MODELS_DIR}/yolov5m_${MM_VER}_4b_rgb_uint8.magicmind
        REMOTE_PATH[0]=http://video.cambricon.com/models/magicmind/${MM_VER}/yolov5m_${MM_VER}_4b_rgb_uint8.magicmind
    fi
    LOCAL_PATH[1]=${MODELS_DIR}/feature_extract_${MM_VER}_4b_rgb_uint8.magicmind
    REMOTE_PATH[1]=http://video.cambricon.com/models/magicmind/${MM_VER}/feature_extract_${MM_VER}_4b_rgb_uint8.magicmind
elif [[ ${1} == "mlu370" ]]; then
    MM_VER=v0.13.0
    if [[ ${3} == "yolov3" ]]; then
        LOCAL_PATH[0]=${MODELS_DIR}/yolov3_${MM_VER}_4b_rgb_uint8.magicmind
        REMOTE_PATH[0]=http://video.cambricon.com/models/magicmind/${MM_VER}/yolov3_${MM_VER}_4b_rgb_uint8.magicmind
    else
        LOCAL_PATH[0]=${MODELS_DIR}/yolov5m_${MM_VER}_4b_rgb_uint8.magicmind
        REMOTE_PATH[0]=http://video.cambricon.com/models/magicmind/${MM_VER}/yolov5m_${MM_VER}_4b_rgb_uint8.magicmind
    fi
    LOCAL_PATH[1]=${MODELS_DIR}/feature_extract_${MM_VER}_4b_rgb_uint8.magicmind
    REMOTE_PATH[1]=http://video.cambricon.com/models/magicmind/${MM_VER}/feature_extract_${MM_VER}_4b_rgb_uint8.magicmind
fi

if [[ ${2} != "encode_jpeg" && ${2} != "encode_video" && ${2} != "rtsp" && ${2} != "vout" ]]; then
    PrintUsages
    exit 1
fi

Download(){
    if [ $# -ne 2 ]; then
        exit -1
    fi
    if [[ ! -f ${1} ]]; then
        wget -O ${1} ${2}
        if [ $? -ne 0 ]; then
            echo "Download ${2} to ${1} failed."
            exit 1
        fi
    fi
}

for i in $(seq 0 `expr ${#LOCAL_PATH[@]} - 1`)
do
    Download ${LOCAL_PATH[$i]} ${REMOTE_PATH[$i]}
done

LABEL_PATH=${MODELS_DIR}/label_map_coco.txt
REMOTE_LABEL_PATH=http://video.cambricon.com/models/labels/label_map_coco.txt
Download ${LABEL_PATH} ${REMOTE_LABEL_PATH}

# generate config file with selected sinker and selected network
pushd ${CURRENT_DIR}
    sed 's/__PLATFORM_PLACEHOLDER__/'"${1}"'/g;s/__SINKER_PLACEHOLDER__/'"${2}"'/g;s/__NN__/'"${3}"'/g' config_template.json &> config.json
popd

${SAMPLES_DIR}/generate_file_list.sh
mkdir -p output
${SAMPLES_DIR}/bin/cns_launcher  \
    --data_path ${SAMPLES_DIR}/files.list_video \
    --src_frame_rate 25   \
    --config_fname ${CURRENT_DIR}/config.json \
    --logtostderr=true

