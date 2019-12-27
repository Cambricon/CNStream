CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
if [ ! -d "$CURRENT_DIR/output" ]; then
 mkdir -p $CURRENT_DIR/output
fi
gunicorn -c web.conf object_detector_sever:app
