#!/bin/bash

source env.sh

python3 convert.py \
    --input ./video/data_dst.mp4 \
    --output ./tmp/result.avi \
    --type video
