#!/bin/bash

# Reset
Clear='\033[0m'       # Text Reset

# Regular Colors
Black='\033[0;30m'        # Black
Red='\033[0;31m'          # Red
Orange='\033[48:2:255:165:0m' # Orange
Green='\033[0;32m'        # Green
Yellow='\033[0;33m'       # Yellow
Blue='\033[0;34m'         # Blue
Purple='\033[0;35m'       # Purple
Cyan='\033[0;36m'         # Cyan
White='\033[0;37m'        # White

###########################################
# ---- Installing packages functions ---- #
###########################################
function map_package_name() {
	package="$1"
	distro="$2"
	case "$package:$distro" in
		# ---- g++ ----
		g++:debian|g++:ubuntu) echo "g++" ;;
		g++:fedora|g++:centos|g++:rhel|g++:almalinux|g++:rocky) echo "gcc-c++" ;;

		# ---- make ----
		make:*) echo "make" ;;

		# ---- libusb ----
		libusb:debian|libusb:ubuntu) echo "libusb-1.0-0-dev" ;;
		libusb:fedora|libusb:centos|libusb:rhel|libusb:almalinux|libusb:rocky) echo "libusbx-devel" ;;

		# ---- usbutils ----
		usbutils:*) echo "usbutils" ;;

		# ---- gnuplot ----
		gnuplot:debian|gnuplot:ubuntu) echo "gnuplot" ;;
		gnuplot:fedora) echo "gnuplot-wx" ;;
		#gnuplot-wx:*) echo "UNAVAILABLE" ;;

		# ---- pkgconf ----
		pkgconf:*) echo "pkgconf" ;;

		# ---- python3 ----
		python3:*) echo "python3" ;;

		# ---- tkinter ----
		tkinter:debian|tkinter:ubuntu) echo "python3-tk" ;;
		tkinter:fedora|tkinter:centos|tkinter:rhel|tkinter:almalinux|tkinter:rocky) echo "python3-tkinter" ;;

		# ---- pillow ----
		pillow:debian|pillow:ubuntu) echo "python3-pil" ;; # ma si userà pip
		pillow:fedora) echo "python3-pillow" ;;
		pillow:centos|pillow:rhel|pillow:almalinux|pillow:rocky) echo "python3-pillow" ;;

		# ---- imagetk ----
		imagetk:debian|imagetk:ubuntu) echo "python3-pil.imagetk" ;;
		imagetk:fedora) echo "python3-pillow-tk" ;;
		imagetk:rhel|imagetk:centos|imagetk:almalinux|imagetk:rocky) echo "python3-pillow-tk" ;;

		*)
			echo "UNKNOWN"
			;;
	esac
	
}


function installPackage() {
	package="$1"
	distro="$2"

	realPkg=$(map_package_name "$package" "$distro")
	echo "Installing package: $realPkg"
	
	case "$distro" in 
		debian|ubuntu)
			sudo apt install -y "$realPkg"
			res=$?
			if [ $res -ne 0 ]; then
				echo -e "${Red}Cannot install $realPkg through 'sudo apt install $realPkg'"
				return -1
			else
				echo -e "${Green}$realPkg installed${Clear}"
			fi
			;;
		fedora)
			sudo dnf install -y "$realPkg"
			res=$?
			if [ $res -ne 0 ]; then
				echo -e "${Red}Cannot install $realPkg through 'sudo dnf install $realPkg'"
				return -1
			else
				echo -e "${Green}$realPkg installed${Clear}"
			fi
			;;
		rhel)
			sudo yum install -y "$realPkg"
			res=$?
			if [ $res -ne 0 ]; then
				echo -e "${Red}Cannot install $realPkg through 'sudo yum install $realPkg'"
				return -1
			else
				echo -e "${Green}$realPkg installed${Clear}"
			fi
			;;
		*)
			echo "Unsupported distro: $distro"
			return 1
			;;
	esac
}


