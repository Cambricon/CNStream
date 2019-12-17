#!/bin/bash

SUDO_CMD=""

command_exists() {
    if ! [[ -x $(command -v "$1") ]]; then
        return 1
    fi

    return 0
}


check_sudo(){
    if command_exists "sudo"; then
        SUDO_CMD="sudo"	
    fi
}

check_sudo

if ! [[ ${FACESWAP_ENV} == "ON" ]]; then
    ${SUDO_CMD} pip3 install -r requirements.txt -i https://pypi.doubanio.com/simple
    export FACESWAP_ENV=ON
fi

CURRENT_DIR=$(dirname ${BASH_SOURCE[0]})

if [ ! -f "$CURRENT_DIR/faceswap.cambricon" ]; then
    wget -P $CURRENT_DIR  https://github.com/Cambricon/models/raw/master/MLU100/faceswap/faceswap.cambricon
fi
