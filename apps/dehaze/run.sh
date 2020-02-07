#/bin/bash
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
# @notice: other flags see ./../bin/dehaze_demo --help
#*************************************************************************#
source env.sh
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)

pushd $CURRENT_DIR
    if [ ! -f "$CURRENT_DIR/Dehaze.cambricon" ]; then
      wget -O Dehaze.cambricon http://video.cambricon.com/models/MLU100/Dehaze/Dehaze.cambricon
    else
      echo "cambricom file exists."
    fi
popd

./bin/dehaze_demo   \
    --data_path ./files.list_image \
    --src_frame_rate 30   \
    --wait_time 0 \
    --rtsp=false \
    --loop=false \
    --config_fname "dehaze_config.json" \
    --alsologtostderr
