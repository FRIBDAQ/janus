#!/bin/bash

Clear='\033[0m'
Yellow='\033[0;33m'       # Yellow
Red='\033[0;31m'
Green='\033[0;32m'        # Green

# Define the help function
function show_help() {
    echo "Please, install Janus manually (short step-by-step guide)":
    echo ""
	echo "Install the following packages before compiling Janus:"
	echo " - g++"
	echo " - make"
	echo " - libusb-1.0"
	echo " - pkg-config"
	echo " - gnuplot with wxt terminal support"
	echo " - python3"
	echo " - python3 tkinter"
	echo " - python3 pillow"
	echo " - python3 pillow tkinter"
	echo 
	echo " *NB: If your gnuplot does not support the wxt terminal, install it from source."
	echo " First install the wxgtk3-dev library (suggested), then:"
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
	echo " In main Janus folder, set absolute path in Makefile, by copying and changing Makefile.tmp:"
	echo '   cd .. (from "ferslib" folder)'
	echo '   absPath="$(pwd)/ferslib/"'
	echo '   relPath="ferslib/"'
	echo '   sed "s|$relPath|$absPath|g" Makefile.tmp > Makefile'
	echo
	echo " Compile Janus:"
	echo '   make all'
    echo ""
}

if [ -n "$BASH" ]; then
	echo -e "${Green}###############################################${Clear}"
else
	echo -e "${Red} Please, run the installer with bash:"
	echo -e " - bash Janus_Installer.bash${Clear}"
	echo "Exiting ..."
	exit 1
fi
	

function CheckRequirements() {
	dist=$1
	chmod +x check_requirements.bash
	./check_requirements.bash $dist
	ret=$?

	if [ $ret -eq 3 ]; then
		echo -e "${Yellow}Gnuplot in Janus may not work properly without wxt terminal installed${Clear}"
	elif [ $ret -ne 0 ];  then
		echo -e "${Red}Required package missing. Cannot procede with Janus installation${Clear}"
		echo -e "Exiting ..."
		return 10
	fi
}

function Installation {
	chmod +x installFERSlibJanus.bash
	./installFERSlibJanus.bash
	res=$?
	return $res
}

function CfgUSB {
	chmod +x setUSBrules.bash
	./setUSBrules.bash
	res=$?
	return $res
}

#################
# --- MAIN ---- #
#################	
	
echo -e -n "${Green}"
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
echo -e "${Clear}"


if [ -e /etc/os-release ]; then
	. /etc/os-release
	name="$ID"
	dist="$ID_LIKE"
	if [ -z $dist ]; then
		dist=$name
	fi
fi

## Fallback for older linux distribution

## Normalize name
case "$name" in
    almalinux|AlmaLinux) name="AlmaLinux" ;;
    centos|CentOS) name="CentOS" ;;
    rockylinux|RockyLinux|rocky) name="Rocky Linux" ;;
    rhel|RedHat*|redhat) name="RedHat" ;;
    fedora|Fedora) name="Fedora" ;;
    ubuntu|Ubuntu) name="Ubuntu" ;;
    debian|Debian) name="Debian" ;;
    opensuse*|suse|SUSE|SLES) name="openSUSE" ;;
    alpine|Alpine) name="Alpine" ;;
    arch|Arch) name="Arch Linux" ;;
    manjaro|Manjaro) name="Manjaro" ;;
    void|Void) name="Void Linux" ;;
    *) name="Other ($name)" ;;
esac


#########################################################################
# Check Ubuntu distribution supported and achitecture
arch=$(arch)

# Accept only 64 bit architectures
case "$arch" in
    x86_64|amd64|aarch64|arm64)
        echo "${name} ${version} (${arch})"
        ;;
    *)
        echo -e "${Red}Unsupported architecture: ${arch}"
        echo -e "(Only 64-bit systems are supported) ${Clear}"
        exit 1
        ;;
esac


iCheck=0
iInstall=0
iSetRule=0

