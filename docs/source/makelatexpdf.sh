#! /bin/bash

make clean
make latex
#SphinxBuildDir=`python parsejson.py` #该语句不再使用
## echo "reulst is $SphinxBuildDir"

## read -p "enter Yes or No: " Arg
## case "$Arg" in 
##    Y|y|Yes|yes)
##      break;;
##    N|n|No|no)
##      exit;
## esac
python parsejson.py

#read BUILD dir for Makefile
#SphinxBuildDi=""
while read line
  do
    #echo $line
    k=${line%=*}
    v=${line#*=}
    #remove space in head and tail
    kv=$(echo $k | awk 'gsub(/^ *| *$/,"")')
    vv=$(echo $v | awk 'gsub(/^ *| *$/,"")')
   
    #echo "kv is $kv"
    #echo "vv is $vv"
    if [ "$kv" = "BUILDDIR" ];
       then
       #echo "the dir is $vv"; 
       SphinxBuildDir="$vv";
       #echo "sphinxbuilddir is $SphinxBuildDir";
       break
    fi
   
 done < ./Makefile

cd $SphinxBuildDir/latex/
make all-pdf
