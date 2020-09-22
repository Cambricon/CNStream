#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/neuware/lib64/

apt-get update && apt-get upgrade -y && apt-get install -y --no-install-recommends \
  python3-dev \
  python3-pip && \
  apt-get clean && \
  pip3 install flask scikit-build opencv-python opencv-contrib-python-headless gunicorn gevent --no-cache-dir
