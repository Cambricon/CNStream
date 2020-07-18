#!/bin/bash
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CURRENT_DIR/../../lib
cd $CURRENT_DIR
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
cd -

