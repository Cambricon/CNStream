# !/bin/bash
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
pushd $CURRENT_DIR
pytest
popd

