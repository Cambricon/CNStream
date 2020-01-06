#!/bin/bash
CWD="$( cd "$( dirname "$0"  )" && pwd  )"

wget_dnld () {

  DOWNLOAD_PATH=$CWD;

  if [ ! -z "$3" ]; then
    mkdir -p $CWD/$3
    DOWNLOAD_PATH=$CWD/$3
  fi;

  if [ ! -f "$DOWNLOAD_PATH/$2" ]; then
    echo "Downloading $1"
    #curl -L --silent -o "$DOWNLOAD_PATH/$2" "$1"
    wget -O "$DOWNLOAD_PATH/$2" "$1"
    EXITCODE=$?

    if [ $EXITCODE -ne 0 ]; then
      echo ""
      echo "Failed to download $1. Exitcode $EXITCODE. Retrying in 10 seconds";
      sleep 10
      #curl -L --silent -o "$DOWNLOAD_PATH/$2" "$1"
      wget -O "$DOWNLOAD_PATH/$2" "$1"
    fi

    EXITCODE=$?

    if [ $EXITCODE -ne 0 ]; then
      echo ""
      echo "Failed to download $1. Exitcode $EXITCODE";
      exit 1
    fi

    echo "... Done"

    if ! tar -xvf "$DOWNLOAD_PATH/$2" -C "$DOWNLOAD_PATH" 2>/dev/null >/dev/null; then
      echo "Failed to extract $2";
      exit 1
    fi

  else
    echo "\"$DOWNLOAD_PATH/$2\" exists, if want to download the latest,please remove \"$2\" firstly!"
    exit 0
  fi
}

wget_dnld "http://www.live555.com/liveMedia/public/live555-latest.tar.gz" "live.tar.gz"
