#!/bin/bash
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/neuware/lib64/

src_dir="$CURRENT_DIR/../../data"
models_dir="$CURRENT_DIR/../../data/models"
dst_dir="$CURRENT_DIR/src/webui/static"
pushd $dst_dir
  if [ ! -d $dst_dir/data ]; then
    echo "generate source data"
    mkdir data
    cp $src_dir/videos/cars.mp4 data/
    cp $src_dir/videos/1080P.h264 data/
  else
    if [ ! -f $dst_dir/data/cars.mp4 ]; then
      echo "copy cars.mp4"
      cp $src_dir/videos/cars.mp4 data/
    fi
    if [ ! -f $dst_dir/data/1080P.h264 ]; then
      echo "copy 1080P.h264"
      cp $src_dir/videos/1080P.h264 data/
    fi
  fi

  pushd $models_dir
    resnet50_model_name="resnet50_v0.13.0_4b_rgb_uint8.magicmind"
    resnet50_label_name="synset_words.txt"
    if [ ! -f $resnet50_model_name ]; then
      echo "download renset50"
      wget -O $resnet50_model_name http://video.cambricon.com/models/magicmind/v0.13.0/resnet50_v0.13.0_4b_rgb_uint8.magicmind
    fi
    if [ ! -f $resnet50_label_name ]; then
      echo "download renset50 label"
      wget -O $resnet50_label_name http://video.cambricon.com/models/labels/synset_words.txt
    fi
    yolov5_model_name="yolov5m_v0.13.0_4b_rgb_uint8.magicmind"
    yolov5_label_name="label_map_coco.txt"
    if [ ! -f $yolov5_model_name ]; then
      echo "download yolov5"
      wget -O $yolov5_model_name http://video.cambricon.com/models/magicmind/v0.13.0/yolov5m_v0.13.0_4b_rgb_uint8.magicmind
    fi
    if [ ! -f $yolov5_label_name ]; then
      echo "download yolov5 label"
      wget -O $yolov5_label_name http://video.cambricon.com/models/labels/label_map_coco.txt
    fi
    feature_extract_model_name="feature_extract_v0.13.0_4b_rgb_uint8.magicmind"
    if [ ! -f $feature_extract_model_name ]; then
      echo "download feature extract model"
      wget -O $feature_extract_model_name http://video.cambricon.com/models/magicmind/v0.13.0/feature_extract_v0.13.0_4b_rgb_uint8.magicmind
    fi
  popd

  pushd $dst_dir
    if [ -d "models" ]; then
      rm -rf "models"
    fi
    ln -s $src_dir"/models" .
  popd
popd

start_webserver() {
pushd src
 gunicorn -c ../webserver_config.py wsgi:app
popd
}

start_webserver
