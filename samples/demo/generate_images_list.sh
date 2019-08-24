#!/bin/bash
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)

if [ -f $CURRENT_DIR/images.list ]; then
  exit 0
else
  echo "generate images list"
  for file in $CURRENT_DIR/../data/images/*
  do
    echo ${file} >> $CURRENT_DIR/images.list
  done
fi
