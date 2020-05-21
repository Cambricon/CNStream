#!/bin/bash
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
arch=`uname -m`
card=$(cat /proc/driver/cambricon/mlus/*/information | grep -Eiow 'mlu-100|mlu-270|mlu-220 M2|mlu-220 EVB' | uniq -c | grep -Eiow 'mlu-270|mlu-100|mlu-220 M2|mlu-220 EVB')
if [ "${card}" == "mlu-270" ]; then
  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CURRENT_DIR/../../mlu/MLU270/libs/$arch:$CURRENT_DIR/../../lib
elif [ "${card}" == "mlu-220 M2" -o "${card}" == "mlu-220 EVB" ]; then
  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CURRENT_DIR/../../mlu/MLU270/libs/$arch:$CURRENT_DIR/../../lib
else
  echo "driver is not installed!!!"
  exit 1
fi

pushd $CURRENT_DIR
  if [ ! -f $CURRENT_DIR/files.list_video ]; then
    echo "generate files.list_video in $CURRENT_DIR"
    for ((i = 0; i < 2; ++i))
    do
      echo "$CURRENT_DIR/../../data/videos/cars.mp4" >> files.list_video
    done
  fi
  if [ ! -f $CURRENT_DIR/files.list_image ]; then
    echo "generate files.list_image in $CURRENT_DIR"
    echo "$CURRENT_DIR/../../data/images/%d.jpg" >> files.list_image
  fi
  if [ $? -ne 0 ]; then
      exit 1
  fi

popd
