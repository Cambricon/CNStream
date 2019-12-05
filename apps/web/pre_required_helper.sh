#!/bin/bash

#This script will compile and install the dependencies build with support for cnstream ubuntu or centos.
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



installLibs() {
    echo "Installing prerequisites"
    ${SUDO_CMD} pip3 install -i  https://pypi.doubanio.com/simple opencv-python\
                Pillow\
		flask\
                greenlet\
		gevent\
                gunicorn==19.9.0
    ${SUDO_CMD} apt-get update
    ${SUDO_CMD} apt-get -y install nginx
}

check_sudo
installLibs

echo "Complete!"

## END ##
