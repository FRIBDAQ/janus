#!/bin/bash
#
# If when download the unix packages you get the error 
# 'Could not get lock /varlib/dpkg/lock_*****', follow
# this guide https://itsfoss.com/could-not-get-lock-error/ 

# If a 32 bits version is needed: 
# install 'sudo apt-get install g++-multilib'	(g++ 32bits)
# install 'sudo apt-get install libudev-dev:i386 libusb-1.0-0-dev:i386'  (32bits usb compatible)
# add write the W32FLAG = -m32 on Makefile

# Check if the script is run as root
isroot=`id -u`

# Reset
NoColor='\033[0m'       # Text Reset

# Regular Colors
Black='\033[0;30m'        # Black
Red='\033[0;31m'          # Red
Green='\033[0;32m'        # Green
Yellow='\033[0;33m'       # Yellow
Blue='\033[0;34m'         # Blue
Purple='\033[0;35m'       # Purple
Cyan='\033[0;36m'         # Cyan
White='\033[0;37m'        # White

# Searching for library needed to compile JanusC
echo "Searching for libusb.h ..."
usblib="/usr/include/libusb-1.0/libusb.h"
res=$?
if [ ! -e $usblib ] && [ $isroot -ne 0 ]; then
        echo "ERROR: libusb.h is missing"
        echo "Please, run the Installer as root or install libusb.h with 'sudo apt-get install libusb-1.0-0-dev'"
        exit 1
elif [ ! -e $usblib ] && [ $isroot -eq 0 ]; then
	echo "Installing libusb.h ..."
	sudo apt-get install libusb-1.0-0-dev
	sudo updatedb && locate libusb.h
	res=$?
	if [ $res -eq 0 ]; then
		echo "libusb.h installed!!"
	fi
else
	echo "libusb.h found!!"
fi

echo "*************************************************"
echo "*************************************************"
echo "*************************************************"
echo "Compiling JanusC ..."

#Compile JanusC
make all
res=$?
if [ $res -ne 0 ]; then
	echo "ERROR: Compilation failed"
	echo "Exiting ..."
	exit 1
fi

echo "Compilation succeded."
echo "JanusC can be run from ./bin/JanusC"
echo "*************************************************"
echo "*************************************************"
echo "*************************************************"
#if [ ! -f bin/BinToCsv ]; then
#	echo "Compiling macro macros/BintoCsv.cpp ..."
#	g++ -o ./bin/BinToCsv macros/BinToCsv.cpp
#	res=$?
#	if [ $res -ne 0 ]; then
#       	echo "ERROR: Compilation failed"
#        	echo "Exiting ..."
#        	exit 1
#	fi
#echo "Compilation succeded!!"
#echo "*************************************************"
#echo "*************************************************"
#echo "*************************************************"
#fi
#Searching for additional packages 'gnuplot' for JanusC and 'python3-tk' for the GUI
#Without them, JanusC or the GUI crash 
echo "Searching for gnuplot ..."
gnp="/usr/bin/gnuplot"
if [ ! -e $gnp ] && [ $isroot -ne 0 ]; then
	echo "Please, install gnuplot with 'sudo apt-get install gnuplot'"
elif [ ! -e $gnp ] && [ $isroot -eq 0 ]; then
	sudo apt-get install gnuplot
else
	echo "gnuplot found!!!"
fi

echo
echo "PLEASE, check if gnuplot support terminal 'wxt' "
echo "by typing on gnuplot shell 'set terminal' command"
echo
echo "*************************************************"
echo "*************************************************"
echo "*************************************************"

if [ $isroot -ne 0 ]; then
	echo "Please, check if python3-tk is installed to make the GUI run, otherwise run 'sudo apt-get install python3-tk'"
	echo "or run this installer as root"	
elif [ $isroot -eq 0 ]; then
	sudo apt-get install python3-tk
fi

# launcher="bin/launch_JanusC.sh"
# if [ ! -e $launcher ]; then
# 	touch $launcher
# 	echo "#!/bin/bash" > $launcher
# 	echo >> $launcher
# 	echo "if [ ! -e JanusC ]; then" >> $launcher
# 	echo "    echo \"Please, compile Janus using make in the main folder\"" >> $launcher
# 	echo "    return -1" >> $launcher
# 	echo "fi" >> $launcher
# 	echo "./JanusC" >> $launcher
# 	echo "stty sane" >> $launcher
# fi
# chmod +x $launcher

echo "*************************************************"
echo "*************************************************"
echo "*************************************************"
# Create rule for USB privilege - the installer must be run as root
if [ $isroot -ne 0 ]; then     # from https://www.xmodulo.com/change-usb-device-permission-linux.html
	echo "Please, be aware that, as user, you might not have the permission to connect"
	echo "with FERS modules via USB. In case of USB connection issues, please re-run"
	echo "this installer as root with USB plugged in to create the permission rule for connecting via USB."
elif [ $isroot -eq 0 ]; then
	# Check idVendor and idProduct of WinUSB. It should be always the same
	usblist=`lsusb | grep 'Microchip Technology'`
	if [ "$usblist" != "" ]; then # USB connected
		bus=`echo $usblist | awk -F " " '{print $2}'`
		device=`echo $usblist | awk -F " " '{print $4}'`
		VV=`lsusb -v -s $bus:$device | grep idVendor`
		PP=`lsusb -v -s $bus:$device | grep idProduct`
		mVendor=`echo $VV | awk -F " " '{print $2}' | awk -F "x" '{print $2}'`
		mProduct=`echo $PP | awk -F " " '{print $2}' | awk -F "x" '{print $2}'`
	else # USB not connected
		mVendor=04d8
		mProduct=0053
		echo "${Yellow}Warning: the idVendor and idProduct are set as default value. Please check these values"
		echo "${Yellow}by plugging in the FERS via USB and typing on shell 'lsusb -v'${NoColor}"
		echo
		echo
	fi

	FILERULE="/etc/udev/rules.d/50-myusb.rules"
	USBRULE='SUBSYSTEMS=="usb",ATTRS{idVendor}=="'$mVendor'",ATTRS{idProduct}=="'$mProduct'",GROUP="users",MODE="0666"'
	# USBRULE='SUBSYSTEMS=="usb",ATTRS{idVendor}=="04d8",ATTRS{idProduct}=="0053",GROUP="users",MODE="0666"'
	echo "Creating the permission rule file " $FILERULE " to let JanusC connect via USB without the need of being root ..."
	if [ ! -e $FILERULE ]; then
		echo "Creating the Rule File ... "
		sudo touch $FILERULE
	fi
	grep -Fxq $USBRULE $FILERULE
	res=$? 
	if [ $res -ne 0 ]; then
		echo $USBRULE >> $FILERULE
		echo "DONE"
	else
		echo "The permission rule is already present on this PC."
	fi
	
	echo "If the USB connection doesn't work, try to unplug and plug back the usb connector,"
	echo "reboot and execute 'sudo udevadm control --reload' and 'sudo udevadm trigger'"
fi

echo
echo "*************************************************"
echo "Installation completed"
#echo
#echo "N.B.: if you use JanusC from shell please launch it"
#echo "with the launcher script ./launch_JanusC.sh"
echo "*************************************************"


