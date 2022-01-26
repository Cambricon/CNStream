#!/bin/bash
CWD="$( cd "$( dirname "$0"  )" && pwd  )"
PACKAGE_NAME=libyuv
LIBYUV_INST_DIR=${CWD}/../3rdparty/libyuv
CMAKE_C_COMPILER=$1
CMAKE_CXX_COMPILER=$2

if [ -f "$LIBYUV_INST_DIR/lib/libyuv.a" ] && [ -f "$LIBYUV_INST_DIR/include/libyuv.h" ];then
  echo "libyuv has been built and installed already"
  exit 0
fi

if [ -d ${LIBYUV_INST_DIR} ]; then
  rm -rf "${LIBYUV_INST_DIR}"
fi

SRC_TYPE=0
if [ -d ${CWD}/${PACKAGE_NAME} ]; then
  SRC_TYPE=1
elif [ -f ${CWD}/${PACKAGE_NAME}.tar.gz ]; then
  SRC_TYPE=2
else
  echo "<<<you need download libyuv first>>>"
  exit 0
fi

if [ ${SRC_TYPE} = 2 ]; then
  tar xf "${CWD}/${PACKAGE_NAME}.tar.gz" -C ${CWD}
fi

cd ${CWD}/${PACKAGE_NAME}

if [ ${CMAKE_C_COMPILER} ] && [ ${CMAKE_CXX_COMPILER} ]; then
  make -f linux.mk CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS="-O2 -fomit-frame-pointer -fPIC -I./include"  CXXFLAGS="-O2 -fomit-frame-pointer -fPIC -I./include"
else
  make -f linux.mk CFLAGS="-O2 -fomit-frame-pointer -fPIC -I./include"  CXXFLAGS="-O2 -fomit-frame-pointer -fPIC -I./include"
fi

if [ $? = 0 ]; then
  echo "build ${PACKAGE_NAME} done"
  mkdir -p $LIBYUV_INST_DIR
  mkdir -p $LIBYUV_INST_DIR/lib
  mkdir -p $LIBYUV_INST_DIR/include
  cp -rf include/*  $LIBYUV_INST_DIR/include
  cp libyuv.a $LIBYUV_INST_DIR/lib
  echo "install ${PACKAGE_NAME} done"
fi

make -f linux.mk clean
cd ${CWD}
if [ ${SRC_TYPE} = 2 ]; then
  rm -rf ${CWD}/${PACKAGE_NAME}
fi