function InstallWxt() {
	distro=$1
	echo -e "${Red}wxt terminal is not supported by your gnuplot version installed.{Clear}"
	echo "Please, try to install gnuplot from source:"
	echo -e "${Yellow} "
	echo " --- Required wxgt3, qt, lua dev library --- "
	echo " on debian: sudo apt install libwxgtk3.0-gtk3-dev qttools5-dev-tools qtbase5-dev lua5-3 liblua5.3-dev libwxvase3.0-dev libcairo2-dev libpango1.0-dev build-essential"
	echo " on ubuntu: sudo apt install libwxgtk3.2-dev qttools5-dev-tools qtbase5-dev lua5.3 liblua5.3-dev libwxvase3.0-dev(non esiste) libcairo2-dev libpango1.0-dev libqt5svg5-dev libqt5network5 libqt5printsupport5 build-essential"
	echo " on RedHat: sudo yum install gtk3-devel qt5-qtbase-devel lua-devel readline-devel cairo-devel pango-devel" 
	echo " on fedora: sudo dnf install wxGTK3-devel qt5-qtbase-devel lua-devel readline-devel "
	echo
	echo "Please check that all required libraries are installed before proceeding, to avoid errors during the gnuplot installation"
	echo " ----------------------------------- "
	
	
	
	# Install gnuplot requirements - to be checked for AlmaLinux and Fedora
	if [ $distro != "ubuntu" ] && [ $distro != "debian" ]; then
		echo " wget wget https://sourceforge.net/projects/gnuplot/files/gnuplot/5.4.10/gnuplot-5.4.10.tar.gz/download -O gnuplot-5.4.10.tar.gz"
		echo " tar -xvf gnuplot .tar.gz"
		echo " ./configure --with-wx"
		echo " make"
		echo " sudo make install"
		return 0
	fi
	echo ${Clear}
	
	mf=$(pwd)/gnuplot
	while true; do
		read -rp "Do you want to install gnuplot from source? [y][n]" instG
		case $instG in
			"n"|"N")
				echo "Skipping gnuplot installation from source..."
				return 0
				;;
			"y"|"Y")
				echo
				break
				;;
		esac
	done
	
	echo "Do you want to install gnuplot in folder $mf/local or in /usr/bin?" 
	echo "(Select local installation if you don't want to overwrite a possible gnuplot exe in /usr/bin folder)"
	echo "[1] $mf/local"
	echo "[2] /usr/bin (Sudo priviledges are needed)"
	while true; do 
		read -rp mselect
		case $mselect in
			1)
				echo "Installing gnuplot locally in $mf/local"
				break
				;;
			2)
				echo "Installing gnuplot in /usr/bin"
				echo "Sudo priviledges required"
				if sudo -v; then 
					echo -e "${Green}Sudo authentication completed${Clear}"
				else
					echo -e "${Red}Sudo authentication failed${Clear}"
				fi
				break
				;;
		esac
	done

	# Install gnuplot requirements - not yet implemented for Almalinux, RedHat, Centos
	
	# Installing gnuplot, already in this folder
	#echo "Installing gnuplot requirements ..."
	#sudo apt install libwxgtk3.0-gtk3-dev qttools5-dev-tools qtbase5-dev lua5-3 liblua5.3-dev libwxvase3.0-dev libcairo2-dev libpango1.0-dev build-essential
	
	echo "Installing gnuplot ..."
	nf=$(printf '%s\n' gnuplot-*.tar.gz | head -n1)
	nbase=${nf%.tar.gz}
	tar -xvf $nf
	mv $nbase gnuplot
	if [ $mselect -eq 1]; then
		./configure --with-wx --prefix=$mf/local
		res=$?
		if [ $res -ne 0 ]; then
			echo -e "${Red}gnuplot configure exit with error $res ${Clear}"
			return $res
		fi
		make 
		res=$?
		if [ $res -ne 0 ]; then
			echo -e "${Red}gnuplot make exit with error $res ${Clear}"
			return $res
		fi
		make install
		res=$?
		if [ $res -ne 0 ]; then
			echo -e "${Red}gnuplot make install exit with error $res ${Clear}"
			return $res
		fi
		echo -e "${Green} Gnuplot succesfully installed${Clear}"
		echo -e "${Yellow}Please, add the path $mf/local/bin to PATH variable in bashrc as:"
		echo -e '  export PATH="mf/local/bin:\$PATH"${Clear}'
		echo
		read -n -1 -s -r -p "Press any key to continue..."
		echo
		
	elif [ $mselect -eq 2 ]; then
		./configure --with-wx
		if [ $res -ne 0 ]; then
			echo -e "${Red}gnuplot configure exit with error $res ${Clear}"
			return $res
		fi
		make 
		res=$?
		if [ $res -ne 0 ]; then
			echo -e "${Red}gnuplot make exit with error $res ${Clear}"
			return $res
		fi
		sudo make install
		res=$?
		if [ $res -ne 0 ]; then
			echo -e "${Red}gnuplot make install exit with error $res ${Clear}"
			return $res
		fi
	fi
	return 0
}



