#! /bin/bash

SphinxBuildDir="_build"
make clean
make latex
python parsejson.py 
cd $SphinxBuildDir/latex/
make all-pdf
