#!/bin/bash

# This installer must be run when the FERSlib is installed
# in /usr/local/lib direcotry, as outlined in the FERSlib README.
# If you wish to run the demo inside a Janus installation, please
# use the installer_demo_locallib.sh script instead. 

cp Makefile_share.tmp Makefile -f

make clean

make