##########################################
#  ---- Check Package Installation ----  #
##########################################
function checkTool() {
	if command -v "$1" >/dev/null 2>&1; then
		echo -e "${Green}OK ${Clear}"
		return 0
	else
		echo -e "${Red}MISSING ${Clear}"
		return 1
	fi
}

function checkTools() {
	mMissing=1
	for tool in "$@"; do
		if command -v "$tool" >/dev/null 2>&1; then
			mMissing=$((mMissing-1))
			echo -e "${Green}OK ${Clear}"
			return 0
		fi	
	done
	echo -e "${Red}MISSING ${Clear}"
	return 1
}


function check_python_module() {
	module="$1"
	python3 -c "import $module" >/dev/null 2>&1
	res=$?
	if [ $res -eq 0 ]; then
		echo -e "${Green}OK ${Clear}"
		return 0
	else
		echo -e "${Red}MISSING ${Clear}"
		return 1
	fi
}


#################################
#  ----  General Summary -----  #
#################################
function generalSummary() {
	echo " PACKAGE    |  STATUS"
	echo " ---------------------"
	
	#g++
	echo -n " g++        |  "
	checkTool g++
	
	#make
	echo -n " make       |  "
	checkTool make
	
	#Libusb
	echo -n " libusb     |  "
	# debian-like
	#usblib="/usr/lib/x86_64-linux-gnu/libusb-1.0.so"
	usblib="/usr/include/libusb-1.0/libusb.h"
	if [ -e $usblib ]; then
		echo -e "${Green}OK ${Clear}"
	else
		echo -e "${Red}MISSING ${Clear}"
	fi
	
	#USButils
	echo -n " usbutils   |  "
	if checkTool lsusb; then
		iLsusb=1
	else
		iMissing=$((iMissing+1))
	fi
	
	#Pkgconf
	echo -n " pkgconf    |  "
	checkTools pkgconf pkg-config
	
	#Gnuplot
	echo -n " gnuplot    |  "
	checkTool gnuplot
	
	#WithWXT
	echo -n " (with wxt) |  "
	availableTerms=$(gnuplot -e "set print '-'; print GPVAL_TERMINALS")
	if echo $availableTerms | grep -q "wxt"; then
		echo -e "${Green}OK ${Clear}"
	else
		echo -e "${Yellow}MISSING ${Clear}"
	fi
	
	#Python3
	echo -n " python3    |  "
	checkTool python3

	#tkinter
	echo -n " tkinter    |  "
	check_python_module	tkinter

	#Pillow
	echo -n " pillow     |  "
	check_python_module PIL	
	
	#TkImage
	echo -n " imagetk    |  "
	python3 -c "from PIL import ImageTk" 2>/dev/null
	res1=$?
	if [ $res1 -eq 0 ]; then
		echo -e "${Green}OK ${Clear}"
	else
		echo -e "${Red}MISSING ${Clear}"
	fi
}

###################################
###################################
# ---- MAIN ---- #
mDistro=$1
if [ -z $mDistro ]; then
	echo -e "${Yellow}Missing linux distribution as input!!"
	echo -e "Please, run the script with the linux distribution in use, i.e.: ./check_requirements debian"
	echo -e "It can be found in /etc/os-release, ID key${Clear}"
	exit 1
