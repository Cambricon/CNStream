#!/bin/bash
#*************************************************************************#
# @param
# drop_rate: Decode Drop frame rate (0~1)
# src_frame_rate: frame rate for send data
# data_path: Video or image list path
# model_path: offline model path
# label_path: label path
# postproc_name: postproc class name (PostprocSsd)
# wait_time: time of one test case. When set tot 0, it will automatically exit after the eos signal arrives
# rtsp = true: use rtsp
# dump_dir: dump result videos to this directory
# loop = true: loop through video
# device_id: mlu device id
#
# @notice: other flags see ./../bin/second_structure --help
#*************************************************************************#
source env.sh

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)

pushd $CURRENT_DIR
    mkdir -p $CURRENT_DIR/secondary_models
    mkdir -p $CURRENT_DIR/secondary_models/Secondary_CarBrand
    if [ ! -f "$CURRENT_DIR/secondary_models/Secondary_CarBrand/resnet18_car.cambricon" ]; then
        wget -P $CURRENT_DIR/secondary_models/Secondary_CarBrand  http://video.cambricon.com/models/MLU100/secondary_models/Secondary_CarBrand/resnet18_car.cambricon
    fi
    if [ ! -f "$CURRENT_DIR/secondary_models/Secondary_CarBrand/car_label.txt" ]; then
        wget -P $CURRENT_DIR/secondary_models/Secondary_CarBrand  http://video.cambricon.com/models/MLU100/secondary_models/Secondary_CarBrand/car_label.txt
    fi
    mkdir -p $CURRENT_DIR/secondary_models/Secondary_CarModel/audi
    if [ ! -f "$CURRENT_DIR/secondary_models/Secondary_CarModel/audi/resnet18_Audi.cambricon" ]; then
        wget -P $CURRENT_DIR/secondary_models/Secondary_CarModel/audi  http://video.cambricon.com/models/MLU100/secondary_models/Secondary_CarModel/audi/resnet18_Audi.cambricon
    fi
    if [ ! -f "$CURRENT_DIR/secondary_models/Secondary_CarModel/audi/Audi_label.txt" ]; then
        wget -P $CURRENT_DIR/secondary_models/Secondary_CarModel/audi  http://video.cambricon.com/models/MLU100/secondary_models/Secondary_CarModel/audi/Audi_label.txt
    fi
    mkdir -p $CURRENT_DIR/secondary_models/Secondary_CarModel/bmw
    if [ ! -f "$CURRENT_DIR/secondary_models/Secondary_CarModel/bmw/resnet18_BMW.cambricon" ]; then
        wget -P $CURRENT_DIR/secondary_models/Secondary_CarModel/bmw  http://video.cambricon.com/models/MLU100/secondary_models/Secondary_CarModel/bmw/resnet18_BMW.cambricon
    fi
    if [ ! -f "$CURRENT_DIR/secondary_models/Secondary_CarModel/bmw/BMW_label.txt" ]; then
        wget -P $CURRENT_DIR/secondary_models/Secondary_CarModel/bmw  http://video.cambricon.com/models/MLU100/secondary_models/Secondary_CarModel/bmw/BMW_label.txt
    fi
    mkdir -p $CURRENT_DIR/secondary_models/Secondary_CarModel/chevrolet
    if [ ! -f "$CURRENT_DIR/secondary_models/Secondary_CarModel/chevrolet/resnet18_Chevrolet.cambricon" ]; then
        wget -P $CURRENT_DIR/secondary_models/Secondary_CarModel/chevrolet  http://video.cambricon.com/models/MLU100/secondary_models/Secondary_CarModel/chevrolet/resnet18_Chevrolet.cambricon
    fi
    if [ ! -f "$CURRENT_DIR/secondary_models/Secondary_CarModel/chevrolet/Chevrolet_label.txt" ]; then
        wget -P $CURRENT_DIR/secondary_models/Secondary_CarModel/chevrolet  http://video.cambricon.com/models/MLU100/secondary_models/Secondary_CarModel/chevrolet/Chevrolet_label.txt
    fi
    mkdir -p $CURRENT_DIR/secondary_models/Secondary_CarModel/ford
    if [ ! -f "$CURRENT_DIR/secondary_models/Secondary_CarModel/ford/resnet18_Ford.cambricon" ]; then
        wget -P $CURRENT_DIR/secondary_models/Secondary_CarModel/ford  http://video.cambricon.com/models/MLU100/secondary_models/Secondary_CarModel/ford/resnet18_Ford.cambricon
    fi
    if [ ! -f "$CURRENT_DIR/secondary_models/Secondary_CarModel/ford/Ford_label.txt" ]; then
        wget -P $CURRENT_DIR/secondary_models/Secondary_CarModel/ford  http://video.cambricon.com/models/MLU100/secondary_models/Secondary_CarModel/ford/Ford_label.txt
    fi
popd

mkdir -p output
./../bin/second_structure  \
    --data_path ./files.list_video \
    --src_frame_rate 30   \
    --wait_time 0 \
    --rtsp=false \
    --loop=false \
    --config_fname "second_structure_config.json" \
    --alsologtostderr
