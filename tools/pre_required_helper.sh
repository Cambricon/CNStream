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



installAptLibs() {
    ${SUDO_CMD} apt-get update
    ${SUDO_CMD} apt-get -y --force-yes install libopencv-dev\
		libgflags-dev\
		libgoogle-glog-dev\
		cmake
}

installYumLibs() {
    ${SUDO_CMD} yum -y install opencv-devel.i686\
		gflags.x86_64\
		glog.x86_64\
		cmake3.x86_64
}	

installLibs() {
    echo "Installing prerequisites"
    . /etc/os-release
    case "$ID" in
        ubuntu | linuxmint ) installAptLibs ;;
        * )                  installYumLibs ;;
    esac
}


check_sudo
installLibs

./build_ffmpeg.sh --build

echo "Complete!"

## END ##