fi
if [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
	echo "Launch the script with the linux distribution in use, as:"
	echo "./check_requirements debian"
	exit 1
fi
 
distOk=1

# ---- Check if the script is run as root
isroot=`id -u`

echo -e "${Green}"
echo "######################################################################"
echo "#     #####  ######  #####  #   #     #####  #####  #####  #####     #"
echo "#     #      #    #  #      ##  #     #      #      #   #  #         #"
echo "#     #      # ## #  ####   # # #  -  ####   ####   # # #  #####     #"
echo "#     #      #    #  #      #  ##     #      #      #  #       #     #"
echo "#     #####  #    #  #####  #   #     #      #####  #   #  #####     #"
echo "######################################################################"
echo -e "${Clear}"

echo "CAEN FERSlib/Janus REQUIREMENTS CHECK:"

# ----- Package installed variables
iGpp=0
iMake=0
iLibusb=0
iLsusb=0
iPkgconf=0
iGnuplot=0
iWithwxt=0
iPython3=0
iTkinter=0
iPillow=0
iTkimage=0
iMissing=0

echo 
#echo -e " PACKAGE  ........  STATUS ${Clear}"
echo " PACKAGE    |  STATUS"
echo " ---------------------"
#echo " ---------------------------"

# ---- g++ ----
echo -n " g++        |  "
if checkTool g++; then
	iGpp=1
else
	iMissing=$((iMissing+1))
fi

# ---- make ---- 
echo -n " make       |  "
if checkTool make; then
	iMake=1
else
	iMissing=$((iMissing+1))
fi

# ---- Libusb ---- 
#echo -n " libusb   ........  "
echo -n " libusb     |  "
# debian-like
#usblib="/usr/lib/x86_64-linux-gnu/libusb-1.0.so"
usblib="/usr/include/libusb-1.0/libusb.h"
# Let's have a look on libusb.h
if [ -e $usblib ]; then
	echo -e "${Green}OK ${Clear}"
	iLibusb=1
else
	echo -e "${Red}MISSING ${Clear}"
	iMissing=$((iMissing+1))
fi

# ---- USButils ----
echo -n " usbutils   |  "
if checkTool lsusb; then
	iLsusb=1
else
	iMissing=$((iMissing+1))
fi

# ---- Pkgconf ---- 
#echo -n " pkgconf  ........  "
echo -n " pkgconf    |  "
if checkTools pkgconf pkg-config; then
	iPkgconf=1
else
	iMissing=$((iMissing+1))
fi

# ---- Gnuplot ---- 
#echo -n " gnuplot  ........  "
echo -n " gnuplot    |  "

if checkTool gnuplot; then
	iGnuplot=1
else
	iMissing=$((iMissing+1))
fi

# ---- WithWXT ---- 
#echo -n " (with wxt) ......  "
echo -n " (with wxt) |  "
if [ $iGnuplot -eq 1 ]; then
	availableTerms=$(gnuplot -e "set print '-'; print GPVAL_TERMINALS")
	if echo $availableTerms | grep -q "wxt"; then
		echo -e "${Green}OK ${Clear}"
		iWithwxt=1
	else
		echo -e "${Yellow}MISSING ${Clear}"
		iMissing=$((iMissing+1))
	fi
else
	echo -e "${Yellow}MISSING${Clear}"
fi

# ---- Python3 ---- 
#echo -n " python3  ........  "
echo -n " python3    |  "
if checkTool python3; then
	iPython3=1
else
	iMissing=$((iMissing+1))
fi

# ---- tkinter ---- 
#echo -n " tkinter  ........  "
echo -n " tkinter    |  "
if check_python_module tkinter; then
	iTkinter=1
else
	iMissing=$((iMissing+1))
fi

# ---- Pillow ---- 
#echo -n " pillow  .........  "
echo -n " pillow     |  "
if check_python_module PIL; then
	iPillow=1
else
	iMissing=$((iMissing+1))
fi

# ---- ImageTK ----
#echo -n " tkImage ........  "
echo -n " imagetk    |  "
python3 -c "from PIL import ImageTk" 2>/dev/null
res1=$?
if [ $res1 -eq 0 ]; then
	echo -e "${Green}OK ${Clear}"
	iTkimage=1
else
	echo -e "${Red}MISSING ${Clear}"
	iMissing=$((iMissing+1))
fi
	
echo


if [ $iMissing -gt 0 ]; then
	echo -e "${Red}$iMissing package(s) missing...${Clear}"
	# ---- It can be run alone. In that case, do not try to install
	# package for the unsopported linux distribution
	case "$mDistro" in
		ubuntu|Ubuntu|debian|Debian)
			mDistro="debian"
			;;
		fedora|Fedora)
			mDistro="fedora"
			;;
		cenots|Centos|rhel|RedHat|redhat|Almalinux|almalinux|rocky|Rocky)
			mDistro="rhel"
			;;
		*)
			distOk=0
			echo -e "${Yellow}$iMissing package(s) still missing" 
			echo -e "You may not be able to install/run FERSlib/Janus succesfully ${Clear}"
			exit -2
			;;
	esac
		
	
	echo "Do you want to install the missing package(s)?"
	echo "N.B: Sudo privileges are required"
	while true; do
		read -p "[y][n]:" choice

		case "$choice" in
			y|Y)
				if sudo -v; then
					echo "Sudo authentication succesfull"
					echo "Updating repository ..."
					case "$mDistro" in 
						debian|ubuntu)
							sudo apt update
							;;
						fedora)
							sudo dnf update
							;;
						rhel)
							sudo yum update
							;;
					esac
					echo "Installing missing package(s)"
					if [ $iGpp -eq 0 ]; then
						installPackage g++ $mDistro
						res=$?
						if [ $res -eq 0 ]; then iMissing=$((iMissing-1)) iGpp=1; fi
					fi
					if [ $iMake -eq 0 ]; then
						installPackage make $mDistro
						res=$?
						if [ $res -eq 0 ]; then iMissing=$((iMissing-1)) iMake=1; fi
					fi
					if [ $iLibusb -eq 0 ];  then 
						installPackage libusb $mDistro
						res=$?
						if [ $res -eq 0 ]; then iMissing=$((iMissing-1)) iLibusb=1; fi
					fi
					if [ $iLsusb -eq 0 ]; then
						installPackage usbutils $mDistro
						res=$?
						if [ $res -eq 0 ]; then iMissing=$((iMissing-1)); iLsusb=1; fi
					fi
					if [ $iPkgconf -eq 0 ]; then 
						installPackage pkgconf $mDistro					
						res=$?
						if [ $res -eq 0 ]; then iMissing=$((iMissing-1)); iPkgconf=1; fi
					fi
					if [ $iGnuplot -eq 0 ]; then 
						installPackage gnuplot $mDistro
						res=$?
						if [ $res -eq 0 ]; then iMissing=$((iMissing-1)); iGnuplot=1; fi
					fi
					if [ $iWithwxt -eq 0 ]; then
						installPackage gnuplot $mDistro
						availableTerms=$(gnuplot -e "set print '-'; print GPVAL_TERMINALS") 
						if echo $availableTerms | grep -q "wxt"; then
							iMissing=$((iMissing-1)); 
							iWithwxt=1
							echo -e "${Green} gnuplot support wxt terminal${Clear}"
						else
							if [ "$mDistro" == "fedora" ]; then
								echo -e "${Yellow}Fedora is using gnuplot-qt as default gnuplot launcher"
								echo -e "You need to uninstall gnuplot-qt and leave it only gnuplot-wx"
								echo -e "or compile gnuplot from source with wxt support, as explained below${Clear}"
							else
								InstallWxt $mDistro
							fi
						fi
					fi
					if [ $iPython3 -eq 0 ]; then 
						installPackage python3 $mDistro 
						res=$?
						if [ $res -eq 0 ]; then iMissing=$((iMissing-1)); iPython3=1; fi
					fi
					if [ $iTkinter -eq 0 ]; then 
						installPackage tkinter $mDistro
						res=$?
						if [ $res -eq 0 ]; then iMissing=$((iMissing-1)); iTkinter=1; fi
					fi
					if [ $iPillow -eq 0 ]; then 
						installPackage pillow $mDistro 
						res=$?
						if [ $res -eq 0 ]; then iMissing=$((iMissing-1)); iPillow=1; fi
					fi
					if [ $iTkimage -eq 0 ]; then 
						installPackage imagetk $mDistro
						res=$?
						if [ $res -eq 0 ]; then iMissing=$((iMissing-1)); iTkimage=1; fi
					fi
				else
					echo "Sudo authentication failed"
				fi
				break
				;;
			n|N)
				echo "Installation skipped"
				echo -e "${Yellow}WARNING: you may not be able to install/run FERSlib/Janus succesfully ${Clear}"
				exit -2
				break
				;;
		esac
	done
	generalSummary
fi

if [ $iMissing -gt 0 ] && [ $distOk -eq 1 ]; then 
	echo -e "${Yellow}$iMissing package(s) still missing" 
	echo -e "You may not be able to install/run FERSlib/Janus succesfully ${Clear}"
	if [ $iMissing -eq 1 ] && [ $iWithwxt -eq 0 ]; then
		exit 3
	else
		exit 2
	fi
else 
	echo -e "${Green}Check requirements completed${Clear}"
	exit 0
fi
