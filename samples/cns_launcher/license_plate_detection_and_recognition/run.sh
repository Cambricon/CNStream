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
    echo "Usages: run.sh [mlu370/ce3226] [encode_jpeg/encode_video/rtsp/vout]"
}

if [ $# -ne 2 ]; then
    PrintUsages
    exit 1
fi

if [[ ${1} == "ce3226" ]]; then
    MM_VER=v0.13.0
    MODEL_PATHS[0]=${MODELS_DIR}/yolov3_${MM_VER}_4b_rgb_uint8.magicmind
    REMOTE_MODEL_PATHS[0]=http://video.cambricon.com/models/magicmind/${MM_VER}/yolov3_${MM_VER}_4b_rgb_uint8.magicmind
    MODEL_PATHS[1]=${MODELS_DIR}/mobilenet_ssd_plate_detection_${MM_VER}_4b_bgr_fp32.magicmind
    REMOTE_MODEL_PATHS[1]=http://video.cambricon.com/models/magicmind/${MM_VER}/mobilenet_ssd_plate_detection_${MM_VER}_4b_bgr_fp32.magicmind
    MODEL_PATHS[2]=${MODELS_DIR}/lprnet_${MM_VER}_4b_bgr_uint8.magicmind
REMOTE_MODEL_PATHS[2]=http://video.cambricon.com/models/magicmind/${MM_VER}/lprnet_${MM_VER}_4b_bgr_uint8.magicmind
elif [[ ${1} == "mlu370" ]]; then
    MM_VER=v0.13.0
    MODEL_PATHS[0]=${MODELS_DIR}/yolov3_${MM_VER}_4b_rgb_uint8.magicmind
    REMOTE_MODEL_PATHS[0]=http://video.cambricon.com/models/magicmind/${MM_VER}/yolov3_${MM_VER}_4b_rgb_uint8.magicmind
    MODEL_PATHS[1]=${MODELS_DIR}/mobilenet_ssd_plate_detection_${MM_VER}_4b_bgr_fp32.magicmind
    REMOTE_MODEL_PATHS[1]=http://video.cambricon.com/models/magicmind/${MM_VER}/mobilenet_ssd_plate_detection_${MM_VER}_4b_bgr_fp32.magicmind
    MODEL_PATHS[2]=${MODELS_DIR}/lprnet_${MM_VER}_4b_bgr_uint8.magicmind
    REMOTE_MODEL_PATHS[2]=http://video.cambricon.com/models/magicmind/${MM_VER}/lprnet_${MM_VER}_4b_bgr_uint8.magicmind
else
    PrintUsages
    exit 1
fi

if [[ ${2} != "encode_jpeg" && ${2} != "encode_video" && ${2} != "rtsp" && ${2} != "vout" ]]; then
    PrintUsages
    exit 1
fi

LABEL_PATH=${MODELS_DIR}/label_map_coco.txt
REMOTE_LABEL_PATH=http://video.cambricon.com/models/labels/label_map_coco.txt
mkdir -p ${MODELS_DIR}

for i in $(seq 0 `expr ${#MODEL_PATHS[@]} - 1`)
do
  if [[ ! -f ${MODEL_PATHS[$i]} ]]; then
      wget -O ${MODEL_PATHS[$i]} ${REMOTE_MODEL_PATHS[$i]}
      if [ $? -ne 0 ]; then
          echo "Download ${REMOTE_MODEL_PATHS[$i]} to ${MODEL_PATHS[$i]} failed."
          exit 1
      fi
  fi
done

if [[ ! -f ${LABEL_PATH} ]]; then
    wget -O ${LABEL_PATH} ${REMOTE_LABEL_PATH}
    if [ $? -ne 0 ]; then
        echo "Download ${REMOTE_LABEL_PATH} to ${LABEL_PATH} failed."
        exit 1
    fi
fi

# generate config file with selected sinker and selected platform
pushd ${CURRENT_DIR}
    sed 's/__PLATFORM_PLACEHOLDER__/'"${1}"'/g;s/__SINKER_PLACEHOLDER__/'"${2}"'/g' config_template.json &> config.json
popd

${SAMPLES_DIR}/generate_file_list.sh
mkdir -p output
${SAMPLES_DIR}/bin/cns_launcher  \
    --data_path ${SAMPLES_DIR}/files.list_video \
    --src_frame_rate 25 \
    --config_fname ${CURRENT_DIR}/config.json \
    --logtostderr=true

