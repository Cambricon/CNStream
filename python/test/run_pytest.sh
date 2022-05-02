# !/bin/bash
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
pushd $CURRENT_DIR
pytest
if [ $? -ne 0 ]; then
  echo "[CNStream] Run pytest failed!!!"
  exit 1
fi
popd

