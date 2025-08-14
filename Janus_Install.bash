#!/bin/bash

Clear='\033[0m'
Yellow='\033[0;33m'       # Yellow
Red='\033[0;31m'

# Define the help function
function show_help() {
    echo "Please, install Janus manually (short step-by-step guide)":
    echo ""
	echo "Install the following packages before compiling Janus:"
	echo " - libusb-1.0"
	echo " - pkg-config"
	echo " - gnuplot with wxt terminal support"
	echo " - python3"
	echo " - python3 tkinter"
	echo " - python3 pillow"
	echo " - python3 pillow tkinter"
	echo 
	echo " *NB: If your gnuplot is not supporting wxt terminal, install it from source:"
	echo "   wget https://sourceforge.net/projects/gnuplot/files/latest/download -O gnuplot.tar.gz"
	echo "   tar -xvf gnuplot .tar.gz"
	echo "   ./configure --with-wx"
	echo "   make"
	echo "   sudo make install"
	echo
	echo " Compile FERSlib:"
	echo "   cd ferslib"
	echo "   run configure with --prefix set as ferslib/local:"
	echo '   ./configure --prefix=$(pwd)/local'
	echo "	 sudo make"
	echo "	 sudo make install"
	echo
	echo " Set absolute path in Makefile, by copying and changing Makefile.tmp:"
	echo '   absPath="$(pwd)/ferslib/"'
	echo '   relPath="ferslib/"'
	echo '   sed "s|$relPath|$absPath|g" Makefile.tmp > Makefile'
	echo
	echo " Compile Janus:"
	echo '   make all'
    echo ""
}

if [ -n "$BASH" ]; then
	echo "###############################################"
else
	echo -e "${Red} Please, run the installer with bash:"
	echo -e " - bash Janus_Installer.bash${Clear}"
	echo "Exiting ..."
	exit 1
fi
	
echo "###############################################"
echo "###                                         ###"
echo "###       WELCOME TO JANUS INSTALLER        ###"
echo "###                                         ###"
echo "### Janus Installer supports the following  ###"
echo "### linux distribution:                     ###"
echo "###  - debian-like                          ###"
echo "###  - fedora/redhat                        ###"
echo "###                                         ###"
echo "###############################################"
echo "###############################################"
echo

gccV=`g++ -dumpversion`
res=$?
if [ $res -ne 0 ]; then
	echo -e "${Red}ERROR: g++ is missing"
	echo "Please, install g++ to proceed with Janus installer"
	echo -e "Exiting ...${Clear}"
	exit 1
fi

#from [https://unix.stackexchange.com/questions/46081/identifying-the-system-package-manager]
declare -A osInfo;
osInfo[/etc/redhat-release]=fedoraRedhat   #yum
osInfo[/etc/arch-release]=arch	#pacman
osInfo[/etc/gentoo-release]=gentoo	#emerge
osInfo[/etc/SuSE-release]=SuSE		#zypp
osInfo[/etc/SUSE-brand]=SuSE		#zypp
osInfo[/etc/debian_version]=debian	#apt-get
osInfo[/etc/alpine-release]=apline	#apk
vers=""
for f in ${!osInfo[@]}
do
    if [[ -f $f ]];then
		vers=${osInfo[$f]}
        # echo Package manager: ${osInfo[$f]}
    fi
done


if [ $gccV -gt 13 ]; then
	echo -e "${Yellow}You are using g++ V${gccV}"
	echo "For any problems while running JanusC, please consider to compile Janus and FERSlib with a g++ version <= 13"
	echo "by changing in Makefile.tmp"
	echo "CXX = g++ ---> CXX = g++-VV"
	if [ $vers == debian ] || [ $vers == fedoraRedhat ]; then
		echo "and in installer_${vers}.bash"
		echo "./configure --prefix=$libpath CXX=g++-$gxxV ---> ./configure --prefix=$libpath CXX=g++-VV"
	fi
	echo -e "where VV is the g++ compiler version in use${Clear}"  
	#echo "by running this installer with the arg: 'bash Janus_Install CXX=g++-VV', where VV is the g++ version you select"
	#echo "CXX = g++ ---> CXX = g++-11"
fi

if [ -z "$vers" ]; then
	vers="in-use"
fi

if [ $vers != debian ] && [ $vers != fedoraRedhat ]; then
	echo -e "${Yellow}Linux distribution ${vers} is not supported by Janus Installer"
	echo
	echo
	show_help
	# echo "Please install the following packages before compiling Janus:"
	# echo " - libusb-1.0"
	# echo " - pkg-config"
	# echo " - gnuplot"
	# echo " - python3"
	# echo " - python3 tkinter"
	# echo " - python3 pillow"
	# echo " - python3 pillow tkinter"
	# echo 
	# echo "Compile Janus by running in shell the command:  make all"
	echo 
	echo -e "${Clear}"
	chmod +x setUSBrules.bash
	./setUSBrules.bash
else
	# Set installer name
	installer=installer_${vers}.bash
	chmod +x $installer
	./$installer
fi

#installer.bash
#./installer.bash
