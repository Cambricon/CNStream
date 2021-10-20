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
    echo "Usages: run.sh [encode_jpeg/encode_video/display/rtsp]"
}

if [ $# -ne 1 ]; then
    PrintUsages
    exit 1
fi

if [[ ${1} != "encode_jpeg" && ${1} != "encode_video" && ${1} != "display" && ${1} != "rtsp" ]]; then
    PrintUsages
    exit 1
fi

MODEL_PATHS[0]=${MODELS_ROOT}/resnet34_ssd_b16c16_mlu270.cambricon
REMOTE_MODEL_PATHS[0]=http://video.cambricon.com/models/MLU270/resnet34_ssd_b16c16_mlu270.cambricon
MODEL_PATHS[1]=${MODELS_ROOT}/vehicle_cts_b4c4_bgra_mlu270.cambricon
REMOTE_MODEL_PATHS[1]=http://video.cambricon.com/models/MLU270/vehicle_cts_b4c4_bgra_mlu270.cambricon
LABEL_PATH=${MODELS_ROOT}/label_voc.txt
REMOTE_LABEL_PATH=http://video.cambricon.com/models/labels/label_voc.txt

mkdir -p ${MODELS_ROOT}
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

# generate config file with selected sinker
pushd ${CURRENT_DIR}
    sed 's/__SINKER_PLACEHOLDER__/'"${1}"'.json/g' config_template.json &> config.json
popd

${SAMPLES_ROOT}/generate_file_list.sh
mkdir -p output
${SAMPLES_ROOT}/bin/cns_launcher  \
    --data_path ${SAMPLES_ROOT}/files.list_video \
    --src_frame_rate 25 \
    --config_fname ${CURRENT_DIR}/config.json \
    --log_to_stderr=true
