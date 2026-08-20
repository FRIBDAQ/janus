#!/bin/bash

# This installer is automatically executed by Janus Installer script.
# It is meant to be run when the FERSlib .so object is created in the
# local folder _Path_To_FERSLIB_/local/lib

absPath="$(pwd)/../../"
relPath="ferslib/"
sed "s|$relPath|$absPath|g" Makefile_local.temp > Makefile 

make clean

make
