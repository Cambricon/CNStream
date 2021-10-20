#!/bin/bash
#*************************************************************************#
# @param
# input_url: video path or image url. for example: /path/to/your/video.mp4, /parth/to/your/images/%d.jpg
# how_to_show: [image] means dump as images, [video] means dump as video (output.avi), [display] means show in a window
#              default value is [display]
# output_dir: where to store the output file. default value is "./"
# output_frame_rate: output frame rate, valid when [how_to_show] set to [video] or [display]. default value is 25
# label_path: label path
# model_path: your offline model path
# func_name:  function name in your model, default value is [subnet0]
# keep_aspect_ratio: keep aspect ratio for image scaling in model's preprocessing. default value is false
# mean_value: mean values in format "100.0, 100.0, 100.0" in BGR order. default value is "0, 0, 0"
# std: std values in format "100.0, 100.0, 100.0", in BGR order. default value is "1.0, 1.0, 1.0"
# model_input_pixel_format: input image format for your model. BGRA/RGBA/ARGB/ABGR/RGB/BGR is supported. default value is BGRA
# dev_id: device odinal index. default value is 0
#*************************************************************************#

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
CNSTREAM_ROOT=${CURRENT_DIR}/../..
SAMPLES_ROOT=${CNSTREAM_ROOT}/samples
MODELS_ROOT=${CNSTREAM_ROOT}/data/models

PrintUsages(){
    echo "Usages: run.sh [mlu220/mlu270]"
}

if [ $# -ne 1 ]; then
    PrintUsages
    exit 1
fi

if [[ ${1} == "mlu220" ]]; then
    MODEL_PATH=${MODELS_ROOT}/yolov3_b4c4_argb_mlu220.cambricon
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU220/Primary_Detector/YOLOv3/yolov3_4c4b_argb_220_v1.5.0.cambricon
elif [[ ${1} == "mlu270" ]]; then
    MODEL_PATH=${MODELS_ROOT}/yolov3_b4c4_argb_mlu270.cambricon
    REMOTE_MODEL_PATH=http://video.cambricon.com/models/MLU270/yolov3/yolov3_4c4b_argb_270_v1.5.0.cambricon
else
    PrintUsages
    exit 1
fi

LABEL_PATH=${MODELS_ROOT}/label_map_coco.txt
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

$CURRENT_DIR/../bin/simple_run_pipeline  \
  --input_url $CURRENT_DIR/../../data/videos/cars.mp4 \
  --model_path ${MODEL_PATH} \
  --label_path ${LABEL_PATH} \
  --keep_aspect_ratio true  \
  --model_input_pixel_format ARGB  \
  --how_to_show video  \
  --dev_id 0

