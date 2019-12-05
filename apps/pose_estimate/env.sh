#!/bin/bash
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)

pushd $CURRENT_DIR
  if [ ! -f $CURRENT_DIR/files.list_video ]; then
    echo "generate files.list_video in $CURRENT_DIR"
    for ((i = 0; i < 1; ++i))
    do
      echo "$CURRENT_DIR/test_data/single people(traffic police).mp4" >> files.list_video
    done
  fi
  if [ $? -ne 0 ]; then
      exit 1
  fi

popd
