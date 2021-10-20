#!/bin/bash
CWD="$( cd "$( dirname "$0"  )" && pwd  )"
PACKAGE_NAME=live
LIVE555_INST_DIR=${CWD}/../3rdparty/live555
LIVE555_CONFIG=linux-with-shared-libraries
export CC=$1  # c compiler
export CXX=$2 # cxx compiler

if [ -f "$LIVE555_INST_DIR/include/liveMedia/liveMedia.hh" ];then
  if [ -f "$LIVE555_INST_DIR/lib/libliveMedia.so" ] || [ -f "$LIVE555_INST_DIR/lib/libliveMedia.a" ];then
    echo "live555 has been built and installed already"
    exit 0
  fi
fi

if [ -d ${LIVE555_INST_DIR} ]; then
  rm -rf "${LIVE555_INST_DIR}"
fi

SRC_TYPE=0
if [ -d ${CWD}/${PACKAGE_NAME} ]; then
  SRC_TYPE=1
elif [ -f ${CWD}/${PACKAGE_NAME}.tar.gz ]; then
  SRC_TYPE=2
else
  echo "<<<you need download latest live555 by download_live.sh>>>"
  exit 0
fi

if [ ${SRC_TYPE} = 2 ]; then
  tar xf "${CWD}/${PACKAGE_NAME}.tar.gz" -C ${CWD}
fi
cd ${CWD}/${PACKAGE_NAME}
sed -i '/COMPILE_OPTS =/ s/$/ -DRTP_PAYLOAD_MAX_SIZE=8192 -DALLOW_SERVER_PORT_REUSE=1 -DALLOW_RTSP_SERVER_PORT_REUSE=1 -DNO_OPENSSL=1/' ./config.${LIVE555_CONFIG}
sed -i 's/ -lssl -lcrypto//g' ./config.${LIVE555_CONFIG}
./genMakefiles ${LIVE555_CONFIG}

make
if [ $? = 0 ]; then
  echo "build ${PACKAGE_NAME} done"
  make install PREFIX=${LIVE555_INST_DIR}
  echo "install ${PACKAGE_NAME} done"
fi
make clean
cd ${CWD}
if [ ${SRC_TYPE} = 2 ]; then
  rm -rf ${CWD}/${PACKAGE_NAME}
fi