while true; do
	echo ""
	echo "Select the action to perform:"
	echo "0. Full installation (suggested for Ubuntu and Fedora distribution or when the requirements are satisfied"
	echo "Or select one of the following installation step:"
	echo "1. Check installation requirements"
	echo "2. Install FERSlib and Janus"
	echo "3. Configure USB permission rules"
	echo "q. Quit this installer"
	echo ""
	read -rp "Enter your choice: " choice
	echo ""
	
	case "$choice" in
	
		0)
			echo "Full installation selected"		
			CheckRequirements $dist
			ret0=$?
			if [ $ret0 -ne 0 ]; then
				echo -e "${Red}Check requirements function exits with error $ret0. Exiting ...${Clear}"
				break    
			fi
			Installation
			ret1=$?
			if [ $ret1 -ne 0 ]; then
				echo -e "${Red}FERSlib and Janus installation exits with error $ret1. Exiting ...${Clear}"
				break
			fi
			CfgUSB
			ret2=$?
			if [ $ret2 -ne 0 ]; then
				echo -e "${Red}Configure USB permission rules exits with error $ret2. Exiting ...${Clear}"
				break
			fi			

			echo -e "${Green}Janus full installation completed succesfully${Clear}"
			break
			#iCheck=1
			#iInstall=1
			#iSetRule=1
			;;

		1)	
			echo "Requirements check selected"
			CheckRequirements $dist 
			ret0=$?
			if [ $ret0 -ne 0 ]; then
				echo -e "${Red}Check requirements function exits with error $ret0. Exiting ...${Clear}"
				break
			fi
			#iCheck=1
			;;
	
		2)
			echo "FERSlib + JANUS installation selected"
			Installation
			ret0=$?
			if [ $ret0 -ne 0 ]; then
				echo -e "${Red}FERSlib and Janus installation exits with error $ret0. Exiting ...${Clear}"
				break
			fi
			#iInstall=1
			;;

		3)
			echo "Configure USB permission rules selected"
			CfgUSB
			ret0=$?
			if [ $ret0 -ne 0 ]; then
				echo -e "${Red}Configure USB permission rules exits with error $ret0. Exiting ...${Clear}"
				break
			fi
			#iSetRule=1
			;;
		q|Q)
			echo "Exiting ..."
			break
			;;
		*)
			;;
	esac
done

# This check must be moved on Check_Requirements script
# gccV=`g++ -dumpversion`
# res=$?
# if [ $res -ne 0 ]; then
	# echo -e "${Red}ERROR: g++ is missing"
	# echo "Please, install g++ to proceed with Janus installer"
	# echo -e "Exiting ...${Clear}"
	# exit 1
# fi





#if [ $gccV -gt 13 ]; then
#	echo -e "${Yellow}You are using g++ V${gccV}"
#	echo "For any problems while running JanusC, please consider to compile Janus and FERSlib with a g++ version <= 13"
#	echo "by changing in Makefile.tmp"
#	echo "CXX = g++ ---> CXX = g++-VV"
#	if [ $vers == debian ] || [ $vers == fedoraRedhat ]; then
#		echo "and in installer_${dist}.bash"
#		echo "./configure --prefix=$libpath ---> ./configure --prefix=$libpath CXX=/pathToExe/g++-VV"
#	fi
#	echo -e "where VV is the g++ compiler version in use${Clear}"  
#	#echo "by running this installer with the arg: 'bash Janus_Install CXX=g++-VV', where VV is the g++ version you select"
#	#echo "CXX = g++ ---> CXX = g++-11"
#fi

#if [ -z "$vers" ]; then
#	vers="in-use"
#fi


	
	
#if [ $name != "Debian" ] && [ $name != "Fedora" ] && [ $name != "Ubuntu" ] && [ $name != "RedHat" ]; then
#	echo -e "${Yellow}Linux distribution ${name} is not supported by Janus Installer"
	# echo 
#	echo
#	show_help
#	# echo "Please install the following packages before compiling Janus:"
#	# echo " - libusb-1.0"
#	# echo " - pkg-config"
#	# echo " - gnuplot"
#	# echo " - python3"
#	# echo " - python3 tkinter"
#	# echo " - python3 pillow"
#	# echo " - python3 pillow tkinter"
#	# echo 
#	# echo "Compile Janus by running in shell the command:  make all"
#	echo 
#	echo -e "${Clear}"
#	
#	if [ $iCheck -eq 1 ]; then
#		chmod +x check_requirements.bash
#		./check_requirements.bash $dist
#	fi
#	
#	if [ $iSetRule -eq 1 ]; then
#		chmod +x setUSBrules.bash
#		./setUSBrules.bash
#	fi
#else
#	# Set installer name
#	# Cambiare nome allo script: installer_fedora.bash
#
#	if [ $iCheck -eq 1 ]; then
#		chmod +x check_requirements.bash
#		./check_requirements.bash $dist
#	fi
#	ret=$?
#
#	if [ $ret -eq 3 ]; then
#		echo -e "${Yellow}Gnuplot in Janus may not work properly without wxt terminal installed${Clear}"
#	elif [ $ret -ne 0 ];  then
#		echo -e "${Red}Required package missing. Cannot procede with Janus installation${Clear}"
#		echo -e "Exiting ..."
#		exit 10
#	fi
#	
#	if [ $iInstall -eq 1 ]; then
#		chmod +x installFERSlibJanus.bash
#		./installFERSlibJanus.bash
#	fi
#	
#	if [ $iSetRule -eq 1 ]; then
#		chmod +x setUSBrules.bash
#		./setUSBrules.bash
#	fi
#	#installer=installer_${dist}.bash
#	#chmod +x $installer
#	#./$installer
#fi

#installer.bash
#./installer.bash
