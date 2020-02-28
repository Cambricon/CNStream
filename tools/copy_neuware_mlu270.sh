#!/bin/bash
#$1 -- neuware_home
#$2 -- dst_header_dir
#$3 -- dst_lib_dir 
CWD="$( cd "$( dirname "$0"  )" && pwd  )"


INCLUDE_DIR=$1/include
LIB_DIR=$1/lib64

cd $INCLUDE_DIR
for file in cn_*
do
    ln -sf $INCLUDE_DIR/$file  $2/$file
done
cd -

ln -sf $INCLUDE_DIR/cnrt.h $2/cnrt.h

cd $LIB_DIR
for file in libcnrt.*
do
    ln -sf $LIB_DIR/$file  $3/$file
done

for file in libcncodec.*
do
    ln -sf $LIB_DIR/$file  $3/$file
done

cd -
