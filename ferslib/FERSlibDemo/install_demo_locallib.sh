#!/bin/bash

# This installer is automatically executed by Janus Installer script.
# It is meant to be run when the FERSlib .so object is created in the
# local folder _Path_To_FERSLIB_/local/lib

cp _TMP_Makefile Makefile

# Get LIBPATH IN MAKEFILE
soPath=$(cat Makefile | grep -m1 INCLUDE)
libsoPath=$(echo $soPath | awk -F " " '{print $NF}')

# Get New LIBPATH in ferslib/local/lib
lPath=$(pwd)
localPath="-Wl,-rpath,${lPath%/*}/local/lib"

echo $libsoPath $localPath

sed -i "s|$libsoPath|$localPath|g" Makefile

make clean

make
