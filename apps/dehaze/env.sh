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
arch=`uname -m`
card=$(cat /proc/driver/cambricon/mlus/*/information | grep -Eiow 'mlu-100|mlu-270')
if [ ${card} == "mlu-100" ]; then
  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CURRENT_DIR/../../mlu/MLU100/libs/$arch:$CURRENT_DIR/../../lib
  echo $LD_LIBRARY_PATH
elif [ ${card} == "mlu-270" ]; then
  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CURRENT_DIR/../../mlu/MLU270/libs/$arch:$CURRENT_DIR/../../lib
else
  echo "driver is not installed!!!"
  exit 1
fi

pushd $CURRENT_DIR
  if [ ! -f $CURRENT_DIR/files.list_video ]; then
    echo "generate files.list_video in $CURRENT_DIR"
    for ((i = 0; i < 2; ++i))
    do
      echo "$CURRENT_DIR/../../data/videos/cars.mp4" >> files.list_video
    done
  fi
  if [ ! -f $CURRENT_DIR/files.list_image ]; then
    echo "generate files.list_image in $CURRENT_DIR"
    echo "$CURRENT_DIR/test_data/0.jpg" >> files.list_image
  fi
  if [ $? -ne 0 ]; then
      exit 1
  fi

popd
