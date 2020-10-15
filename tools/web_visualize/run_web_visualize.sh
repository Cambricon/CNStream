#!/bin/bash
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/neuware/lib64/

src_dir="$CURRENT_DIR/../../data"
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

  resnet50_model_path="models/MLU270/Classification/resnet50"
  resnet50_model_name="resnet50_offline_v1.3.0.cambricon"
  resnet50_label_name="synset_words.txt"
  mkdir -p $src_dir/$resnet50_model_path
  pushd $src_dir/$resnet50_model_path
    if [ ! -f $resnet50_model_name ]; then
      echo "download renset50"
      wget -O $resnet50_model_name  http://video.cambricon.com/$resnet50_model_path/$resnet50_model_name
    fi
    if [ ! -f $resnet50_label_name ]; then
      echo "download renset50 label"
      wget -O $resnet50_label_name http://video.cambricon.com/$resnet50_model_path/$resnet50_label_name
    fi
  popd
  ssd_model_path="models/MLU270/Primary_Detector/ssd"
  ssd_model_name="resnet34_ssd.cambricon"
  ssd_label_name="label_voc.txt"
  echo $src_dir/$ssd_model_path
  pushd $src_dir/$ssd_model_path
    if [ ! -f $ssd_model_name ]; then
      echo "download ssd"
      wget -O $ssd_model_name  http://video.cambricon.com/$ssd_model_path/$ssd_model_name
    fi
    if [ ! -f $ssd_label_name ]; then
      echo "download ssd label"
      wget -O $ssd_label_name http://video.cambricon.com/$ssd_model_path/$ssd_label_name
    fi
  popd
  yolov3_model_path="models/MLU270/yolov3"
  yolov3_model_name="yolov3_offline_u4_v1.3.0.cambricon"
  yolov3_label_name="label_map_coco.txt"
  mkdir -p $src_dir/$yolov3_model_path
  pushd $src_dir/$yolov3_model_path
    if [ ! -f $yolov3_model_name ]; then
      echo "download yolov3"
      wget -O $yolov3_model_name  http://video.cambricon.com/$yolov3_model_path/$yolov3_model_name
    fi
    if [ ! -f $yolov3_label_name ]; then
      echo "download yolov3 label"
      wget -O $yolov3_label_name http://video.cambricon.com/$yolov3_model_path/$yolov3_label_name
    fi
  popd
  feature_extract_model_path="models/MLU270/feature_extract"
  feature_extract_model_name="feature_extract_v1.3.0.cambricon"
  mkdir -p $src_dir/$feature_extract_model_path
  pushd $src_dir/$feature_extract_model_path
    if [ ! -f $feature_extract_model_name ]; then
      echo "download feature extract model"
      wget -O $feature_extract_model_name  http://video.cambricon.com//$feature_extract_model_path/$feature_extract_model_name
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
