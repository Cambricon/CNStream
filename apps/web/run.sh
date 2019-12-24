CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)

pushd $CURRENT_DIR

    if [ ! -d "$CURRENT_DIR/output" ]; then
     mkdir -p $CURRENT_DIR/output
    fi

    if [ ! -d "$CURRENT_DIR/model" ]; then
     mkdir -p $CURRENT_DIR/model
    fi

    if [ ! -f "$CURRENT_DIR/model/Dehaze.cambricon" ]; then
      wget -P $CURRENT_DIR/model https://github.com/Cambricon/models/raw/master/MLU100/Dehaze/Dehaze.cambricon
    fi

    if [ ! -f "$CURRENT_DIR/model/SuperResolution.cambricon" ]; then
      wget -P $CURRENT_DIR/model https://github.com/Cambricon/models/raw/master/MLU100/SuperResolution/SuperResolution.cambricon
    fi

    if [ ! -f "$CURRENT_DIR/model/japan_8mp.cambricon" ]; then
      wget -P $CURRENT_DIR/model https://github.com/Cambricon/models/raw/master/MLU100/style_transfer/japan_8mp.cambricon
    fi
    gunicorn -c web.conf object_detector_sever:app
popd
