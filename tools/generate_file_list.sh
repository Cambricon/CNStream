#!/bin/sh
# USAGE: $0 path-to-files

if [ -f files.list ]; then
  rm files.list
fi

curr_dir=$(pwd)

cd $1
new_dir=$(pwd)

for file in *
do
  echo $new_dir/${file} >> $curr_dir/files.list
done
