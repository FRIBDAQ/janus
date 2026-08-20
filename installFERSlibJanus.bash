#!/bin/bash

# Reset
Clear='\033[0m'       # Text Reset

# Regular Colors
Black='\033[0;30m'        # Black
Red='\033[0;31m'          # Red
Green='\033[0;32m'        # Green
Yellow='\033[0;33m'       # Yellow
Blue='\033[0;34m'         # Blue
Purple='\033[0;35m'       # Purple
Cyan='\033[0;36m'         # Cyan
White='\033[0;37m'        # White


echo -e "${Green}"
echo "####################################################"
echo "#     #####  #####  #####  #####  #   #  #         #"
echo "#     #      #      #   #  #      #      #         #"
echo "#     ####   ####   # # #  #####  #   #  # # #     #"
echo "#     #      #      #  #       #  #   #  #   #     #"
echo "#     #      #####  #   #  #####  ##  #  # # #     #"
echo "####################################################"
echo -e "${Clear}"

echo "Compiling CAEN FERSlib "

cd ferslib
# ---- Check for FERSlib local installation -----
#Flib="local/lib/libcaenferslib.so"
#Finclude="local/include"
#
#if [ -e $Flib ] && [ -e $Finclude/FERSlib.h ] && [ -e $Finclude/FERS_Registers_520X.h ] && [ -e $Finclude/FERS_Registers_5215.h ] && [ -e $Finclude/ExportTypes.h ]
#	echo -e "${Green}FERSlib already installed${Clear}"
#	echo "Do you want to overwrite the installation?"
#fi

myDir=$(pwd)
BAD_CHARS=$(echo "$myDir" | sed 's|[A-Za-z0-9._/@-]||g')

if [ -n "$BAD_CHARS" ]; then
	echo -e "${Red}Error: Unsafe character(s) detected in $myDir"
	echo -e "Unsafe characters: $BAD_CHARS${Clear}"
	echo
	exit 3
fi

libpath=$(pwd)/local
./configure --prefix=$libpath
res0=$?
make
res1=$?
make install
res2=$?

if [ $res0 -ne 0 ] || [ $res1 -ne 0 ] || [ $res2 -ne 0 ]; then
	echo -e "${Red}ERROR: cannot install FERSlib!!!"
	echo -e "Exiting ...${Clear}"
	exit -1
else
	echo -e "${Green}CAEN FERSlib installed${Clear}"
fi

cd ..

echo -e "${Green}" 
echo "#############################################"
echo "#       # #  #####  #   #  #   #  #####     #"
echo "#         #  #   #  ##  #  #   #  #         #"
echo "#         #  # # #  # # #  #   #  #####     #"
echo "#     #   #  #   #  #  ##  #   #      #     #"
echo "#     # # #  #   #  #   #  #####  #####     #"
echo "#############################################"
echo -e "${Clear}"

echo "Compiling JanusC ..."

# ---- Change path from relative to absolute -----
absPath="$(pwd)/ferslib/"
relPath="ferslib/"
sed "s|$relPath|$absPath|g" Makefile.tmp > Makefile

# ---- Compile JanusC -----
make all
res=$?
if [ $res -ne 0 ]; then
	echo -e "${Red}ERROR: Compilation failed"
	echo -e "Exiting ...${Clear}"
	exit -1
fi

echo -e "${Green}Compilation succeded."
echo -e "JanusC can be run from bin folder as ./JanusC${Clear}"

