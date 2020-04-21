#! /bin/bash

SphinxBuildDir="_build"
make clean
make latex
python parsejson.py $SphinxBuildDir
cd $SphinxBuildDir/latex/
make all-pdf
