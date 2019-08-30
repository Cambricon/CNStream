#!/bin/bash
if [ -f /etc/os-release ]; then
  os_ret=`cat /etc/os-release | grep "ID=ubuntu"`
  if [ ${os_ret}" " != " " ]; then
    # ubuntu
    OS_VERSION="ubuntu16.04"
  else
    os_ret=`cat /etc/os-release | grep "ID=debian"`
    if [ ${os_ret}" " != " " ]; then
      # debian
      OS_VERSION="debian9.5"
    else
      os_ret=`cat /etc/os-release | grep "ID=\"centos\""`
      if [ ${os_ret}" " != " " ]; then
        # centos
        OS_VERSION="centos7.2"
      else
        # unknown
        echo "unsupport os version"
        # exit 1
      fi
    fi
  fi
fi

# echo "OS version is $OS_VERSION"
CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)

pushd $CURRENT_DIR
  if [ ! -f $CURRENT_DIR/files.list_video ]; then
    echo "generate files.list_video in $CURRENT_DIR"
    for ((i = 0; i < 2; ++i))
    do
      echo "$CURRENT_DIR/../data/videos/cars.mp4" >> files.list_video
    done
  fi
  if [ ! -f $CURRENT_DIR/files.list_image ]; then
    echo "generate files.list_image in $CURRENT_DIR"
    echo "$CURRENT_DIR/../data/images/%d.jpg" >> files.list_image
  fi
  if [ $? -ne 0 ]; then
      exit 1
  fi

popd
