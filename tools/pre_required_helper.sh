#!/bin/bash

#This script will compile and install the dependencies build with support for cnstream ubuntu or centos.
CWD="$( cd "$( dirname "$0"  )" && pwd  )"
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

download() {
  echo ""
  echo "download $1"
  echo "======================="

  if [ -f "$CWD/$1.done" ]; then
    echo "$1 already exist. Remove $CWD/$1.done lockfile to re-download it."
    return 1
  fi

  return 0
}

download_done () {
  touch "$CWD/$1.done"
}

installAptLibs() {
  ${SUDO_CMD} apt-get update
  ${SUDO_CMD} apt-get -y --force-yes install libopencv-dev\
    libgflags-dev\
    libgoogle-glog-dev\
    libfreetype6\
    ttf-wqy-zenhei\
    cmake\
    libsdl2-dev\
    curl libcurl4-openssl-dev\
    librdkafka-dev\
    lcov
}

installYumLibs() {
  ${SUDO_CMD} yum -y install opencv-devel.x86_64\
    gflags-devel\
    glog-devel\
    cmake3.x86_64\
    freetype-devel\
    SDL2_gfx-devel.x86_64\
    wqy-zenhei-fonts\
    curl libcurl-devel \
    librdkafka-devel \
    lcov
  ${SUDO_CMD} yum install -y epel-release rpm
  #remove below line for long time to update
  #${SUDO_CMD} yum update -y 
  ${SUDO_CMD} rpm --import http://li.nux.ro/download/nux/RPM-GPG-KEY-nux.ro
  ${SUDO_CMD} rpm -Uvh http://li.nux.ro/download/nux/dextop/el7/x86_64/nux-dextop-release-0-5.el7.nux.noarch.rpm 
  ${SUDO_CMD} yum install ffmpeg ffmpeg-devel -y     
  echo "Should find ffmpeg libraries in /usr/lib64 and header files in /usr/include/ffmepg"
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


font_path=`fc-list :lang=zh | grep "wqy-zenhei.ttc"`
${SUDO_CMD} ln -sf ${font_path%%:*} /usr/include/wqy-zenhei.ttc

if download "live555"; then
  ./download_live.sh  
  download_done "live555";
fi

echo "Complete!"

## END ##
