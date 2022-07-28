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
    echo "Usages: run.sh [mlu220/mlu270/mlu370] [encode_jpeg/encode_video/display/rtsp] [yolov3/yolov5]"
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
if [[ ${1} == "mlu220" ]]; then
    if [[ ${3} == "yolov3" ]]; then
        LOCAL_PATH[0]=${MODELS_DIR}/yolov3_b4c4_argb_mlu220.cambricon
        REMOTE_PATH[0]=http://video.cambricon.com/models/MLU220/yolov3_b4c4_argb_mlu220.cambricon
    else
        LOCAL_PATH[0]=${MODELS_DIR}/yolov5_b4c4_rgb_mlu220.cambricon
        REMOTE_PATH[0]=http://video.cambricon.com/models/MLU220/yolov5/yolov5_b4c4_rgb_mlu220.cambricon
    fi
    LOCAL_PATH[1]=${MODELS_DIR}/feature_extract_for_tracker_b4c4_argb_mlu220.cambricon
    REMOTE_PATH[1]=http://video.cambricon.com/models/MLU220/feature_extract_for_tracker_b4c4_argb_mlu220.cambricon
elif [[ ${1} == "mlu270" ]]; then
    if [[ ${3} == "yolov3" ]]; then
        LOCAL_PATH[0]=${MODELS_DIR}/yolov3_b4c4_argb_mlu270.cambricon
        REMOTE_PATH[0]=http://video.cambricon.com/models/MLU270/yolov3_b4c4_argb_mlu270.cambricon
    else
        LOCAL_PATH[0]=${MODELS_DIR}/yolov5_b4c4_rgb_mlu270.cambricon
        REMOTE_PATH[0]=http://video.cambricon.com/models/MLU270/yolov5/yolov5_b4c4_rgb_mlu270.cambricon
    fi
    LOCAL_PATH[1]=${MODELS_DIR}/feature_extract_for_tracker_b4c4_argb_mlu270.cambricon
    REMOTE_PATH[1]=http://video.cambricon.com/models/MLU270/feature_extract_for_tracker_b4c4_argb_mlu270.cambricon
elif [[ ${1} == "mlu370" ]]; then
    if [[ ${3} == "yolov3" ]]; then
        LOCAL_PATH[0]=${MODELS_DIR}/yolov3_nhwc.model
        REMOTE_PATH[0]=http://video.cambricon.com/models/MLU370/yolov3_nhwc_tfu_0.8.2_uint8_int8_fp16.model
    else
        LOCAL_PATH[0]=${MODELS_DIR}/yolov5_nhwc.model
        REMOTE_PATH[0]=http://video.cambricon.com/models/MLU370/yolov5_nhwc_tfu_0.8.2_uint8_int8_fp16.model
    fi
    LOCAL_PATH[1]=${MODELS_DIR}/feature_extract_nhwc.model
    REMOTE_PATH[1]=http://video.cambricon.com/models/MLU370/feature_extract_nhwc_tfu_0.8.2_fp32_int8_fp16.model
else
    PrintUsages
    exit 1
fi

if [[ ${2} != "encode_jpeg" && ${2} != "encode_video" && ${2} != "display" && ${2} != "rtsp" ]]; then
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

# generate config file with selected sinker and selected platform
pushd ${CURRENT_DIR}
    sed 's/__PLATFORM_PLACEHOLDER__/'"${1}"'/g;s/__NN__/'"${3}"'/g' config_template.json | sed 's/__SINKER_PLACEHOLDER__/'"${2}"'.json/g' &> config.json
popd

${SAMPLES_DIR}/generate_file_list.sh
mkdir -p output
${SAMPLES_DIR}/bin/cns_launcher  \
    --data_path ${SAMPLES_DIR}/files.list_video \
    --src_frame_rate 25   \
    --config_fname ${CURRENT_DIR}/config.json \
    --logtostderr=true

