#!/bin/bash
#$1 -- neuware_home
#$2 -- dst_header_dir
#$3 -- dst_lib_dir 
CWD="$( cd "$( dirname "$0"  )" && pwd  )"

cp -p $1/include/cnrt.h $2
cp -p $1/include/cncodec.h $2
cp -p $1/lib64/libcnrt.* $3
cp -p $1/lib64/libcncodec.* $3
