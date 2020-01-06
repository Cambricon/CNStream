#! /bin/bash
CURRENT_DIR=$(dirname $(readlink -f $0) )

model_file=""
track_model_path=""
platform=$(echo $1 | tr [a-z] [A-Z])

if [[ $platform == "MLU100" ]]; then
  model_file="../data/models/MLU100/resnet34_ssd.cambricon"
  track_model_path="../data/models/MLU100/feature_extractor.cambricon"
  label_path="../data/models/MLU100/label_voc.txt"
  data_path="../data/videos/cars.mp4"
elif [[ $platform == "MLU270" ]]; then
  model_file="../data/models/MLU270/resnet34_ssd.cambricon"
  track_model_path="cpu"
  data_path="../data/videos/1080P.h264"
  label_path="../data/models/MLU270/label_voc.txt"
else
  echo "Please add MLU100 or MLU270 parameter"
  exit 0
fi

pushd $CURRENT_DIR
./bin/stream-app  \
     --model_path $model_file \
     --track_model_path $track_model_path \
     --label_path $label_path \
     --data_path $data_path \
     --show=false \
     --wait_time 0
popd
