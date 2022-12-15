#!/bin/bash
#*************************************************************************#
# @param
# input_url: video path or image url. for example: /path/to/your/video.mp4, /parth/to/your/images/%d.jpg
# how_to_show: [image] means dump as images, [video] means dump as video (output.avi),
#              default value is [video]
# output_dir: where to store the output file. default value is "./"
# output_frame_rate: output frame rate, valid when [how_to_show] set to [video]. default value is 25
# label_path: label path
# model_path: your offline model path
# func_name:  function name in your model, default value is [subnet0]
# keep_aspect_ratio: keep aspect ratio for image scaling in model's preprocessing. default value is false
# pad_value: pad values in format "128, 128, 128" in model input pixel format order,
#            default value is "114, 114, 114"
# mean_value: mean values in format "100.0, 100.0, 100.0" in model input pixel format order,
#             default value is "0, 0, 0"
# std: std values in format "100.0, 100.0, 100.0", in model input pixel format order,
#      default value is "1.0, 1.0, 1.0"
# model_input_pixel_format: input image format for your model.RGB/BGR is supported. default value is RGB
# dev_id: device odinal index. default value is 0
#*************************************************************************#

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
source ${CURRENT_DIR}/../env.sh
PrintUsages(){
    echo "Usages: run.sh [mlu370/ce3226]"
}

if [ $# -ne 1 ]; then
    PrintUsages
    exit 1
fi

if [[ ${1} == "mlu370" ]]; then
    MM_VER=v0.13.0
    MODEL_PATH=${MODELS_DIR}/yolov5m_${MM_VER}_4b_rgb_uint8.magicmind
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/magicmind/${MM_VER}/yolov5m_${MM_VER}_4b_rgb_uint8.magicmind
elif [[ ${1} == "ce3226" ]]; then
    MM_VER=v0.13.0
    MODEL_PATH=${MODELS_DIR}/yolov5m_${MM_VER}_4b_rgb_uint8.magicmind
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/magicmind/${MM_VER}/yolov5m_${MM_VER}_4b_rgb_uint8.magicmind
else
    PrintUsages
    exit 1
fi

LABEL_PATH=${MODELS_DIR}/label_map_coco.txt
REMOTE_LABEL_PATH=http://video.cambricon.com/models/MLU270/yolov3/label_map_coco.txt

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

OUTPUT_DIR=${CURRENT_DIR}/output
mkdir -p ${OUTPUT_DIR}

$CURRENT_DIR/../bin/simple_run_pipeline  \
  --input_url $CURRENT_DIR/../../data/videos/cars.mp4 \
  --input_num 1 \
  --model_path ${MODEL_PATH} \
  --label_path ${LABEL_PATH} \
  --keep_aspect_ratio true  \
  --model_input_pixel_format RGB  \
  --how_to_show video  \
  --output_dir ${OUTPUT_DIR} \
  --dev_id 0

